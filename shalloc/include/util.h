#ifndef SHALLOC_UTIL_H
#define SHALLOC_UTIL_H

/* Utility functions. */
void shalloc_space_new_heap(shalloc_heap_t *heap);
void shalloc_map_fixed_pages(void *addr, size_t size, int prot,
    int mmap_flags);

shalloc_buff_t* shalloc_get_buff(shalloc_buff_t *buff,
    void *start, size_t size, size_t block_size,
    enum shalloc_buff_alloc_type type,
    enum shalloc_buff_alloc_type default_type);
shalloc_buff_t* shalloc_clone_buff(shalloc_buff_t *buff,
    void *start, size_t size, shalloc_buff_t *from_buff);

shalloc_heap_t* shalloc_get_heap(shalloc_heap_t *heap, char *addr,
    size_t size, int mmap_flags, enum shalloc_buff_alloc_type type);
int shalloc_map_heap(shalloc_heap_t *heap);
void shalloc_heap_destroy(shalloc_heap_t* heap);

shalloc_region_t* shalloc_get_region(shalloc_region_t *region,
    shalloc_region_t *parent, size_t buff_size, size_t block_size, int flags,
    enum shalloc_buff_alloc_type type);

#endif /* SHALLOC_UTIL_H */
