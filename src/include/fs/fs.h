#ifndef XJOS_FS_H
#define XJOS_FS_H

#include <xjos/types.h>
#include <xjos/list.h>

#define BLOCK_SIZE 1024 // block size in bytes
#define SECTOR_SIZE 512

#define MINIX1_MAGIC 0x137F
#define NAME_LEN 14 // max length of file name

#define IMAP_NR 8   // inode map blocks number max
#define ZMAP_NR 8   // zone map blocks number max

#define BLOCK_BITS (BLOCK_SIZE * 8) // block map bits number

typedef struct inode_desc_t {
    u16 mode;       // file type and attr(rwx bits)
    u16 uid;        // owner user id
    u32 size;       // file size in bytes
    u32 mtime;      // access time (UTC)
    u8 gid;         // group id
    u8 nlinks;      // * number of links
    u16 zones[9];   // * block numbers (0-6 direct, 7 indirect, 8 double indirect)
} inode_desc_t;

typedef struct inode_t {
    inode_desc_t *desc;     // pointer to inode descriptor
    struct buffer_t *buf;   // pointer to buffer containing inode 
    dev_t dev;           // device number
    idx_t nr;           // inode number
    u32 count;          // reference count
    time_t atime;        // access time
    time_t ctime;        // creation time
    list_node_t node;    // list node
    dev_t mount;        // install device
} inode_t;

typedef struct super_desc_t {
    u16 inodes;         // total number of inodes
    u16 zones;          // logical blocks
    u16 imap_blocks;    // (i node)number of inode map blocks
    u16 zmap_blocks;    // (z blk)number of zone map blocks
    u16 firstdatazone;  // number of first data zone
    u16 long_zone_size; // log2 of blocks per zone
    u32 max_size;       // maximum file size
    u16 magic;          // magic number
} super_desc_t;

typedef struct super_block_t {
    super_desc_t *desc;         // pointer to super block descriptor
    struct buffer_t *buf;       // pointer to buffer containing super block
    struct buffer_t *imaps[IMAP_NR]; // inode map buffers
    struct buffer_t *zmaps[ZMAP_NR]; // zone map buffers
    dev_t dev;             // device number
    list_t inode_list;     // list of inodes (useing)
    inode_t *iroot;         // root inode
    inode_t *imount;        // mount inode
} super_block_t;
 
typedef struct dentry_t {
    u16 nr;         // inode number
    char name[NAME_LEN]; // file name
} dentry_t;

// dev contains super block
super_block_t *get_super(dev_t dev);
super_block_t *read_super(dev_t dev);

idx_t balloc(dev_t dev);
void bfree(dev_t dev, idx_t idx);
idx_t ialloc(dev_t dev);
void ifree(dev_t dev, idx_t idx);

#endif // XJOS_FS_H