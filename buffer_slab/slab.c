#include <buffer_slab/slab.h>
#include <limits.h>

static inline void slab_check(slab_header_t *header)
{
    assert(header->magic_start && header->magic_start == header->magic_end);
}

static inline unsigned slab_address_to_block(slab_header_t *header,
    char *addr)
{
    unsigned block;
    if (addr < header->blocks) {
        return UINT_MAX;
    }
    block = addr - header->blocks;
    if (block%header->block_size > 0) {
        return UINT_MAX;
    }
    block/=header->block_size;
    if (block >= header->num_blocks) {
        return UINT_MAX;
    }
    return block;
}

static inline void* slab_block_to_address(slab_header_t *header,
    unsigned block)
{
    return header->blocks + block*header->block_size;
}

static inline unsigned slab_find_free_block(slab_header_t *header)
{
    unsigned next_block = header->next_block;
    unsigned i;
    for (i=0;i<header->num_blocks;i++) {
        if (header->bitmap[next_block] == 0) {
            return next_block;
        }
        next_block = (next_block+1) % header->num_blocks;
    }
    return UINT_MAX;
}

int slab_init(void *buff, size_t buff_size, size_t block_size)
{
    slab_header_t *header = (slab_header_t*) buff;
    block_size += block_size%sizeof(long) ?
        sizeof(long) - block_size%sizeof(long) : 0;
    if (buff_size <= sizeof(slab_header_t) + block_size*2 + sizeof(long)) {
        return -1;
    }
    if (block_size <=0 || buff_size <=0) {
        return -2;
    }
    header->magic_start = header->magic_end = 0xdeadbeef;
    header->block_size = block_size;
    header->num_blocks = (buff_size-sizeof(long)) / (block_size+1);
    assert(header->num_blocks > 0);
    header->bitmap = (char*) buff + sizeof(slab_header_t);
    header->blocks = header->bitmap + header->num_blocks;
    if (((unsigned long)header->blocks)%sizeof(long) > 0) {
        header->blocks += sizeof(long) -
            ((unsigned long)header->blocks)%sizeof(long);
    }
    memset(header->bitmap, 0, sizeof(char)*header->num_blocks);
    header->next_block = 0;
    return 0;
}

void slab_close(void *buff)
{
    slab_header_t *header = (slab_header_t*) buff;
    slab_check(header);
    memset(header, 0, sizeof(slab_header_t));
}

void* slab_alloc(void *buff, size_t size)
{
    slab_header_t *header = (slab_header_t*) buff;
    unsigned b;
    assert(size == header->block_size && "Invalid slab allocation size!");
    b = slab_find_free_block(header);
    if (b == UINT_MAX) {
        return NULL;
    }
    header->bitmap[b]=1;
    header->next_block = (b+1) % header->num_blocks;
    return slab_block_to_address(header, b);
}

void slab_free(void *buff, void *ptr)
{
    slab_header_t *header = (slab_header_t*) buff;
    unsigned b = slab_address_to_block(header, (char*)ptr);
    assert(b != UINT_MAX && "Invalid free!");
    assert(header->bitmap[b] == 1 && "Double free!");
    header->bitmap[b]=0;
}
