#include <shalloc/shalloc.h>
#include <buffer_slab/slab.h>

/* Buffer slab allocator interface. */
int _slab_create(void *ref)
{
    shalloc_buff_t *buff = (shalloc_buff_t*) ref;
    slab_init(buff->start, buff->size, buff->block_size);
    return 0;
}

void _slab_destroy(void *ref)
{
    shalloc_buff_t *buff = (shalloc_buff_t*) ref;
    slab_close(buff->start);
}

void* _slab_malloc(void *ref, size_t size)
{
    shalloc_buff_t *buff = (shalloc_buff_t*) ref;
    return slab_alloc(buff->start, size);
}

void _slab_free(void *ref, void *ptr)
{
    shalloc_buff_t *buff = (shalloc_buff_t*) ref;
    slab_free(buff->start, ptr);
}
