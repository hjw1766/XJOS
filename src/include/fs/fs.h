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
#define BLOCK_INODES (BLOCK_SIZE / sizeof(inode_desc_t)) // block inode number
#define BLOCK_DENTRIES (BLOCK_SIZE / sizeof(dentry_t)) // block dir entries number
#define BLOCK_INDEXES (BLOCK_SIZE / sizeof(u16)) // block index number

#define DIRECT_BLOCK (7)                    // direct block numbers in inode
#define INDIRECT1_BLOCK (BLOCK_INDEXES)      // indirect1 block numbers in inode
#define INDIRECT2_BLOCK (BLOCK_INDEXES * BLOCK_INDEXES) // indirect2 block numbers in inode
#define TOTAL_BLOCK (DIRECT_BLOCK + INDIRECT1_BLOCK + INDIRECT2_BLOCK) // total block numbers in inode

#define SEPARATOR1 '/'            // directory separator
#define SEPARATOR2 '\\'           
#define IS_SEPARATOR(c) (c == SEPARATOR1 || c == SEPARATOR2)

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
    time_t ctime;        // modify time
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

typedef struct dcache_entry_t {
    list_node_t hnode; // hash table node
    list_node_t lru_node;   // lru list node
    idx_t nr;
    struct inode_t *dir;    // parent directory inode

    char name[NAME_LEN + 1];
    u32 hash;        // name hash value
} dcache_entry_t;

// dev contains super block
super_block_t *get_super(dev_t dev);
super_block_t *read_super(dev_t dev);

idx_t balloc(dev_t dev);
void bfree(dev_t dev, idx_t idx);
idx_t ialloc(dev_t dev);
void ifree(dev_t dev, idx_t idx);

idx_t bmap(inode_t *inode, idx_t block, bool create);

inode_t *get_root_inode();
inode_t *iget(dev_t dev, idx_t nr);
void iput(inode_t *inode);

void dcache_init();
idx_t dcache_lookup(struct inode_t *dir, const char *name, size_t len);
void dcache_add(struct inode_t *dir, const char *name, size_t len, idx_t nr);

inode_t *named(char *pathname, char **next); // get pathname parent dir inode
inode_t *namei(char *pathname);              // get pathname inode

// in inode offset read/write len bytes -> buf
int inode_read(inode_t *inode, char *buf, u32 len, off_t offset);
int inode_write(inode_t *inode, char *buf, u32 len, off_t offset);

// free all data blocks of inode
void inode_truncate(inode_t *inode);

#endif // XJOS_FS_H