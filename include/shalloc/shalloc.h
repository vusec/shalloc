#ifndef SHALLOC_H
#define SHALLOC_H

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/mman.h>
#include <sys/ipc.h>
#include <sys/shm.h>

/*
 * Remove the definition of mmap_flags from magic_structs.h
 */
#ifdef _MAGIC_STRUCTS_H
#undef mmap_flags
#endif
/*
 * A shadow allocator which supports a number of independent heaps in a
 * "shadow" space allocated in a fixed (and reserved) address space region.
 * Relies on independent buffer-based allocators to manage individual heaps
 * and regions within heaps.
 */

/* General definitions. */
#define SHALLOC_PAGE_SIZE                   ((size_t) 0x1000)

#ifndef SHALLOC_PRIVATE_HEAP_SIZE
#  define SHALLOC_PRIVATE_HEAP_SIZE         (SHALLOC_PAGE_SIZE)
#endif

#ifndef SHALLOC_DEFAULT_MMAP_FLAGS
#  define SHALLOC_DEFAULT_MMAP_FLAGS        (MAP_ANONYMOUS|MAP_PRIVATE)
#endif

#ifndef SHALLOC_BASE_ADDR
#  ifdef __MINIX
#    define SHALLOC_BASE_ADDR               ((unsigned) 0xA0000000)
#    define SHALLOC_LAST_ADDR               ((unsigned) 0xBFFFFFFF)
#  else
#    define SHALLOC_BASE_ADDR               ((unsigned) 0x80000000)
#    define SHALLOC_LAST_ADDR               ((unsigned) 0xBFFFFFFF)
#  endif
#endif

#ifndef SHALLOC_USE_GUARD_PAGES
#  ifdef __MINIX
#    define SHALLOC_USE_GUARD_PAGES         0
#  else
#    define SHALLOC_USE_GUARD_PAGES         1
#  endif
#endif

#define SHALLOC_BASE_SIZE \
    ((size_t)(SHALLOC_LAST_ADDR - SHALLOC_BASE_ADDR + 1))
#define SHALLOC_SPACE_SIZE (shalloc_space->data.size)

#define SHALLOC_OFFSET_OF(T,M) ((size_t)(&((T *)0)->M))
#define SHALLOC_CONTAINER_OF(ptr, type, member) \
    (type *)( (char *)(ptr) - SHALLOC_OFFSET_OF(type,member) )

/* Shalloc flags */
enum shalloc_flags {
    HEAP = 0,
    NON_RESIZABLE,
    ELASTIC,
    __NUM_SHALLOC_FLAGS
};
#define SHALLOC_FLAG(F) (1 << (F))

/* Allocator functions. */
typedef int   (*shalloc_create_t)(void* ref);
typedef void  (*shalloc_destroy_t)(void* ref);
typedef void* (*shalloc_malloc_t)(void* ref, size_t size);
typedef void  (*shalloc_free_t)(void* ref, void *ptr);
typedef void* (*shalloc_calloc_t)(void* ref, size_t nmemb,
    size_t size);

void* gen_memset_calloc(void *ref, size_t nmemb, size_t size);
void gen_empty_destroy(void *ref);

/* Shadow buffer definitions. */
enum shalloc_buff_alloc_type {
    SHALLOC_BUFF_ALLOC_TYPE_NONE = 0,
    SHALLOC_BUFF_ALLOC_TYPE_BUFFER,
    SHALLOC_BUFF_ALLOC_TYPE_SIMPLE,
    SHALLOC_BUFF_ALLOC_TYPE_MPLITE,
    SHALLOC_BUFF_ALLOC_TYPE_NOFREE,
    SHALLOC_BUFF_ALLOC_TYPE_SLAB,
    __NUM_SHALLOC_BUFF_ALLOC_TYPES
};

typedef struct shalloc_buff_op_s {
    shalloc_create_t create;
    shalloc_destroy_t destroy;
    shalloc_malloc_t malloc;
    shalloc_free_t free;
    shalloc_calloc_t calloc;
} shalloc_buff_op_t;

typedef struct shalloc_buff_s {
    void *start;
    void *end;
    size_t size;
    size_t unused_size;
    size_t block_size;
    struct shalloc_buff_s *next;
    enum shalloc_buff_alloc_type alloc_type;
    shalloc_buff_op_t *op;
} shalloc_buff_t;

#define SHALLOC_BUFF_ALLOC_SIZE(S) ((size_t)(2.1*((double)(S))))
#define SHALLOC_BUFF_MIN_SIZE       (SHALLOC_PAGE_SIZE/4)
#define SHALLOC_IS_BUFF_ADDR(B, A) (((void*)(A)) >= (B)->start && \
    ((void*)(A)) <= (B)->end)

/* Shadow region definitions. */
typedef struct shalloc_region_s {
    shalloc_buff_t *data_head;
    shalloc_buff_t *data_tail;
    shalloc_buff_t default_data;
    int flags;
    struct shalloc_region_s *parent;
} shalloc_region_t;

typedef struct {
    int num_buffs;
    size_t tot_buff_size;
    size_t last_buff_size;
} shalloc_region_info_t;

#define SHALLOC_REGION_TYPE_DEFAULT         SHALLOC_BUFF_ALLOC_TYPE_NOFREE
#define SHALLOC_REGION_BUFF_SIZE_DEFAULT    SHALLOC_PAGE_SIZE
#define SHALLOC_REGION_BUFF_ITER(R,PREV,CURR,DO) do { \
	PREV=CURR=NULL; \
        if((R)->data_head) { \
            while(!(CURR) || (CURR)->next) { \
                PREV = CURR; \
                CURR = !(CURR) ? (R)->data_head : (CURR)->next; \
                DO \
            } \
        } \
    } while(0)

/* Shadow heap definitions. */
typedef struct {
    shalloc_buff_t base;
    shalloc_region_t region;
    int inherit_id; 
    int mmap_flags;
} shalloc_heap_t;

#define SHALLOC_MAX_INHERIT_HEAPS       10
#define SHALLOC_INHERIT_ID              "inherit_id"
#define SHALLOC_MAP_INHERIT             0x0800
#define SHALLOC_GET_EXEC_ENVP()         (SHALLOC_CONTROL_DATA->inherit_envp)

/* Shadow magic info */
typedef struct {
    char *inherit_envp[2];
    char inherit_envp_buff[32];
    int inherit_id;
    int num_heaps;
    shalloc_heap_t heap_list[SHALLOC_MAX_INHERIT_HEAPS];
} shalloc_magic_t;

/* Make SHALLOC_BUFF_ALLOC_TYPE_MPLITE the default heap allocator.
 * - SHALLOC_BUFF_ALLOC_TYPE_MPLITE wastes more memory (see MPLITE_MIN_ALLOC)
 * - SHALLOC_BUFF_ALLOC_TYPE_SIMPLE seems buggy
 * - SHALLOC_BUFF_ALLOC_TYPE_BUFFER has problems with large allocations
 */
#define SHALLOC_HEAP_TYPE_DEFAULT           SHALLOC_BUFF_ALLOC_TYPE_MPLITE

/* Shadow space definitions. */
typedef struct {
    shalloc_buff_t base;
    shalloc_buff_t data;
    void *magic_control_page;
    void *magic_inherit_page;
    int num_heaps;
    int has_noninherit_heaps;
    int mmap_flags;
    shalloc_heap_t *priv_heap;
    shalloc_buff_op_t buff_ops[__NUM_SHALLOC_BUFF_ALLOC_TYPES];
} shalloc_space_t;
extern shalloc_space_t *shalloc_space;
extern shalloc_buff_op_t shalloc_space_buff_ops[__NUM_SHALLOC_BUFF_ALLOC_TYPES];

#define SHALLOC_IS_SPACE_ADDR(A) SHALLOC_IS_BUFF_ADDR(&shalloc_space->data, A)

/* Printing functions. */
#define shalloc_printf                    printf

void shalloc_space_print();
void shalloc_buff_print(shalloc_buff_t *buff);
void shalloc_heap_print(shalloc_heap_t *heap);
void shalloc_region_print(shalloc_region_t *region);

/* Shalloc allocator interface. */
void shalloc_buff_reset(shalloc_buff_t* buff);

shalloc_heap_t* shalloc_heap_create(size_t size, int mmap_flags,
    enum shalloc_buff_alloc_type type);
shalloc_region_t* shalloc_heap_to_region(shalloc_heap_t* heap);
shalloc_buff_t* shalloc_heap_to_buff(shalloc_heap_t* heap);
shalloc_heap_t* shalloc_region_to_heap(shalloc_region_t* region);

shalloc_region_t* shalloc_region_create(shalloc_region_t *region,
    size_t init_size, size_t buff_size,  size_t block_size, int flags,
    enum shalloc_buff_alloc_type type);
void shalloc_region_destroy(shalloc_region_t* region);
void shalloc_region_reset(shalloc_region_t* region);
void shalloc_region_get_info(shalloc_region_t* region,
    shalloc_region_info_t *info);

void* shalloc_malloc(shalloc_region_t *region, size_t size);
void shalloc_free(shalloc_region_t *region, void *ptr);
void* shalloc_calloc(shalloc_region_t *region, size_t nmemb, size_t size);
void* shalloc_orealloc(shalloc_region_t *region, void *ptr, size_t size,
    size_t old_size);

void shalloc_space_init();
void shalloc_space_close();
void shalloc_space_freeze();
#define SHALLOC_INHERIT_DATA     (shalloc_space->magic_inherit_page)
#define SHALLOC_CONTROL_DATA     ((shalloc_magic_t*)shalloc_space->magic_control_page)

#endif /* SHALLOC_H */

