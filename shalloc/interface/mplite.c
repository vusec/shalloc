#include <shalloc/shalloc.h>
#include <mplite/mplite.h>

#define MPLITE_MIN_ALLOC    64

/* Mplite allocator interface. */
int _mplite_create(void *ref)
{
    shalloc_buff_t *buff = (shalloc_buff_t*) ref;
    mplite_t *handle = (mplite_t*) buff->start;
    int ret;
    if (buff->size <= sizeof(mplite_t)) {
        return -1;
    }
    ret = mplite_init(handle, handle+1, buff->size-sizeof(mplite_t),
        MPLITE_MIN_ALLOC, NULL);
    return ret == MPLITE_OK ? 0 : -1;
}

void* _mplite_malloc(void *ref, size_t size)
{
    shalloc_buff_t *buff = (shalloc_buff_t*) ref;
    mplite_t *handle = (mplite_t*) buff->start;
    return mplite_malloc(handle, size);
}

void _mplite_free(void *ref, void *ptr)
{
    shalloc_buff_t *buff = (shalloc_buff_t*) ref;
    mplite_t *handle = (mplite_t*) buff->start;
    mplite_free(handle, ptr);
}
