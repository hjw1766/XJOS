#include <xjos/arena.h>
#include <xjos/memory.h>
#include <xjos/string.h>
#include <xjos/stdlib.h>
#include <xjos/assert.h>


extern u32 free_pages;
static arena_descriptor_t descriptors[DESC_COUNT];


void arena_init() {
    u32 block_size = 16;
    for (size_t i = 0; i < DESC_COUNT; i++) {
        arena_descriptor_t *desc = &descriptors[i];
        desc->block_size = block_size;
        desc->total_block = (PAGE_SIZE - sizeof(arena_t)) / block_size;
        list_init(&desc->free_list);
        block_size <<= 1;    // 16 32 64...1024
    }
}


// get arena memory poitner the idx block 
static arena_t *get_arena_block(arena_t *arena, u32 idx) {
    assert(idx < arena->desc->total_block);
    void *addr = (void *)(arena + 1);
    u32 gap = idx * arena->desc->block_size;
    return addr + gap;
}


static arena_t *get_block_arena(block_t *block) {
    return (arena_t *)((u32)block & 0xFFFFF000);
}


void *kmalloc(size_t size) {
    arena_descriptor_t *desc = NULL;
    arena_t *arena;
    block_t *block;
    char *addr;

    if (size > 1024) {
        u32 asize = size + sizeof(arena_t);
        u32 count = div_round_up(asize, PAGE_SIZE);

        arena = (arena_t *)alloc_kpage(count);
        memset(arena, 0, count * PAGE_SIZE);
        arena->large = true;
        arena->count = count;
        arena->desc = NULL;
        arena->magic = XJOS_MAGIC;

        addr = (char *)((u32)arena + sizeof(arena_t));
        return addr;
    }

    // size < 1024byte
    for (size_t i = 0; i < DESC_COUNT; i++) {
        desc = &descriptors[i];
        if (desc->block_size >= size)
            break;
    }

    assert(desc != NULL);

    // find free block
    if (list_empty(&desc->free_list)) {
        arena = (arena_t *)alloc_kpage(1);
        memset(arena, 0, PAGE_SIZE);

        arena->desc = desc;                     // use which descriptor
        arena->large = false;                   // small arena
        arena->count = desc->total_block;       // total number of blocks
        arena->magic = XJOS_MAGIC;              // magic number

        for (size_t i = 0; i < desc->total_block; i++) {
            block = (block_t *)get_arena_block(arena, i);  // get i-th block
            assert(!list_search(&arena->desc->free_list, block));
            list_push(&arena->desc->free_list, block);
            assert(list_search(&arena->desc->free_list, block));
        }
    } 

    // distribute block
    block = list_pop(&desc->free_list);

    arena = get_block_arena(block);     // base address of arena
    assert(arena->magic == XJOS_MAGIC && !arena->large);

    arena->count--;

    return block;
}


void kfree(void *ptr) {
    assert(ptr);

    block_t *block = (block_t *)ptr;
    arena_t *arena = get_block_arena(block);

    assert(arena->large == true || arena->large == false);
    assert(arena->magic == XJOS_MAGIC);

    if (arena->large) {
        free_kpage((u32)arena, arena->count);
        return;
    }

    assert(arena->count < arena->desc->total_block);

    memset(block, 0, arena->desc->block_size);
    list_push(&arena->desc->free_list, block);
    arena->count++;

    if (arena->count == arena->desc->total_block) {
        for (size_t i = 0; i < arena->desc->total_block; i++) {
            block = (block_t *)get_arena_block(arena, i);
            assert(list_search(&arena->desc->free_list, block));
            list_remove(block);
            assert(!list_search(&arena->desc->free_list, block));
        }
        free_kpage((u32)arena, 1);
    }
}