#include <shalloc/shalloc.h>
#include "include/util.h"

/* Region utility functions. */
static shalloc_buff_t* shalloc_region_alloc_buff(shalloc_region_t *region,
    size_t size)
{
    shalloc_buff_t *data;
    size_t default_data_size = region->default_data.size;
    if (region->flags & SHALLOC_FLAG(ELASTIC)) {
        default_data_size *= 2;
    }
    if (default_data_size > size) {
        size = default_data_size;
    }

    assert(region->parent);
    data = (shalloc_buff_t*) shalloc_malloc(region->parent,
        sizeof(shalloc_buff_t) + size);
    if (!data) {
        return NULL;
    }
    shalloc_clone_buff(data, data+1, size, &region->default_data);
    if (data->op->create(data) < 0) {
        shalloc_free(region->parent, data);
        return NULL;
    }
    region->default_data.size = default_data_size;

    return data;
}

static void shalloc_region_free_buff(shalloc_region_t *region,
    shalloc_buff_t *data)
{
    data->op->destroy(data);
    if (region->parent) {
        shalloc_free(region->parent, data);
    }
}

static int shalloc_region_grow(shalloc_region_t *region, size_t size)
{
    shalloc_buff_t *buff;
    assert(size > 0);
    if (region->data_tail && (region->flags & SHALLOC_FLAG(NON_RESIZABLE))) {
        return -1;
    }
    buff = shalloc_region_alloc_buff(region, size);
    if (!buff) {
        return -1;
    }
    if (!region->data_tail) {
        assert(!region->data_head);
        region->data_head = region->data_tail = buff;
    }
    else {
        region->data_tail->next = buff;
        region->data_tail = buff;
    }
    buff->next = NULL;

    return 0;
}

shalloc_region_t* shalloc_get_region(shalloc_region_t *region,
    shalloc_region_t *parent, size_t buff_size, size_t block_size, int flags,
    enum shalloc_buff_alloc_type type)
{
    shalloc_buff_t *data;

    /* Fill in information. */
    if (buff_size == 0) {
        buff_size = SHALLOC_REGION_BUFF_SIZE_DEFAULT;
    }
    data = &region->default_data;
    shalloc_get_buff(data, (void*)-1, buff_size, block_size,
        type, SHALLOC_REGION_TYPE_DEFAULT);
    region->data_head = NULL;
    region->data_tail = NULL;
    region->parent = parent;
    region->flags = flags;

    return region;
}

/* Shalloc region allocator interface. */
shalloc_region_t* shalloc_region_create(shalloc_region_t *parent,
    size_t init_size, size_t buff_size, size_t block_size, int flags,
    enum shalloc_buff_alloc_type type)
{
    shalloc_region_t *region;

    /* Allocate region object. */
    assert(parent && "Bad parent!");
    if (flags & SHALLOC_FLAG(HEAP)) {
        return NULL;
    }
    region = shalloc_malloc(parent, sizeof(shalloc_region_t));
    if (!region) {
        return NULL;
    }
    shalloc_get_region(region, parent, buff_size, block_size, flags, type);
    if (init_size > 0) {
        if (shalloc_region_grow(region, init_size) < 0) {
            shalloc_region_destroy(region);
            return NULL;
        }
    }

    return region;
}

void shalloc_region_destroy(shalloc_region_t* region)
{
    shalloc_region_reset(region);
    if (region->parent) {
        shalloc_free(region->parent, region);
    }
    if (region->flags & SHALLOC_FLAG(HEAP)) {
        shalloc_heap_destroy(shalloc_region_to_heap(region));
        return;
    }
}

void shalloc_region_reset(shalloc_region_t* region)
{
    shalloc_buff_t *prev, *curr;

    SHALLOC_REGION_BUFF_ITER(region, prev, curr,
        if (prev && prev != region->data_head) {
            shalloc_region_free_buff(region, prev);
        }
    );
    if (region->data_head) {
        shalloc_buff_reset(region->data_head);
        region->data_head->next = NULL;
        region->data_tail = region->data_head;
    }
}

void shalloc_region_get_info(shalloc_region_t* region,
    shalloc_region_info_t *info)
{
    shalloc_buff_t *prev, *curr;

    memset(info, 0, sizeof(shalloc_region_info_t));
    SHALLOC_REGION_BUFF_ITER(region, prev, curr,
        info->tot_buff_size += curr->size;
        info->num_buffs++;
    );
    if (curr) {
        info->last_buff_size = curr->size;
    }
}

void* shalloc_malloc(shalloc_region_t *region, size_t size)
{
    void *ptr;
    shalloc_buff_t *data_tail = region->data_tail;
    if (!size) {
        return NULL;
    }
    ptr = NULL;
    if (data_tail) {
        ptr = data_tail->op->malloc(data_tail, size);
    }
    if (!ptr) {
        if (shalloc_region_grow(region, SHALLOC_BUFF_ALLOC_SIZE(size)) == 0) {
            data_tail = region->data_tail;
            ptr = data_tail->op->malloc(data_tail, size);
        }
    }
    return ptr;
}

void shalloc_free(shalloc_region_t *region, void *ptr)
{
    shalloc_buff_t *data_tail = region->data_tail;
    if (!ptr) {
        return;
    }

    /* We only support deallocation in the last data buffer. */
    if (data_tail && SHALLOC_IS_BUFF_ADDR(data_tail, ptr)) {
        data_tail->op->free(data_tail, ptr);
    }
}

void* shalloc_calloc(shalloc_region_t *region, size_t nmemb, size_t size)
{
    void *ptr;
    shalloc_buff_t *data_tail = region->data_tail;
    size_t tot_size = nmemb*size;
    if (tot_size == 0) {
        return NULL;
    }
    ptr = NULL;
    if (data_tail) {
        ptr = data_tail->op->calloc(data_tail, nmemb, size);
    }
    if (!ptr) {
        if (shalloc_region_grow(region,
            SHALLOC_BUFF_ALLOC_SIZE(tot_size)) == 0) {
            data_tail = region->data_tail;
            ptr = data_tail->op->calloc(data_tail, nmemb, size);
        }
    }
    return ptr;
}

void* shalloc_orealloc(shalloc_region_t *region, void *ptr, size_t size,
    size_t old_size)
{
    void *new_ptr;

    if (ptr == NULL) {
        return shalloc_malloc(region, size);
    }
    if (size == 0) {
        shalloc_free(region, ptr);
        return NULL;
    }
    if (size <= old_size) {
        return ptr;
    }

    new_ptr = shalloc_malloc(region, size);
    if (!new_ptr) {
        return NULL;
    }
    memcpy(new_ptr, ptr, old_size);
    shalloc_free(region, ptr);
    return new_ptr;
}

