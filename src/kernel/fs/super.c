#include <fs/fs.h>
#include <fs/buffer.h>
#include <drivers/device.h>
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
            return sb;
        }
    }

    return NULL;
}


super_block_t *read_super(dev_t dev) {
    super_block_t *sb = get_super(dev);
    if (sb)
        return sb;

    LOGK("Reading super block from device %d\n", dev);

    sb = get_free_super();

    buffer_t *buf = bread(dev, 1); // super block is in block 1

    sb->buf = buf;
    sb->desc = (super_desc_t *)buf->data;
    sb->dev = dev;

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

    idx_t idx = 0;
    inode_t *inode = iget(root->dev, 1);

    idx = bmap(inode, 3, true);

    idx = bmap(inode, 7 + 7, true);

    idx = bmap(inode, 7 + 512 * 3 + 510, true);

    iput(inode);
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