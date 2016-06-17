#ifndef SHALLOC_INTERFACE_H
#define SHALLOC_INTERFACE_H

/* Buffer allocator interface. */
int _buffer_create(void *ref);
void* _buffer_malloc(void *ref, size_t size);
void _buffer_free(void *ref, void *ptr);

/* Simple allocator interface. */
int _simple_create(void *ref);
void* _simple_malloc(void *ref, size_t size);
void _simple_free(void *ref, void *ptr);

/* Mplite allocator interface. */
int _mplite_create(void *ref);
void* _mplite_malloc(void *ref, size_t size);
void _mplite_free(void *ref, void *ptr);

/* No-free allocator interface. */
int _nofree_create(void *ref);
void _nofree_destroy(void *ref);
void* _nofree_malloc(void *ref, size_t size);
void _nofree_free(void *ref, void *ptr);

/* Slab allocator interface. */
int _slab_create(void *ref);
void _slab_destroy(void *ref);
void* _slab_malloc(void *ref, size_t size);
void _slab_free(void *ref, void *ptr);

#endif /* SHALLOC_INTERFACE_H */

