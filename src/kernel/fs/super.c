#include <fs/fs.h>
#include <fs/buffer.h>
#include <drivers/device.h>
#include <fs/stat.h>
#include <libc/assert.h>
#include <libc/string.h>
#include <xjos/debug.h>
#include <xjos/stdlib.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

#define SUPER_NR 16

static super_block_t super_table[SUPER_NR]; // super block table
static super_block_t *root;           // root super block pointer

// get a free super block from super block table
static super_block_t *get_free_super() {
    for (size_t i = 0; i < SUPER_NR; i++) {
        super_block_t *sb = &super_table[i];
        if (sb->dev == EOF) {
            return sb;
        }
    }

    panic("no free super block!!!");
}


// get dev super block
super_block_t *get_super(dev_t dev) {
    for (size_t i = 0; i < SUPER_NR; i++) {
        super_block_t *sb = &super_table[i];
        if (sb->dev == dev) {
            sb->count++;    // fix bug: should increase count here
            return sb;
        }
    }

    return NULL;
}


void put_super(super_block_t *sb) {
    if (!sb)
        return;
    assert(sb->count > 0);
    sb->count--;
    if (sb->count)
        return;

    sb->dev = EOF;
    iput(sb->imount);
    iput(sb->iroot);

    for (size_t i = 0; i < sb->desc->imap_blocks; i++)
        brelse(sb->imaps[i]);
    for (size_t i = 0; i < sb->desc->zmap_blocks; i++)
        brelse(sb->zmaps[i]);

    brelse(sb->buf);
}


super_block_t *read_super(dev_t dev) {
    super_block_t *sb = get_super(dev);
    if (sb) {
        return sb;
    }

    LOGK("Reading super block from device %d\n", dev);

    sb = get_free_super();

    buffer_t *buf = bread(dev, 1); // super block is in block 1

    sb->buf = buf;
    sb->desc = (super_desc_t *)buf->data;
    sb->dev = dev;
    sb->count = 1;

    assert(sb->desc->magic == MINIX1_MAGIC);

    memset(sb->imaps, 0, sizeof(sb->imaps));
    memset(sb->zmaps, 0, sizeof(sb->zmaps));

    // read inode map blocks
    int idx = 2;    // 0 boot, 1 super
    for (int i = 0; i < sb->desc->imap_blocks; i++) {
        assert(i < IMAP_NR);
        if ((sb->imaps[i] = bread(dev, idx)))
            idx++;
        else
            break;
    }

    // read zone map blocks
    for (int i = 0; i < sb->desc->zmap_blocks; i++) {
        assert(i < ZMAP_NR);
        if ((sb->zmaps[i] = bread(dev, idx)))
            idx++;
        else
            break;
    }

    return sb;
}


static void mount_root() {
    LOGK("Mounting root file system...\n");

    device_t *master = device_find(DEV_IDE_PART, 0);
    root = read_super(master->dev);

    root->iroot = iget(root->dev, 1);       // get '/'
    root->imount = iget(root->dev, 1);

    root->iroot->mount = master->dev;
}


void super_init() {
    for (size_t i = 0; i < SUPER_NR; i++) {
        super_block_t *sb = &super_table[i];
        sb->dev = EOF;
        sb->desc = NULL;
        sb->buf = NULL;
        sb->iroot = NULL;
        sb->imount = NULL;
        list_init(&sb->inode_list);
    }

    mount_root();
}


int sys_mount(char *devname, char *dirname, int flags) {
    LOGK("mount %s to %s\n", devname, dirname);

    inode_t *devinode = NULL;
    inode_t *dirinode = NULL;
    super_block_t *sb = NULL;
    devinode = namei(devname);
    if (!devinode)
        goto rollback;
    if (!ISBLK(devinode->desc->mode))
        goto rollback;

    // zones[0] is device number
    dev_t dev = devinode->desc->zones[0];

    dirinode = namei(dirname);
    if (!dirinode)
        goto rollback;
    if (!ISDIR(dirinode->desc->mode))
        goto rollback;
    if (dirinode->count != 1 || dirinode->mount) 
        goto rollback;

    sb = read_super(dev);   // read super block
    if (sb->imount)         // already mounted
        goto rollback;
    
    sb->iroot = iget(dev, 1);   // get root inode of mounted fs
    sb->imount = dirinode;
    dirinode->mount = dev;
    iput(devinode);

    return 0;

rollback:
    put_super(sb);
    iput(devinode);
    iput(dirinode);
    return EOF;
}


int sys_umount(char *target) {
    LOGK("umount %s\n", target);

    inode_t *inode = NULL;
    super_block_t *sb = NULL;
    int ret = EOF;

    inode = namei(target);
    if (!inode)
        goto rollback;
    if (!ISBLK(inode->desc->mode) && inode->nr != 1)
        goto rollback;
    if (inode == root->imount)
        goto rollback;

    dev_t dev = inode->dev;
    if (ISBLK(inode->desc->mode))
        dev = inode->desc->zones[0];
    
    sb = get_super(dev);
    if (!sb || !sb->imount)
        goto rollback;

    if (sb->iroot->count > 2)   // root inode using
        goto rollback;
    if (list_len(&sb->inode_list) > 1)  // other inodes using
        goto rollback;

    iput(sb->iroot);
    sb->iroot = NULL;
    sb->imount->mount = 0;
    iput(sb->imount);
    sb->imount = NULL;

    sb->count--;

    ret = 0;

rollback:
    put_super(sb);
    iput(inode);
    return ret;
}