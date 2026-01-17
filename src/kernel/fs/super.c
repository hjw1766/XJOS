#include <fs/fs.h>
#include <fs/buffer.h>
#include <drivers/device.h>
#include <fs/stat.h>
#include <xjos/syscall.h>
#include <xjos/task.h>
#include <xjos/assert.h>
#include <xjos/string.h>
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

int devmkfs(dev_t dev, u32 icount) {
    super_block_t *sb = NULL;
    buffer_t *buf = NULL;
    int ret = EOF;

    int total_block = device_ioctl(dev, DEV_CMD_SECTOR_SIZE, NULL, 0) / BLOCK_SECS;
    assert(total_block);    // device size > 0
    assert(icount < total_block);

    if (!icount) {
        icount = total_block / 3;   // default 1/3 blocks for inodes
    }

    sb = get_free_super();
    sb->dev = dev;
    sb->count = 1;

    buf = bread(dev, 1); // super block is in block 1
    sb->buf = buf;
    bdirty(buf, true);

    // init super block
    super_desc_t *desc = (super_desc_t *)buf->data;
    sb->desc = desc;

    int inode_blocks = div_round_up(icount * sizeof(inode_desc_t), BLOCK_SIZE);
    desc->inodes = icount;
    desc->zones = total_block;
    // icount need use imap
    desc->imap_blocks = div_round_up(icount, BLOCK_BITS);

    // data zones need use zmap
    int zcount = total_block - desc->imap_blocks - inode_blocks - 2;
    desc->zmap_blocks = div_round_up(zcount, BLOCK_BITS);

    desc->firstdatazone = 2 + desc->imap_blocks + desc->zmap_blocks + inode_blocks;
    desc->long_zone_size = 0;
    desc->max_size = BLOCK_SIZE * TOTAL_BLOCK;
    desc->magic = MINIX1_MAGIC;

    // clear map
    memset(sb->imaps, 0, sizeof(sb->imaps));
    memset(sb->zmaps, 0, sizeof(sb->zmaps));

    int idx = 2;    // 0 boot, 1 super
    for (int i = 0; i < sb->desc->imap_blocks; i++) {
        if ((sb->imaps[i] = bread(dev, idx))) {
            memset(sb->imaps[i]->data, 0, BLOCK_SIZE);
            bdirty(sb->imaps[i], true);
            idx++;
        } else
            break;
    }

    for (int i = 0; i < sb->desc->zmap_blocks; i++) {
        if ((sb->zmaps[i] = bread(dev, idx))) {
            memset(sb->zmaps[i]->data, 0, BLOCK_SIZE);
            bdirty(sb->zmaps[i], true);
            idx++;
        } else
            break;
    }

    // init bitmap
    idx = balloc(dev);  // alloc block 0 (not use)
    idx = ialloc(dev);  // alloc inode 0 (not use)
    idx = ialloc(dev);  // alloc inode 1 (root inode)

    int counts[] = {
        icount + 1,     // idx 0 not use
        zcount,
    };

    buffer_t *maps[] = {
        sb->imaps[sb->desc->imap_blocks - 1],   // last imap block
        sb->zmaps[sb->desc->zmap_blocks - 1],   // last zmap block
    };

    for (size_t i = 0; i < 2; i++) {
        int count = counts[i];
        buffer_t *map = maps[i];
        bdirty(map, true);

        int offset = count % BLOCK_BITS;
        int begin = offset / 8;
        // 指向最后一个字节(部分使用的混合字节)
        char *ptr = (char *)map->data + begin;
        // 避开前面的完整字节，直接先 + 1
        memset(ptr + 1, 0xFF, BLOCK_SIZE - begin - 1);
        
        // 处理混合字节
        int bits = 0x80;
        char data = 0;
        int remian = 8 - offset % 8;
        while (remian--) {
            data |= bits;
            bits >>= 1;
        }
        ptr[0] = data;
    }

    task_t *task = running_task();

    inode_t *iroot = new_inode(dev, 1);
    sb->iroot = iroot;

    iroot->desc->mode = (0777 & ~task->umask) | IFDIR;
    iroot->desc->size = sizeof(dirent_t) * 2;
    iroot->desc->nlinks = 2;

    buf = bread(dev, bmap(iroot, 0, true));
    
    dentry_t *entry = (dirent_t *)buf->data;
    memset(entry, 0, BLOCK_SIZE);
    
    strcpy(entry->name, ".");
    entry->nr = iroot->nr;
    entry++;
    
    strcpy(entry->name, "..");
    entry->nr = iroot->nr;
    
    bdirty(buf, true);
    brelse(buf);
    ret = 0;
rollback:
    put_super(sb);
    return ret;
}

int sys_mkfs(char *devname, int icount) {
    inode_t *inode = NULL;
    int ret = EOF;

    inode = namei(devname);
    if (!inode)
        goto rollback;
    if (!ISBLK(inode->desc->mode))
        goto rollback;
    
    dev_t dev = inode->desc->zones[0];
    assert(dev);

    ret = devmkfs(dev, icount);

rollback:
    iput(inode);
    return ret;
}