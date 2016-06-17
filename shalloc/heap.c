#include <shalloc/shalloc.h>
#include "include/util.h"

/* Heap utility functions. */
shalloc_heap_t* shalloc_get_heap(shalloc_heap_t *heap, char *addr,
    size_t size, int mmap_flags, enum shalloc_buff_alloc_type type)
{
    size_t data_size, base_size;
    shalloc_buff_t *buff;

    /* Page align. */
    assert(size > 0);
    if (size % SHALLOC_PAGE_SIZE > 0) {
        size = (size/SHALLOC_PAGE_SIZE)*SHALLOC_PAGE_SIZE;
        if (size == 0) {
            size += SHALLOC_PAGE_SIZE;
        }
    }

    /* Compute size. */
    data_size = size;
#if SHALLOC_USE_GUARD_PAGES
    if (size > 100*SHALLOC_PAGE_SIZE) {
        data_size -= SHALLOC_PAGE_SIZE;
    }
    base_size = data_size + SHALLOC_PAGE_SIZE;
#else
    base_size = data_size;
#endif

    /* Fill in information. */
    buff = &heap->base;
    shalloc_get_buff(buff, addr, base_size, 0, 0, 0);
    shalloc_get_region(&heap->region, NULL, data_size, 0,
        SHALLOC_FLAG(HEAP)|SHALLOC_FLAG(NON_RESIZABLE),
        SHALLOC_HEAP_TYPE_DEFAULT);
    buff = &heap->region.default_data;
    heap->region.data_head = heap->region.data_tail = buff;
    shalloc_get_buff(buff, addr, data_size, 0,
        type, SHALLOC_HEAP_TYPE_DEFAULT);
    heap->mmap_flags = mmap_flags;

    return heap;
}

int shalloc_map_heap(shalloc_heap_t *heap)
{
    shalloc_buff_t *data = shalloc_heap_to_buff(heap);
    int remapped = 0, inherit_mem_id;
    char *inherit_mem_ptr;

    if (heap->mmap_flags & SHALLOC_MAP_INHERIT) {
        if (heap->inherit_id == -1) {
            inherit_mem_id = shmget(IPC_PRIVATE, data->size, SHM_R | SHM_W);
            assert(inherit_mem_id != -1);
            inherit_mem_ptr = shmat(inherit_mem_id, data->start, SHM_REMAP);
            assert(inherit_mem_ptr != MAP_FAILED);
            heap->inherit_id = inherit_mem_id;

            // mark for delete
            shmctl(inherit_mem_id, IPC_RMID, NULL);
        } else {
            inherit_mem_ptr = shmat(heap->inherit_id, data->start, SHM_REMAP);
            assert(inherit_mem_ptr != MAP_FAILED);
        }
        remapped = 1;

    } else if (heap->mmap_flags != SHALLOC_DEFAULT_MMAP_FLAGS) {
        /* Remap the heap with the right flags if necessary. */
        shalloc_map_fixed_pages(data->start, data->size,
            PROT_READ|PROT_WRITE, heap->mmap_flags);
        remapped = 1;
    }

    if (heap->base.size > data->size) {
        /* Add guard page. */
        shalloc_map_fixed_pages((char*)data->start+data->size,
            SHALLOC_PAGE_SIZE, PROT_NONE, MAP_ANONYMOUS|MAP_PRIVATE);
        remapped = 1;
    }

    return remapped;
}

void shalloc_heap_destroy(shalloc_heap_t* heap)
{
    shalloc_buff_t *data;
    shalloc_magic_t *magic_info;

    data = shalloc_heap_to_buff(heap);

    if (heap->mmap_flags & SHALLOC_MAP_INHERIT) {
        if (heap->inherit_id != -1) {
            shmdt(data->start);
            shmctl(heap->inherit_id, IPC_RMID, NULL);
            heap->inherit_id = -1;
        }
        magic_info = (shalloc_magic_t *) shalloc_space->magic_control_page;
        magic_info->num_heaps--;
    } else {
        shalloc_free(shalloc_heap_to_region(shalloc_space->priv_heap), heap);
    }

    munmap(heap->base.start, heap->base.size);
}

/* Shalloc heap allocator interface. */
shalloc_heap_t* shalloc_heap_create(size_t size, int mmap_flags,
    enum shalloc_buff_alloc_type type)
{
    shalloc_heap_t *heap;
    shalloc_buff_t *data;
    shalloc_region_t *priv_region;
    shalloc_magic_t *magic_info;
    int remapped;
    char *addr;

    /* Allocate heap object. */
    assert(shalloc_space && "shalloc_space not initialized");
    if (mmap_flags & SHALLOC_MAP_INHERIT) {
        magic_info = (shalloc_magic_t *) shalloc_space->magic_control_page;
        assert(magic_info->num_heaps < SHALLOC_MAX_INHERIT_HEAPS && "Out of space for inherit heaps");
        heap = &magic_info->heap_list[magic_info->num_heaps];
        heap->inherit_id = -1;
        magic_info->num_heaps++;
    } else {
        priv_region = shalloc_heap_to_region(shalloc_space->priv_heap);
        heap = shalloc_malloc(priv_region, sizeof(shalloc_heap_t));
        assert(heap && "Out of private heap memory!");
    }

    /* Figure out if we have enough space for the new heap. */
    addr = (char*)shalloc_space->data.start + shalloc_space->data.size
        - shalloc_space->data.unused_size;
    assert(addr <= (char*)shalloc_space->data.end);
    shalloc_get_heap(heap, addr, size, mmap_flags, type);
    if (addr + heap->base.size > (char*)shalloc_space->data.end+1) {
        if (mmap_flags & SHALLOC_MAP_INHERIT) {
            magic_info = (shalloc_magic_t *) shalloc_space->magic_control_page;
            magic_info->num_heaps--;
        } else {
            shalloc_free(priv_region, heap);
        }
        return NULL;
    }

    /* Map the new heap. */
    remapped = shalloc_map_heap(heap);

    /* Create data buffer. */
    data = shalloc_heap_to_buff(heap);
    if (data->op->create(data) < 0) {
        if (remapped) {
            /* Detach if shared */
            if (mmap_flags & SHALLOC_MAP_INHERIT) {
                shmdt(data->start);
                shmctl(heap->inherit_id, IPC_RMID, NULL);
                heap->inherit_id = -1;
            }
            shalloc_map_fixed_pages(data->start, data->size,
                PROT_READ|PROT_WRITE, SHALLOC_DEFAULT_MMAP_FLAGS);
        }
        if (mmap_flags & SHALLOC_MAP_INHERIT) {
            magic_info = (shalloc_magic_t *) shalloc_space->magic_control_page;
            magic_info->num_heaps--;
        } else {
            shalloc_free(priv_region, heap);
        }
        return NULL;
    }

    /* Update shadow space. */
    shalloc_space_new_heap(heap);

    return heap;
}

shalloc_region_t* shalloc_heap_to_region(shalloc_heap_t* heap)
{
    return heap ? &heap->region : NULL;
}

shalloc_buff_t* shalloc_heap_to_buff(shalloc_heap_t* heap)
{
    shalloc_buff_t *data;
    if (!heap) {
        return NULL;
    }
    data = shalloc_heap_to_region(heap)->data_tail;
    assert(data);
    return data;
}

shalloc_heap_t* shalloc_region_to_heap(shalloc_region_t* region)
{
    if (!region || !(region->flags & SHALLOC_FLAG(HEAP))) {
        return NULL;
    }
    return SHALLOC_CONTAINER_OF(region, shalloc_heap_t, region);
}

