#ifndef BUFFER_SLAB_H
#define BUFFER_SLAB_H

#include <string.h>
#include <stdlib.h>
#include <assert.h>

/* Simple buffer-based slab allocator.
 * Can only allocate/deallocate blocks of a predetermined size. 
 */
typedef struct {
    int magic_start;
    size_t block_size;
    size_t num_blocks;
    char *bitmap;
    char *blocks;
    unsigned next_block;
    int magic_end;
} slab_header_t;

int slab_init(void *buff, size_t buff_size, size_t block_size);
void slab_close(void *buff);
void* slab_alloc(void *buff, size_t size);
void slab_free(void *buff, void *ptr);

#endif /* BUFFER_SLAB_H */

