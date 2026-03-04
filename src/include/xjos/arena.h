#ifndef XJOS_ARENA_H
#define XJOS_ARENA_H

#include <xjos/types.h>
#include <xjos/list.h>

#define DESC_COUNT 7

typedef list_node_t block_t;    // memory block

// memory descriptor
typedef struct {
    u32 total_block;             // total number of blocks
    u32 block_size;              // size of each block
    u32 page_count;             // free page count for this block size
    list_t free_list;             // free block list
}arena_descriptor_t;

// One page or more pages
typedef struct {
    arena_descriptor_t *desc;   // arena descriptor
    u32 count;                  // number of blocks in this page
    bool large;                  // flag 1024-byte
    u32 magic;                  // magic number
}arena_t;

void *kmalloc(size_t size);
void kfree(void *ptr);


#endif /* XJOS_ARENA_H */