#include <fs/fs.h>
#include <xjos/syscall.h>
#include <libc/assert.h>
#include <xjos/debug.h>
#include <fs/buffer.h>
#include <libc/string.h>
#include <xjos/stdlib.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)


#define INODE_NR 64

static inode_t inode_table[INODE_NR];

// apply inode
static inode_t *get_free_inode() {
    for (size_t i = 0; i < INODE_NR; i++) {
        inode_t *inode = &inode_table[i];
        if (inode->dev == EOF)
            return inode;
    }

    panic("no free inode");
}


// release inode
static void put_free_inode(inode_t *inode) {
    assert(inode != inode_table);
    assert(inode->count == 0);
    inode->dev = EOF;
}


// get root inode
inode_t *get_root_inode() {
    return inode_table;
}


// inode nr -> block
static inline idx_t inode_block(super_block_t *sb, idx_t nr) {
    /*
        one block(1024 byte) / inode_desc_t(32 byte) = 32 inode_desc_t
        inode 0~31 -> block 1 (inode table)
        inode 32~63 -> block 2
    */
    return 2 + sb->desc->imap_blocks + sb->desc->zmap_blocks + (nr - 1) / BLOCK_INODES;
}


// find inode by nr
static inode_t *find_inode(dev_t dev, idx_t nr) {
    super_block_t *sb = get_super(dev);
    assert(sb);
    // inode list
    list_t *list = &sb->inode_list;

    inode_t *inode;

    list_for_each_entry(inode, list, node) {
        if (inode->nr == nr) {
            return inode;
        }
    }

    return NULL;
}


// get dev - nr inode
inode_t *iget(dev_t dev, idx_t nr) {
    // find cached inode
    inode_t *inode = find_inode(dev, nr);
    if (inode) {
        inode->count++;
        inode->atime = time();

        return inode;
    }

    // miss
    super_block_t *sb = get_super(dev);
    assert(sb);

    assert(nr <= sb->desc->inodes);

    inode = get_free_inode();
    inode->dev = dev;
    inode->nr = nr;
    inode->count = 1;

    // add super block inode list
    list_push(&sb->inode_list, &inode->node);

    idx_t block = inode_block(sb, nr);

    buffer_t *buf = bread(dev, block);
    inode->buf = buf;   // Record buffer pointer
    
    // point to inode descriptor in buffer
    inode->desc = &((inode_desc_t *)buf->data)[(inode->nr - 1) % BLOCK_INODES];

    inode->ctime = inode->desc->mtime;
    inode->atime = time();

    return inode;
}


// free inode
void iput(inode_t *inode) {
    if (!inode)
        return;

    inode->count--;
    if (inode->count)
        return;
    
    // count == 0

    brelse(inode->buf);
    
    list_remove(&inode->node);

    put_free_inode(inode);
}


void inode_init() {
    for (size_t i = 0; i < INODE_NR; i++) {
        inode_t *inode = &inode_table[i];
        inode->dev = EOF;
    }
}