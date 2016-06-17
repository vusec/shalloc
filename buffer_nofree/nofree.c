#include <buffer_nofree/nofree.h>

#define NOFREE_CHECK_LEVEL    1

static void nofree_check(nofree_header_t *header)
{
    assert(header->magic_start && header->magic_start == header->magic_end);
    assert(header->next);
}

static size_t nofree_align_up(size_t size, size_t align)
{
    return (size + align - 1) & ~(align - 1);
}

int nofree_init(void *buff, size_t buff_size)
{
    nofree_header_t *header = (nofree_header_t*) buff;
    if (buff_size <= sizeof(nofree_header_t)) {
        return -1;
    }
    header->magic_start = header->magic_end = 0xdeadbeef;
    header->next = (char*)buff +
        nofree_align_up(sizeof(nofree_header_t), sizeof(long));
    return 0;
}

void nofree_close(void *buff, size_t buff_size)
{
    nofree_header_t *header = (nofree_header_t*) buff;
    nofree_check(header);
    memset(header, 0, sizeof(nofree_header_t));
}

void* nofree_alloc(void *buff, size_t buff_size, size_t size,
    size_t align)
{
    void *ptr;
    nofree_header_t *header = (nofree_header_t*) buff;
#if NOFREE_CHECK_LEVEL > 0
    nofree_check(header);
#endif
    if (size == 0) {
        return NULL;
    }
    if (align) {
        size = nofree_align_up(size, align);
    }
    ptr = header->next;
    header->next += size;
    if (header->next > (char*) buff + buff_size || (void*) header->next < buff) {
        header->next -= size;
        return NULL;
    }
    return ptr;
}

