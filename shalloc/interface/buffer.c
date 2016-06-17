#include <shalloc/shalloc.h>
#include <buffer/alloc.h>

/* Buffer allocator interface. */
int _buffer_create(void *ref)
{
    shalloc_buff_t *buff = (shalloc_buff_t*) ref;
    alloc_init(buff->start, buff->size);
    return 0;
}

void* _buffer_malloc(void *ref, size_t size)
{
    shalloc_buff_t *buff = (shalloc_buff_t*) ref;
    return alloc_get(buff->start, buff->size, size, sizeof(long));
}

void _buffer_free(void *ref, void *ptr)
{
    shalloc_buff_t *buff = (shalloc_buff_t*) ref;
    alloc_free(buff->start, buff->size, ptr);
}

