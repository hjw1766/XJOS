#include <fs/fs.h>
#include <fs/buffer.h>
#include <drivers/device.h>
#include <libc/assert.h>
#include <libc/string.h>
#include <xjos/debug.h>

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


static void test_file_system(device_t *device, int test_inode_nr) {
    LOGK("========================================\n");
    LOGK("Testing file system on device %s (dev %d), Reading Inode %d\n", device->name, device->dev, test_inode_nr);

    super_block_t *sb = read_super(device->dev);
    assert(sb);

    // calculate the block number where the inode table is located
    int inode_table_blk = 2 + sb->desc->imap_blocks + sb->desc->zmap_blocks;

    // read first inode
    buffer_t *buf = bread(device->dev, inode_table_blk);
    inode_desc_t *inode_table = (inode_desc_t *)buf->data; 
    inode_desc_t *inode = &inode_table[test_inode_nr - 1]; // inode号从1开始  

    LOGK("  [Inode %d] Mode: 0x%04x, Size: %d\n", test_inode_nr, inode->mode, inode->size);

    // type
    if (inode->mode & 0x4000) {
        LOGK("    Type: Directory\n");
        if (inode->zones[0]) {
            buffer_t *dir_buf = bread(device->dev, inode->zones[0]);
            dentry_t *entry = (dentry_t *)dir_buf->data;

            while (entry->nr) {
                LOGK("    [Dentry] Inode: %d, Name: %s\n", entry->nr, entry->name);
                entry++;
            }
        }
    } else if (inode->mode & 0x8000) {
        LOGK("    Type: Regular File\n");
        if (inode->zones[0]) {
            buffer_t *file_buf = bread(device->dev, inode->zones[0]);
            char *data = file_buf->data;

            LOGK("    File Data (first 64 bytes or less):\n");
            LOGK("data: %s\n", data);
            LOGK("\n");
        }
    } else {
        LOGK("    Type: Unknown\n");
    }

    LOGK("========================================\n");
}


static void mount_root() {
    LOGK("Mounting root file system...\n");

    device_t *master = device_find(DEV_IDE_PART, 0);
    root = read_super(master->dev);

    device_t *slave = device_find(DEV_IDE_PART, 1);
    read_super(slave->dev);

    test_file_system(master, 1);
    test_file_system(master, 7);

    test_file_system(slave, 1);
    test_file_system(slave, 2);
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