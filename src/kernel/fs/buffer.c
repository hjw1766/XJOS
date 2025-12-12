#include <fs/buffer.h>
#include <xjos/memory.h>
#include <xjos/debug.h>
#include <xjos/task.h>
#include <libc/assert.h>
#include <libc/string.h>
#include <drivers/device.h>


#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

// hash
static list_t *hash_table;   // hash table
static u32 hash_mask;
static u32 hash_size;

static buffer_t *buffer_start;
static u32 buffer_count = 0;

// current buffer ptr
static buffer_t *buffer_ptr;
// current data buffer ptr
static void *buffer_data;

static list_t free_list;    // cache free list(LRU)
static list_t dirty_list;   // cache dirty list [新增: 脏缓冲链表]
static list_t wait_list;    // wait list

/**
 * hash function
 */
static u32 hash_fn(dev_t dev, idx_t block) {
    u32 key = dev ^ block;
    key += ~(key << 15);
    key ^= (key >> 10);
    key += (key << 3);
    key ^= (key >> 6);
    key += ~(key << 11);
    key ^= (key >> 16);
    return key & hash_mask;
}


/**
 * hash table handle
 */

static buffer_t *get_from_hash_table(dev_t dev, idx_t block) {
    u32 idx = hash_fn(dev, block);
    list_t *list = &hash_table[idx];

    // traverse bucket - list
    for (list_node_t *node = list->head.next; node != &list->head; node = node->next) {
        buffer_t *bf = list_entry(node, buffer_t, hnode);
        if (bf->dev == dev && bf->block == block) {
            return bf;
        }
    }
    return NULL;
}


static void hash_insert(buffer_t *bf) {
    u32 idx = hash_fn(bf->dev, bf->block);
    list_t *list = &hash_table[idx];
    // insert to head, locality
    list_push(list, &bf->hnode);
}


static void hash_remove(buffer_t *bf) {
    // bf exists in hash table
    if (bf->hnode.next && bf->hnode.prev) {
        list_remove(&bf->hnode);
    }
}


/**
 * buffer alloc and control
 */

static buffer_t *get_new_buffer() {
    buffer_t *bf = NULL;

    if ((u32)buffer_ptr + sizeof(buffer_t) < (u32)buffer_data) {
        bf = buffer_ptr;
        bf->data = buffer_data;
        bf->dev = EOF;
        bf->block = 0;
        bf->count = 0;
        bf->dirty = false;
        bf->valid = false;
        
        buffer_count++;
        buffer_ptr++;
        buffer_data -= BLOCK_SIZE;
    }
    return bf;
}


static buffer_t *get_free_buffer() {
    buffer_t *bf = NULL;
    while (true) {
        // 1. try to get new buffer
        bf = get_new_buffer();
        if (bf) {
            return bf;
        }

        // 2. LRU back replace
        if (!list_empty(&free_list)) {
            bf = list_entry(list_popback(&free_list), buffer_t, lru_node);
            
            if (bf->dirty) {
                // must write back first
                bwrite(bf);
            }
            hash_remove(bf);

            bf->valid = false;
            bf->dirty = false;  // 多余操作
            return bf;
        }

        // 3. wait for buffer release
        task_block(running_task(), &wait_list, TASK_WAITING);
    }
}


/**
 * buffer kernel API
 */

buffer_t *getblk(dev_t dev, idx_t block) {
    buffer_t *bf = get_from_hash_table(dev, block);
    if (bf) {
        // cache hit
        assert(bf->valid);
        bf->count++;
        if (bf->count == 1) {
            // 被复用
            list_remove(&bf->lru_node);
        }
        return bf;
    }

    // cache miss
    bf = get_free_buffer();
    assert(bf->count == 0);
    assert(bf->dirty == false);

    bf->count = 1;
    bf->dev = dev;
    bf->block = block;

    hash_insert(bf);

    return bf;
}


buffer_t *bread(dev_t dev, idx_t block) {
    buffer_t *bf = getblk(dev, block);
    assert(bf != NULL);
    if (bf->valid)
        return bf;

    // read disk
    device_request(bf->dev, bf->data, BLOCK_SECS, bf->block * BLOCK_SECS, 0, REQ_READ);

    bf->dirty = false;
    bf->valid = true;
    return bf;
}


void bwrite(buffer_t *bf) {
    assert(bf);
    if (!bf->dirty)     // no need to write
        return;

    // write to disk
    device_request(bf->dev, bf->data, BLOCK_SECS, bf->block * BLOCK_SECS, 0, REQ_WRITE);

    bdirty(bf, false);
    bf->valid = true;
} 


void brelse(buffer_t *bf) {
    if (!bf)
        return;
    
    bf->count--;
    assert(bf->count >= 0);

    if (bf->count == 0) {
        // 只要引用归零， 就放入 free_list
        list_push(&free_list, &bf->lru_node);
        // wake-up waiters
        if (!list_empty(&wait_list)) {
            // wake up one waiting task
            task_t *task = list_entry(list_pop(&wait_list), task_t, node);
            task_unblock(task);
        }
    }
}


void bsync() {
    // [修改] 优化后的 sync，只处理脏链表
    buffer_t *bf = NULL;
    int flushed_count = 0; // 新增统计
    list_node_t *node = dirty_list.head.next;
    while (node != &dirty_list.head) {
        bf = list_entry(node, buffer_t, dirty_node);
        node = node->next; // 先保存下一个节点，防止 bwrite 修改链表

        bwrite(bf);
        flushed_count++;
    }

    if (flushed_count > 0) {
        LOGK("bsync: [Dirty List Logic] Flushed %d blocks to disk.\n", flushed_count);
    }
}


void bdirty(buffer_t *bf, bool dirty) {
    if (bf->dirty == dirty)
        return;
    bf->dirty = dirty;

    if (dirty) {
        // 变脏
        list_push(&dirty_list, &bf->dirty_node);
    } else {
        // 变干净
        list_remove(&bf->dirty_node);
    }
}


/**
 * init
 */

void buffer_init() {
    LOGK("buffer_init: init...\n");

    list_init(&free_list);
    list_init(&dirty_list); // [新增] 初始化脏链表
    list_init(&wait_list);

    u32 total_mem_size = KERNEL_BUFFER_SIZE;
    u32 entry_size = sizeof(buffer_t) + BLOCK_SIZE;
    u32 max_buffers = total_mem_size / entry_size;
    
    LOGK("buffer_init: estimated max buffers = %d\n", max_buffers);
    hash_size = 1;
    while (hash_size < max_buffers)
        hash_size <<= 1;    // bucket >= max_buffers
    
    hash_mask = hash_size - 1;
    LOGK("buffer_init: hash table size = %d, mask = 0x%x\n", hash_size, hash_mask);

    hash_table = (list_t *)KERNEL_BUFFER_MEM; 
    u32 hash_table_bytes = hash_size * sizeof(list_t);

    for (u32 i = 0; i < hash_size; i++) {
        list_init(&hash_table[i]);
    }

    buffer_start = (buffer_t *)(KERNEL_BUFFER_MEM + hash_table_bytes);
    buffer_ptr = buffer_start;

    buffer_data = (void *)(KERNEL_BUFFER_MEM + KERNEL_BUFFER_SIZE - BLOCK_SIZE);
    assert((u32)buffer_ptr < (u32)buffer_data);
}