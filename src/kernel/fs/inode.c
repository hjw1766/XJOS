#include <fs/fs.h>
#include <fs/stat.h>
#include <xjos/assert.h>
#include <xjos/debug.h>
#include <fs/buffer.h>
#include <xjos/string.h>
#include <xjos/stdlib.h>
#include <xjos/task.h>
#include <xjos/memory.h>
#include <xjos/arena.h>
#include <xjos/fifo.h>
#include <drivers/device.h>


#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

extern time_t sys_time();


#define INODE_NR 64

static inode_t inode_table[INODE_NR];

// apply inode
inode_t *get_free_inode() {
    // inode_table[0] is reserved as the bootstrap root placeholder.
    for (size_t i = 1; i < INODE_NR; i++) {
        inode_t *inode = &inode_table[i];
        if (inode->type == FS_TYPE_NONE)
            return inode;
    }

    panic("no free inode");
}


// release inode
void put_free_inode(inode_t *inode) {
    assert(inode != inode_table);
    assert(inode->count == 0);
    inode->dev = EOF;
    inode->nr = 0;
    inode->super = NULL;
    inode->op = NULL;
    inode->type = FS_TYPE_NONE;
}


// get root inode
inode_t *get_root_inode() {
    return inode_table;
}


// find inode by nr
inode_t *find_inode(dev_t dev, idx_t nr) {
    super_t *super = get_super(dev);
    assert(super);
    // inode list
    list_t *list = &super->inode_list;

    inode_t *inode;

    list_for_each_entry(inode, list, node) {
        if (inode->nr == nr) {
            return inode;
        }
    }

    return NULL;
}


inode_t *fit_inode(inode_t *inode) {
    if (!inode || !inode->mount)
        return inode;
    
    super_t *super = get_super(inode->mount);
    assert(super);

    // must be mount root inode
    assert(super->iroot);

    inode_t *root = super->iroot;
    root->count++;  // increase reference count
    iput(inode);    // release original inode

    return root;
}


// free inode
void iput(inode_t *inode) {
    if (!inode)
        return;

    inode->op->close(inode);
}


void inode_init() {
    for (size_t i = 0; i < INODE_NR; i++) {
        inode_t *inode = &inode_table[i];
        inode->dev = EOF;
        inode->count = 0;
        inode->nr = 0;
        inode->super = NULL;
        inode->op = NULL;
        inode->type = FS_TYPE_NONE;
        inode->rxwaiter = NULL;
        inode->txwaiter = NULL;
    }
}
