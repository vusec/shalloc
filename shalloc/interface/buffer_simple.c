#include <shalloc/shalloc.h>
#include <buffer_simple/memmgr.h>

/* Simple allocator interface. */
int _simple_create(void *ref)
{
    shalloc_buff_t *buff = (shalloc_buff_t*) ref;
    memmgr_init(buff->start, buff->size);
    return 0;
}

void* _simple_malloc(void *ref, size_t size)
{
    shalloc_buff_t *buff = (shalloc_buff_t*) ref;
    return memmgr_alloc(buff->start, buff->size, size);
}

void _simple_free(void *ref, void *ptr)
{
    shalloc_buff_t *buff = (shalloc_buff_t*) ref;
    memmgr_free(buff->start, buff->size, ptr);
}
