#include <shalloc/shalloc.h>
#include "include/util.h"

shalloc_space_t *shalloc_space = NULL;

/* Generic allocator functions. */
void* gen_memset_calloc(void *ref, size_t nmemb, size_t size)
{
    shalloc_buff_t *data = (shalloc_buff_t*) ref;
    void *ptr = data->op->malloc(data, size*nmemb);
    if (ptr) {
        memset(ptr, 0, size*nmemb);
    }
    return ptr;
}

void gen_empty_destroy(void *ref)
{
}

/* Utility functions. */
void shalloc_map_fixed_pages(void *addr, size_t size, int prot,
    int mmap_flags)
{
    void *real_addr = mmap(addr, size, prot, mmap_flags|MAP_FIXED, -1, 0);
    assert(addr == real_addr);
}

void shalloc_space_new_heap(shalloc_heap_t *heap)
{
    if (heap->mmap_flags & SHALLOC_MAP_INHERIT) {
        assert(!shalloc_space->has_noninherit_heaps && "Inherit heaps should always be created first!");
    }
    else {
        shalloc_space->has_noninherit_heaps = 1;
    }

    assert(shalloc_space->data.unused_size >= heap->base.size);
    shalloc_space->data.unused_size -= heap->base.size;
    shalloc_space->num_heaps++;
}

/* Printing functions. */
void shalloc_space_print()
{
    shalloc_printf("SPACE:{");
    shalloc_printf(" .base=");
    shalloc_buff_print(&shalloc_space->base);
    shalloc_printf(" .data=");
    shalloc_buff_print(&shalloc_space->data);
    shalloc_printf(" .num_heaps=%d", shalloc_space->num_heaps);
    shalloc_printf(" .mmap_flags=0x%08x", shalloc_space->mmap_flags);
    shalloc_printf(" .priv_heap=");
    shalloc_heap_print(shalloc_space->priv_heap);
    shalloc_printf(" }");
}

void shalloc_buff_print(shalloc_buff_t *buff)
{
    shalloc_printf("BUFF:{");
    shalloc_printf(" .start=%p", (void*) buff->start);
    shalloc_printf(" .end=%p", (void*) buff->end);
    shalloc_printf(" .size=%zu", buff->size);
    shalloc_printf(" .unused_size=%zu", buff->unused_size);
    shalloc_printf(" .next=%p", (void*) buff->next);
    shalloc_printf(" }");
}

void shalloc_heap_print(shalloc_heap_t *heap)
{
    shalloc_printf("HEAP:{");
    shalloc_printf(" .base=");
    shalloc_buff_print(&heap->base);
    shalloc_printf(" .region=");
    shalloc_region_print(&heap->region);
    shalloc_printf(" .mmap_flags=0x%08x", heap->mmap_flags);
    shalloc_printf(" }");
}

void shalloc_region_print(shalloc_region_t *region)
{
    shalloc_printf("REGION:{");
    shalloc_printf(" .data_head=%p", (void*) region->data_head);
    shalloc_printf(" .data_tail=%p", (void*) region->data_tail);
    shalloc_printf(" .default_data=");
    shalloc_buff_print(&region->default_data);
    shalloc_printf(" .flags=0x%08x", region->flags);
    shalloc_printf(" .parent=%p", (void*) region->parent);
    shalloc_printf(" }");
}

/* Shalloc allocator interface. */
void shalloc_space_init()
{
    char *base_addr, *addr;
    int ret, i;
    shalloc_heap_t heap, *priv_heap;
    shalloc_buff_t *buff;
    shalloc_region_t *priv_region;
    shalloc_magic_t *magic_info;
    int inherit_mem_id;
    char *inherit_mem_ptr;

    if (shalloc_space) {
        return;
    }

    /* Check configuration. */
    assert(SHALLOC_BASE_SIZE >= 3*SHALLOC_PRIVATE_HEAP_SIZE);
    assert(SHALLOC_DEFAULT_MMAP_FLAGS != 0);

    /* Map (and reserve) shadow space memory. */
    base_addr=mmap((void*)SHALLOC_BASE_ADDR, SHALLOC_BASE_SIZE,
        PROT_READ|PROT_WRITE, SHALLOC_DEFAULT_MMAP_FLAGS|MAP_FIXED, -1, 0);
    assert(base_addr != MAP_FAILED);
    addr = base_addr;

#if SHALLOC_USE_GUARD_PAGES
    /* Map first guard page. */
    shalloc_map_fixed_pages(addr, SHALLOC_PAGE_SIZE, PROT_NONE,
        MAP_ANONYMOUS|MAP_PRIVATE);
    addr += SHALLOC_PAGE_SIZE;
#endif

    /* Create private heap for our own data structures. */
    shalloc_get_heap(&heap, addr, SHALLOC_PRIVATE_HEAP_SIZE,
        MAP_ANONYMOUS|MAP_PRIVATE, 0);
    shalloc_map_heap(&heap);
    buff = shalloc_heap_to_buff(&heap);
    buff->op = &shalloc_space_buff_ops[SHALLOC_HEAP_TYPE_DEFAULT];
    ret = buff->op->create(buff);
    assert(ret == 0);
    priv_region = shalloc_heap_to_region(&heap);
    priv_heap = shalloc_malloc(priv_region, sizeof(shalloc_heap_t));
    assert(priv_heap);
    memcpy(priv_heap, &heap, sizeof(shalloc_heap_t));
    priv_region = shalloc_heap_to_region(priv_heap);
    priv_region->data_head = priv_region->data_tail =
        &priv_region->default_data;
    addr += priv_heap->base.size;

    /* Create inherited magic pages. */
    if (getenv(SHALLOC_INHERIT_ID) == NULL) {
        inherit_mem_id = shmget(IPC_PRIVATE, 2*SHALLOC_PAGE_SIZE, SHM_R | SHM_W);
        assert(inherit_mem_id != -1);
        inherit_mem_ptr = shmat(inherit_mem_id, addr, SHM_REMAP);
        assert(inherit_mem_ptr != MAP_FAILED);

        // mark for delete
        shmctl(inherit_mem_id, IPC_RMID, NULL);

        magic_info = (shalloc_magic_t *) inherit_mem_ptr;
        magic_info->inherit_id = inherit_mem_id;
        sprintf(magic_info->inherit_envp_buff, "%s=%d",
            SHALLOC_INHERIT_ID, inherit_mem_id);
        magic_info->inherit_envp[0] = magic_info->inherit_envp_buff;
        magic_info->inherit_envp[1] = NULL;
        magic_info->num_heaps = 0;
    } else {
        inherit_mem_ptr = shmat(atoi(getenv(SHALLOC_INHERIT_ID)), addr, SHM_REMAP); //TODO:
        assert(inherit_mem_ptr != MAP_FAILED);
        unsetenv(SHALLOC_INHERIT_ID);

        magic_info = (shalloc_magic_t *) inherit_mem_ptr;
        for (i = 0; i < magic_info->num_heaps; i++) {
            shalloc_heap_t inherit_heap = magic_info->heap_list[i];
            shalloc_buff_t *data = shalloc_heap_to_buff(&inherit_heap);
            inherit_mem_ptr = shmat(inherit_heap.inherit_id, data->start, SHM_REMAP);
            assert(inherit_mem_ptr != MAP_FAILED);
        }
    }

    /* Initialize shalloc space. */
    shalloc_space = shalloc_malloc(priv_region, sizeof(shalloc_space_t));
    assert(shalloc_space);
    buff = &shalloc_space->base;
    shalloc_get_buff(buff, base_addr, SHALLOC_BASE_SIZE, 0, 0, 0);

    shalloc_space->magic_control_page = addr;
    shalloc_space->magic_inherit_page = addr + SHALLOC_PAGE_SIZE;
    addr += 2*SHALLOC_PAGE_SIZE;

    buff = &shalloc_space->data;
    shalloc_get_buff(buff, addr,
        (char*)shalloc_space->base.end - (char*)addr + 1, 0, 0, 0);
    shalloc_space->mmap_flags = SHALLOC_DEFAULT_MMAP_FLAGS;
    shalloc_space->num_heaps = 0;
    shalloc_space->has_noninherit_heaps = 0;
    shalloc_space->priv_heap = priv_heap;
    memcpy(shalloc_space->buff_ops, shalloc_space_buff_ops,
        sizeof(shalloc_buff_op_t)*__NUM_SHALLOC_BUFF_ALLOC_TYPES);

    /* Fix up our own private heap alloc operations. */
    buff = shalloc_heap_to_buff(priv_heap);
    buff->op = &shalloc_space->buff_ops[SHALLOC_HEAP_TYPE_DEFAULT];

    /* Update shalloc space size information when inheriting heaps. */
    for (i = 0; i < magic_info->num_heaps; i++) {
        shalloc_heap_t *inherit_heap = &magic_info->heap_list[i];
        shalloc_space_new_heap(inherit_heap);
    }
}

void shalloc_space_close()
{
    int i, inherit_id;
    shalloc_magic_t *magic_info;

    if (!shalloc_space) {
        return;
    }

    magic_info = (shalloc_magic_t *)shalloc_space->magic_control_page;
    for (i = 0; i < magic_info->num_heaps; i++) {
        shalloc_heap_t inherit_heap = magic_info->heap_list[i];
        shalloc_buff_t *data = shalloc_heap_to_buff(&inherit_heap);
        shmdt(data->start);
        shmctl(inherit_heap.inherit_id, IPC_RMID, NULL);
    }
    inherit_id = magic_info->inherit_id;
    shmdt(shalloc_space->magic_control_page);
    shmctl(inherit_id, IPC_RMID, NULL);

    munmap(shalloc_space->base.start, shalloc_space->base.size);
    shalloc_space = NULL;
}

void shalloc_space_freeze()
{
    /* Freeze shalloc space. No more heaps can be create/modified from now on.*/
    int ret = mprotect(shalloc_space->priv_heap->base.start,
        shalloc_space->priv_heap->base.size, PROT_READ);
    assert(ret == 0);
}

