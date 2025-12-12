#ifndef XJOS_BUFFER_H
#define XJOS_BUFFER_H

#include <xjos/types.h>
#include <fs/fs.h>
#include <xjos/list.h>
#include <xjos/spinlock.h>

#define BLOCK_SECS (BLOCK_SIZE / SECTOR_SIZE) // 1 block = 2 sectors

typedef struct buffer_t {
    char *data;         // buffer data ptr
    dev_t dev;        // device number
    idx_t block;    // block number
    int count;      // reference count
    list_node_t hnode;  // hash list node
    // list_node_t rnode;  // buffer list node

    list_node_t lru_node;   // node for free_list(clean & idle)
    list_node_t dirty_node; // node for dirty_list(all dirty buffers) 

    spinlock_t lock; // todo: spinlock for buffer
    bool dirty;    // has been modified
    bool valid;    // has been read from disk
} buffer_t;

void bdirty(buffer_t *bf, bool dirty);

buffer_t *getblk(dev_t dev, idx_t block);
buffer_t *bread(dev_t dev, idx_t block);
void bwrite(buffer_t *bf);
void brelse(buffer_t *bf);

void buffer_init();

void bsync();

#endif //XJOS_BUFFER_H