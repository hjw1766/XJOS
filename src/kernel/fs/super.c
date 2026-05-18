#include <fs/fs.h>
#include <fs/buffer.h>
#include <drivers/device.h>
#include <fs/stat.h>
#include <xjos/task.h>
#include <xjos/assert.h>
#include <xjos/string.h>
#include <xjos/debug.h>
#include <xjos/stdlib.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

#define SUPER_NR 16

static super_t super_table[SUPER_NR]; // 超级块表
static super_t *root;                 // 根文件系统超级块

// get a free super block from super block table
super_t *get_free_super() {
    for (size_t i = 0; i < SUPER_NR; i++) {
        super_t *super = &super_table[i];
        if (super->type == FS_TYPE_NONE) {
            return super;
        }
    }

    panic("no free super block!!!");
}


// get dev super block
super_t *get_super(dev_t dev) {
    for (size_t i = 0; i < SUPER_NR; i++) {
        super_t *super = &super_table[i];
        if (super->type == FS_TYPE_NONE)
            continue;

        if (super->dev == dev) {
            return super;
        }
    }

    return NULL;
}


void put_super(super_t *super) {
    if (!super)
        return;
    assert(super->count > 0);
    super->count--;
    if (super->count)
        return;

    super->type = FS_TYPE_NONE;
    iput(super->imount);
    iput(super->iroot);
    brelse(super->buf);
}


super_t *read_super(dev_t dev) {
    super_t *super = get_super(dev);
    if (super) {
        super->count++;
        return super;
    }

    LOGK("Reading super block from device %d\n", dev);

    super = get_free_super();
    super->count++;

    for (size_t i = 1; i < FS_TYPE_NUM; i++) {
        fs_op_t *op = fs_get_op(i);
        if (!op)
            continue;
        if (op->super(dev, super) == EOK) {
            return super;
        }
    }

    put_super(super);
    return NULL;
}


static void mount_root() {
    LOGK("Mounting root file system...\n");

    device_t *master = device_find(DEV_IDE_PART, 0);
    root = read_super(master->dev);
    assert(root);
    assert(root->iroot);

    root->imount = root->iroot;
    root->imount->count++;
    root->iroot->mount = master->dev;

    extern task_t *tasks_table[TASK_NR];
    inode_t *boot_root = get_root_inode();
    for (size_t i = 0; i < TASK_NR; i++) {
        task_t *task = tasks_table[i];
        if (!task)
            continue;
        if (task->iroot == boot_root)
            task->iroot = root->iroot;
        if (task->ipwd == boot_root)
            task->ipwd = root->iroot;
    }
}


void super_init() {
    for (size_t i = 0; i < SUPER_NR; i++) {
        super_t *super = &super_table[i];
        super->dev = EOF;
        super->type = FS_TYPE_NONE;
        super->desc = NULL;
        super->buf = NULL;
        super->iroot = NULL;
        super->block_size = 0;
        super->sector_size = 0;
        list_init(&super->inode_list);
    }

    mount_root();
}
