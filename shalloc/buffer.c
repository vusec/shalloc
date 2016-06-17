#include <shalloc/shalloc.h>
#include "include/util.h"
#include "include/interface.h"

/* Allocator function definitions. */
shalloc_buff_op_t shalloc_space_buff_ops[__NUM_SHALLOC_BUFF_ALLOC_TYPES] = {
    /* SHALLOC_BUFF_ALLOC_TYPE_NONE */
    { 0, 0, 0, 0, 0 },

    /* SHALLOC_BUFF_ALLOC_TYPE_BUFFER */
    {
        _buffer_create,
        gen_empty_destroy,
        _buffer_malloc,
        _buffer_free,
        gen_memset_calloc
    },

    /* SHALLOC_BUFF_ALLOC_TYPE_SIMPLE */
    {
        _simple_create,
        gen_empty_destroy,
        _simple_malloc,
        _simple_free,
        gen_memset_calloc
    },

    /* SHALLOC_BUFF_ALLOC_TYPE_MPLITE */
    {
        _mplite_create,
        gen_empty_destroy,
        _mplite_malloc,
        _mplite_free,
        gen_memset_calloc
    },

    /* SHALLOC_BUFF_ALLOC_TYPE_NOFREE */
    {
        _nofree_create,
        _nofree_destroy,
        _nofree_malloc,
        _nofree_free,
        gen_memset_calloc,
    },

    /* SHALLOC_BUFF_ALLOC_TYPE_SLAB */
    {
        _slab_create,
        _slab_destroy,
        _slab_malloc,
        _slab_free,
        gen_memset_calloc
    }
};

/* Utility functions. */
shalloc_buff_t* shalloc_get_buff(shalloc_buff_t *buff,
    void *start, size_t size, size_t block_size,
    enum shalloc_buff_alloc_type type,
    enum shalloc_buff_alloc_type default_type)
{
    if (start && size < SHALLOC_BUFF_MIN_SIZE) {
        size = SHALLOC_BUFF_MIN_SIZE;
    }
    memset(buff, 0, sizeof(shalloc_buff_t));
    buff->start = start;
    buff->end = (char*)start + size - 1;
    buff->size = size;
    buff->unused_size = size;
    buff->block_size = block_size;
    if (!type) {
        type = default_type;
    }
    assert(type < __NUM_SHALLOC_BUFF_ALLOC_TYPES);
    buff->alloc_type = type;
    if (type != SHALLOC_BUFF_ALLOC_TYPE_NONE && shalloc_space) {
        buff->op = &shalloc_space->buff_ops[type];
    }

    return buff;
}

shalloc_buff_t* shalloc_clone_buff(shalloc_buff_t *buff,
    void *start, size_t size, shalloc_buff_t *from_buff)
{
    shalloc_get_buff(buff, start, size, from_buff->block_size,
        from_buff->alloc_type, from_buff->alloc_type);

    return buff;
}

/* Shalloc allocator interface. */
void shalloc_buff_reset(shalloc_buff_t *buff)
{
    int ret;
    buff->op->destroy(buff);
    ret = buff->op->create(buff);
    assert(ret >= 0 && "Corrupted buffer?");
}
