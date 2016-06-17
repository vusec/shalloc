#include <shalloc/shalloc.h>
#include <buffer_nofree/nofree.h>

/* No-free buffer-based allocator interface. */
int _nofree_create(void *ref)
{
    shalloc_buff_t *buff = (shalloc_buff_t*) ref;
    return nofree_init(buff->start, buff->size);
}

void _nofree_destroy(void *ref)
{
    shalloc_buff_t *buff = (shalloc_buff_t*) ref;
    nofree_close(buff->start, buff->size);
}

void* _nofree_malloc(void *ref, size_t size)
{
    shalloc_buff_t *buff = (shalloc_buff_t*) ref;
    return nofree_alloc(buff->start, buff->size, size, sizeof(long));
}

void _nofree_free(void *ref, void *ptr)
{
}

