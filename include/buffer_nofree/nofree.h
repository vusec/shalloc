#ifndef BUFFER_NOFREE_H
#define BUFFER_NOFREE_H

#include <string.h>
#include <stdlib.h>
#include <assert.h>

/* Simple buffer-based allocator implementation that never frees memory. */
typedef struct {
    int magic_start;
    char *next;
    int magic_end;
} nofree_header_t;

int nofree_init(void *buff, size_t buff_size);
void nofree_close(void *buff, size_t buff_size);
void* nofree_alloc(void *buff, size_t buff_size, size_t size,
    size_t align);

#endif /* BUFFER_NOFREE_H */

