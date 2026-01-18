#include <fs/fs.h>
#include <fs/stat.h>
#include <xjos/assert.h>
#include <xjos/debug.h>
#include <fs/buffer.h>
#include <xjos/string.h>
#include <xjos/stdlib.h>
#include <xjos/task.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

extern time_t sys_time();


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


static inode_t *fit_inode(inode_t *inode) {
    if (!inode || !inode->mount)
        return inode;
    
    super_block_t *sb = get_super(inode->mount);
    assert(sb);

    // must be mount root inode
    assert(sb->iroot);

    inode_t *root = sb->iroot;
    root->count++;  // increase reference count
    iput(inode);    // release original inode

    return root;
}


// get dev - nr inode
inode_t *iget(dev_t dev, idx_t nr) {
    // find cached inode
    inode_t *inode = find_inode(dev, nr);
    if (inode) {
        inode->count++;
        inode->atime = sys_time();

        return fit_inode(inode);
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
    inode->atime = sys_time();

    return inode;
}

inode_t *new_inode(dev_t dev, idx_t nr) {
    task_t *task = running_task();
    inode_t *inode = iget(dev, nr);
    if (!inode) {
        LOGK("new_inode: iget failed for dev %d nr %d in task %d\n", dev, nr, task->pid);
        return NULL;
    }

    bdirty(inode->buf, true); // mark dirty

    inode->desc->mode = 0777 & (~task->umask);
    inode->desc->uid = task->uid;
    inode->desc->gid = task->gid;
    inode->desc->size = 0;
    inode->desc->mtime = sys_time();
    inode->desc->nlinks = 1;
    // clear zones
    memset(inode->desc->zones, 0, sizeof(inode->desc->zones));

    inode->atime = inode->ctime = inode->desc->mtime;

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


int inode_read(inode_t *inode, char *buf, u32 len, off_t offset) {
    assert(ISFILE(inode->desc->mode) || ISDIR(inode->desc->mode));

    if (offset >= inode->desc->size)
        return EOF;
    
    u32 begin = offset;     // 记录初始偏移
    
    // file size limit
    u32 left = MIN(len, inode->desc->size - offset);

    while (left) {
        // exp. offset = 1500, BLOCK_SIZE = 1024 start = 1500 % 1024 = 476
        u32 start = offset % BLOCK_SIZE; // 块内偏移
        u32 chars = MIN(BLOCK_SIZE - start, left);

        // offset / block size 算出逻辑块号
        // nr 获取物理块号
        idx_t nr = bmap(inode, offset / BLOCK_SIZE, false);
        if (nr == 0) {
            // fill 0
            memset(buf, 0, chars);  // hole block
            buf += chars;
            offset += chars;        // file offset++
            left -= chars;          // left--
            continue;
        }

        //[IO]
        buffer_t *bf = bread(inode->dev, nr);

        // update
        offset += chars;    // file offset++
        left -= chars;      // left--

        char *ptr = bf->data + start;
        memcpy(buf, ptr, chars);

        buf += chars;

        brelse(bf);
    }

    inode->atime = sys_time();  // update access time
    return offset - begin;  // 实际读取字节数
}


int inode_write(inode_t *inode, char *buf, u32 len, off_t offset) {
    assert(ISFILE(inode->desc->mode));

    u32 begin = offset;     // 记录初始偏移
    u32 left = len;

    while (left) {
        idx_t nr = bmap(inode, offset / BLOCK_SIZE, true);
        assert(nr);

        // [RMW] 先读后写
        buffer_t *bf = bread(inode->dev, nr);
        bdirty(bf, true); // 标记脏块

        u32 start = offset % BLOCK_SIZE; // 块内偏移
        char *ptr = bf->data + start;
        u32 chars = MIN(BLOCK_SIZE - start, left);

        offset += chars;    // file offset++
        left -= chars;      // left--

        // [Expansion]
        if (offset > inode->desc->size) {
            inode->desc->size = offset;
            bdirty(inode->buf, true);
        }

        memcpy(ptr, buf, chars);

        buf += chars;

        brelse(bf);
    }

    inode->desc->mtime = inode->atime = sys_time(); // update modify & access time

    // bwrite(inode->buf);

    return offset - begin;  // 实际写入字节数
}


/**
 * @param inode: 文件 inode
 * @param array: 当前层级的块号数组（可能是 inode->zones，也可能是某个间接块里的数据）
 * @param index: 要释放的块在数组中的下标
 * @param level: 当前层级（0=数据块, 1=一级间接, 2=二级间接）
*/
static void inode_bfree(inode_t *inode, u16 *array, int index, int level) {
    // 空快
    if (!array[index])
        return;

    // level 0
    if (!level) {
        bfree(inode->dev, array[index]);
        return;
    }

    // level > 0
    // A. 先把这个索引块读到内存里
    buffer_t *buf = bread(inode->dev, array[index]);

    // B. 遍历索引块里的每一个条目 (Minix 1块1024字节 / 2字节u16 = 512个条目)
    for (size_t i = 0; i < BLOCK_INDEXES; i++) {
        // C. 递归释放下一级
        inode_bfree(inode, (u16 *)buf->data, i, level - 1);
    }

    // D. 释放缓冲区
    brelse(buf);
    // E. 释放当前索引块
    bfree(inode->dev, array[index]);
}


void inode_truncate(inode_t *inode)
{   
    if (!ISFILE(inode->desc->mode) && !ISDIR(inode->desc->mode)) {
        return;
    }

    // 2. 释放直接块 (Level 0)
    // 遍历 inode->zones[0] 到 zones[6]
    for (size_t i = 0; i < DIRECT_BLOCK; i++) {
        inode_bfree(inode, inode->desc->zones, i, 0); // Level 0
        inode->desc->zones[i] = 0; // 内存里的指针清零
    }

    // 3. 释放一级间接块 (Level 1)
    // 处理 inode->zones[7]
    inode_bfree(inode, inode->desc->zones, DIRECT_BLOCK, 1); // Level 1
    inode->desc->zones[DIRECT_BLOCK] = 0;

    // 4. 释放二级间接块 (Level 2)
    // 处理 inode->zones[8]
    inode_bfree(inode, inode->desc->zones, DIRECT_BLOCK + 1, 2); // Level 2
    inode->desc->zones[DIRECT_BLOCK + 1] = 0;

    // 5. 更新元数据
    inode->desc->size = 0;      // 文件大小变 0
    bdirty(inode->buf, true);   // 标记 inode 脏，等待写回
    inode->desc->mtime = sys_time();// 更新修改时间
    
    // 6. 强制写回磁盘（持久化修改）-> 延迟写回
    // bwrite(inode->buf);
}