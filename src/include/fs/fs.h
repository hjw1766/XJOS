#ifndef XJOS_FS_H
#define XJOS_FS_H

#include <xjos/types.h>
#include <xjos/list.h>
#include <fs/stat.h>

#define MAXNAMELEN 64
#define MAX_PATH_LEN 1024

enum file_flag {
    O_RDONLY = 00,              // read only
    O_WRONLY = 01,              // write only
    O_RDWR = 02,                // read and write
    O_ACCMODE = 03,             // mask for above modes
    O_CREAT = 00100,            // create file if it does not exist
    O_EXCL = 00200,             // exclusive use flag
    O_NOCTTY = 00400,           // do not assign controlling terminal
    O_TRUNC = 01000,            // file exists (write): truncate it
    O_APPEND = 02000,           // add to the end of file
    O_NONBLOCK = 04000,         // non-blocking mode
};

typedef struct inode_desc_t {
    u16 mode;       // file type and attr(rwx bits)
    u16 uid;        // owner user id
    u32 size;       // file size in bytes
    u32 mtime;      // access time (UTC)
    u8 gid;         // group id
    u8 nlinks;      // * number of links
    u16 zones[9];   // * block numbers (0-6 direct, 7 indirect, 8 double indirect)
} inode_desc_t;

enum {
    FS_TYPE_NONE = 0,
    FS_TYPE_PIPE,
    FS_TYPE_SOCKET,
    FS_TYPE_MINIX,
    FS_TYPE_NUM,
};

typedef struct inode_t {
    list_node_t node;  // list node

    void *desc;

    union {
        struct buffer_t *buf;   // inode 描述符对应buf
        void *addr;             // pipe 缓冲地址
    };

    dev_t dev;  // 设备号
    dev_t rdev; // 虚拟设备号

    idx_t nr;     // i 节点号
    size_t count; // 引用计数

    time_t atime; // 访问时间
    time_t mtime; // 修改时间
    time_t ctime; // 创建时间

    dev_t mount; // 安装设备

    mode_t mode; // 文件模式
    size_t size; // 文件大小
    int type;    // 文件系统类型

    int uid; // 用户 id
    int gid; // 组 id

    struct super_t *super;   // 超级块
    struct fs_op_t *op;      // 文件系统操作

    struct task_t *rxwaiter;    // read wait process
    struct task_t *txwaiter;    // write wait process
} inode_t;

typedef struct super_t {
    void *desc;           // 超级块描述符
    struct buffer_t *buf; // 超级块描述符 buffer
    dev_t dev;            // 设备号
    u32 count;            // 引用计数
    int type;             // 文件系统类型
    size_t sector_size;   // 扇区大小
    size_t block_size;    // 块大小
    list_t inode_list;    // 使用中 inode 链表
    inode_t *iroot;       // 根目录 inode
    inode_t *imount;      // 安装到的 inode} super_block_t;
} super_t;

typedef struct dentry_t {
    idx_t nr;         // inode number
    u32 length;       // 目录长度
    u32 namelen;      // 文件名长度
    char name[MAXNAMELEN]; // file name
} dentry_t;

typedef struct dcache_entry_t {
    list_node_t hnode; // hash table node
    list_node_t lru_node;   // lru list node
    idx_t nr;
    // struct inode_t *dir;  dont need dir pointer

    dev_t dev;    // parent device number
    idx_t p_nr;  // parent inode number

    char name[MAXNAMELEN + 1];
    u32 hash;        // name hash value
} dcache_entry_t;

typedef struct file_t {
    inode_t *inode;     // file inode
    u32 count;          // reference count
    off_t offset;       // file offset
    int flags;          // file flag
} file_t;

typedef dentry_t dirent_t;

typedef enum whence_t {
    SEEK_SET = 1,  // 直接设置偏移
    SEEK_CUR,      // 当前位置偏移
    SEEK_END       // 结束位置偏移
} whence_t;

typedef struct fs_op_t {
    int (*mkfs)(dev_t dev, int args);

    int (*super)(dev_t dev, super_t *super);

    int (*open)(inode_t *dir, char *name, int flags, int mode, inode_t **result);
    void (*close)(inode_t *inode);

    int (*read)(inode_t *inode, char *data, int len, off_t offset);
    int (*write)(inode_t *inode, char *data, int len, off_t offset);
    int (*truncate)(inode_t *inode);

    int (*stat)(inode_t *inode, stat_t *stat);
    int (*permission)(inode_t *inode, int mask);

    int (*namei)(inode_t *dir, char *name, char **next, inode_t **result);
    int (*mkdir)(inode_t *dir, char *name, int mode);
    int (*rmdir)(inode_t *dir, char *name);
    int (*link)(inode_t *odir, char *oldname, inode_t *ndir, char *newname);
    int (*unlink)(inode_t *dir, char *name);
    int (*mknod)(inode_t *dir, char *name, int mode, int dev);
    int (*readdir)(inode_t *inode, dentry_t *entry, size_t count, off_t offset);
} fs_op_t;

err_t fd_check(fd_t fd, file_t **file);
fd_t fd_get(file_t **file);
err_t fd_put(fd_t fd);

int fs_default_nosys();  // 未实现的系统调用
void *fs_default_null(); // 返回空指针

super_t *get_super(dev_t dev);  // 获得 dev 对应的超级块
super_t *read_super(dev_t dev); // 读取 dev 对应的超级块
void put_super(super_t *sb);

inode_t *get_root_inode(); // 获取根目录 inode
void iput(inode_t *inode); // 释放 inode
inode_t *find_inode(dev_t dev, idx_t nr);
inode_t *fit_inode(inode_t *inode);

super_t *get_free_super();

void dcache_init();
idx_t dcache_lookup(struct inode_t *dir, const char *name, size_t len);
void dcache_add(struct inode_t *dir, const char *name, size_t len, idx_t nr);

inode_t *named(char *pathname, char **next); // get pathname parent dir inode
inode_t *namei(char *pathname);              // get pathname inode

fs_op_t *fs_get_op(int type);
void fs_register_op(int type, fs_op_t *op);

inode_t *get_free_inode();
void put_free_inode(inode_t *inode);

file_t *get_file();
void put_file(file_t *file);

#define P_EXEC IXOTH
#define P_READ IROTH
#define P_WRITE IWOTH

bool match_name(const char *name, const char *entry_name, char **next);

#endif // XJOS_FS_H
