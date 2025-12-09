#include <fs/fs.h>
#include <fs/buffer.h>
#include <fs/stat.h>
#include <xjos/syscall.h>
#include <libc/string.h>
#include <xjos/task.h>
#include <libc/assert.h>
#include <xjos/debug.h>


#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

#define P_EXEC IXOTH
#define P_READ IROTH
#define P_WRITE IWOTH


static bool permission(inode_t *inode, u16 mask) {
    u16 mode = inode->desc->mode;

    if (!inode->desc->nlinks)
        return false; // deleted file
    
    task_t *task = running_task();
    if (task->uid == KERNEL_USER)
        return true; // kernel has all permissions

    u16 perm;
    if (task->uid == inode->desc->uid)
        perm = (mode >> 6) & 0b111;  // Owner
    else if (task->gid == inode->desc->gid)
        perm = (mode >> 3) & 0b111;  // Group
    else
        perm = mode & 0b111;          // Other

    return (perm & mask) == mask;
}


// get first separator
char *strsep(const char *str) {
    char *ptr = (char *)str;
    while (true) {
        if (IS_SEPARATOR(*ptr))
            return ptr;
        
        if (*ptr == EOS)
            return NULL;
    }
}


char *strrsep(const char *str) {
    char *last = NULL;
    char *ptr = (char *)str;
    while (true) {
        if (IS_SEPARATOR(*ptr))
            last = ptr;
        if (*ptr++ == EOS)
            return last;
    }
}

/**
 * 哈希相关
 */
#define DCACHE_NR  128 // dcache entry number
#define DCACHE_HASH_SIZE  64 // dcache hash table size

static dcache_entry_t dcache_entries[DCACHE_NR];
static list_t dcache_hash_table[DCACHE_HASH_SIZE];
static list_t dcache_lru_list; // lru list

// DJB2
static u32 str_hash(const char *name, size_t len) {
    u32 hash = 5381;
    for (size_t i = 0; i < len; i++) {
        hash = ((hash << 5) + hash) + (u8)name[i]; // hash * 33 + c
    }
    return hash;
}

/**
 * Dcache kernel logic
 */
void dcache_init() {
    LOGK("dcache_init()\n");
    // Initialize hash table
    for (int i = 0; i < DCACHE_HASH_SIZE; i++) {
        list_init(&dcache_hash_table[i]);
    }

    list_init(&dcache_lru_list);

    for (int i = 0; i < DCACHE_NR; i++) {
        dcache_entry_t *entry = &dcache_entries[i];
        entry->dir = NULL;
        entry->nr = 0;
        list_push(&dcache_lru_list, &entry->lru_node);
    }    
}

// find cache entry
idx_t dcache_lookup(inode_t *dir, const char *name, size_t len) {
    u32 hash = str_hash(name, len);
    u32 idx = hash % DCACHE_HASH_SIZE;

    list_t *head = &dcache_hash_table[idx];
    dcache_entry_t *entry;

    list_for_each_entry(entry, head, hnode) {
        // 1. check hash
        if (entry->hash != hash)
            continue;
        // 2. check parent dir
        if (entry->dir != dir)
            continue;

        // 3. check name
        if (memcmp(entry->name, name, len) == 0 && entry->name[len] == EOS) {
            // Move to front of LRU list
            list_remove(&entry->lru_node);
            list_push(&dcache_lru_list, &entry->lru_node);

            return entry->nr;
        }
    }

    return 0; // miss
}

// add cache entry
void dcache_add(inode_t *dir, const char *name, size_t len, idx_t nr) {
    // 1. get free entry from LRU list
    if (list_empty(&dcache_lru_list)) {
        LOGK("dcache_add: no free entry\n");
        return;
    }

    // front is the most recently used
    list_node_t *node = list_pop(&dcache_lru_list);
    dcache_entry_t *entry = list_entry(node, dcache_entry_t, lru_node);

    if (entry->dir != NULL)
        list_remove(&entry->hnode); // remove from hash table
    
    // 2. fill entry
    entry->nr = nr;
    entry->dir = dir;
    entry->hash = str_hash(name, len);

    // 超出 14限制 截断
    size_t copy_len = len > NAME_LEN ? NAME_LEN : len;
    memcpy(entry->name, name, copy_len);
    entry->name[copy_len] = EOS;

    // 3. insert into hash table
    u32 idx = entry->hash % DCACHE_HASH_SIZE;
    list_push(&dcache_hash_table[idx], &entry->hnode);

    // 4. insert into lru list tail (most recently used)
    list_push(&dcache_lru_list, &entry->lru_node);
}


static bool match_name(const char *name, const char *entry_name, size_t entry_len, char **next) {
    // 1. memcmp asm 4byte compare
    if (memcmp(name, entry_name, entry_len) != 0)
        return false;

    // 2. Check for string end or separator
    /*
        usr/bin, next char is '/'
        usr, next char is '\0'
        usrx, next char is 'x' (not match)
    */
    char terminator = name[entry_len];
    if (terminator != EOS && !IS_SEPARATOR(terminator))
        return false;

    // 3. update pointer
    if (next) {
        /**
         * usr/bin  -> entry_len = 3 -> next = &name[4] ('bin')
         */
        if (IS_SEPARATOR(terminator)) {
            while (IS_SEPARATOR(name[entry_len]))
                entry_len++;
        }
        *next = (char *)name + entry_len;
    }
    return true;
}


// name length
static int minix_name_len(const char * name) {
    int len = 0;
    while (len < NAME_LEN && name[len] != EOS) {
        len++;
    }
    return len;
}


/**
 * 低级查找函数：返回包含 dentry 的 buffer
 * @param dir       父目录
 * @param name      路径名 (如 "a/b")
 * @param next      [OUT] 返回下级路径指针
 * @param result    [OUT] 返回找到的 dentry 指针
 */
static buffer_t *find_entry(inode_t **dir, const char *name, char **next, dentry_t **result) {
    // 确保是目录
    if (!ISDIR((*dir)->desc->mode)) 
        return NULL;

    u32 entries = (*dir)->desc->size / sizeof(dentry_t);
    idx_t block_idx = 0;
    buffer_t *buf = NULL;
    dentry_t *entry = NULL;

    for (u32 i = 0; i < entries; i++) {
        // 跨块处理
        if (!buf || (u32)entry >= (u32)buf->data + BLOCK_SIZE) {
            if (buf) brelse(buf);

            block_idx = bmap((*dir), i / BLOCK_DENTRIES, false);
            if (block_idx == 0) { // 稀疏文件空洞
                i += BLOCK_DENTRIES - 1;
                entry = NULL;
                continue;
            }

            buf = bread((*dir)->dev, block_idx);
            entry = (dentry_t *)buf->data;
        }

        // 匹配逻辑
        if (entry->nr != 0) {
            int entry_len = minix_name_len(entry->name);
            // 只有当磁盘里的文件名长度匹配，才进行后续比较
            if (match_name(name, entry->name, entry_len, next)) {
                *result = entry;
                return buf; // 返回 buf，持有锁
            }
        }
        entry++;
    }

    if (buf) brelse(buf);
    return NULL;
}


/**
 * 添加目录项
 * @param dir   父目录
 * @param name  新文件名
 * @param result [OUT] 返回新建的 dentry 指针
 */
buffer_t *add_entry(inode_t *dir, const char *name, dentry_t **result) {
    char *next = NULL;
    buffer_t *buf = NULL;

    // 1. 检查是否已存在 (复用 find_entry)
    // 注意：find_entry 期望 inode**, 我们取地址传进去
    buf = find_entry(&dir, name, &next, result);
    if (buf) {
        return buf; // 已存在，直接返回
    }

    // 2. 准备插入
    // 必须确保 name 里没有路径分隔符 (add_entry 只负责当前层)
    for (int i = 0; i < NAME_LEN && name[i]; i++) {
        assert(!IS_SEPARATOR(name[i]));
    }

    // 3. 扫描空位 (或者追加到末尾)
    u32 i = 0;
    dentry_t *entry = NULL;
    idx_t block_idx = 0;

    for (;; i++) {
        // 跨块逻辑 (含自动扩容)
        if (!buf || (u32)entry >= (u32)buf->data + BLOCK_SIZE) {
            if (buf) brelse(buf);

            // create=true: 如果 block 不存在，自动分配！
            block_idx = bmap(dir, i / BLOCK_DENTRIES, true); 
            assert(block_idx != 0);

            buf = bread(dir->dev, block_idx);
            entry = (dentry_t *)buf->data;
        }

        // 4. 找到空闲位 (nr == 0) 或者 到达了文件末尾
        if (i * sizeof(dentry_t) >= dir->desc->size) {
            // 到达末尾，扩展文件大小
            entry->nr = 0;
            dir->desc->size = (i + 1) * sizeof(dentry_t);
            dir->buf->dirty = true;
        }

        if (entry->nr == 0) {
            // 找到空位了！写入数据
            
            // 安全复制 (处理 Minix 14字节 无\0 问题)
            // 必须把 name 区域清零再 copy，防止脏数据
            memset(entry->name, 0, NAME_LEN); 
            strlcpy(entry->name, name, NAME_LEN);
            
            dir->desc->mtime = time(); // 更新目录修改时间
            dir->buf->dirty = true;
            
            buf->dirty = true; // 标记目录块为脏
            *result = entry;
            return buf;
        }

        entry++;
    }
}


// 高级查找：供外部通过路径获取 inode
idx_t dir_lookup(inode_t *dir, const char *name, size_t len) {
    // 1. 查缓存
    idx_t nr = dcache_lookup(dir, name, len);
    if (nr != 0) return nr;

    // 2. 查磁盘
    dentry_t *entry = NULL;
    char *next = NULL;
    
    // 调用提取出来的 find_entry
    buffer_t *buf = find_entry(&dir, name, &next, &entry);
    if (buf) {
        nr = entry->nr;
        
        // 确保匹配长度一致 (防止 find_entry 匹配了前缀)
        // 这里的校验是为了双重保险，因为 find_entry 的 next 逻辑
        int entry_len = minix_name_len(entry->name);
        if (entry_len == len) {
            dcache_add(dir, name, len, nr);
        }

        brelse(buf);
        return nr;
    }
    
    return 0;
}


// get pathname parent dir inode
inode_t *named(char *pathname, char **next) {
    inode_t *inode = NULL;
    task_t *task = running_task();
    char *left = pathname;
    if (IS_SEPARATOR(left[0])) {
        inode = task->iroot;
        left++; // skip '/'
    } else if (left[0])
        inode = task->ipwd;
    else
        return NULL;

    inode->count++;
    *next = left;

    if (!*left)     // "/" or "."
        return inode;

    char *right = strrsep(left);
    if (!right || right < left)
        return inode;  // single component (/home) 

    right++; // skip separator
    dentry_t *entry = NULL;
    buffer_t *buf = NULL;
    while (true) {
        brelse(buf);
        buf = find_entry(&inode, left, next, &entry);
        if (!buf)
            goto failure;
        
        dev_t dev = inode->dev;
        iput(inode);    // release parent dir inode
        inode = iget(dev, entry->nr);
        if (!ISDIR(inode->desc->mode) || !permission(inode, P_EXEC))
            goto failure;
        if (right == *next)
            goto success; // last component
        
        left = *next;
    }

success:
    brelse(buf);
    return inode;

failure:
    brelse(buf);
    iput(inode);
    return NULL;
}


inode_t *namei(char *pathname) {
    char *next = NULL;
    inode_t *dir = named(pathname, &next);
    if (!dir)
        return NULL;
    if (!(*next))
        return dir; // exact match '/'
    
    char *name = next;
    dentry_t *entry = NULL;
    buffer_t *buf = find_entry(&dir, name, &next, &entry);
    if (!buf) {     // not found
        iput(dir);
        return NULL;
    }

    inode_t *inode = iget(dir->dev, entry->nr);

    iput(dir);
    brelse(buf);

    return inode;
}

#include <xjos/memory.h>

void dir_test() {
    inode_t *inode = namei("/hello.txt");
    if (!inode) {
        return; 
    }

    char buf[1024];
    memset(buf, 'A', 1024);
    for (int i=0; i<8; i++) {
        inode_write(inode, buf, 1024, i*1024);
    }
    
    LOGK("Before truncate: size = %d\n", inode->desc->size);
    // 预期输出: 8192
    

    LOGK("Truncating file...\n");
    inode_truncate(inode);

    
    LOGK("After truncate: size = %d\n", inode->desc->size);
    // 预期输出: 0
    
    int bytes_read = inode_read(inode, buf, 1024, 0);
    LOGK("Read bytes after truncate: %d\n", bytes_read);
    // 预期输出: 0 (EOF)

    LOGK("Zone[0] after truncate = %d\n", inode->desc->zones[0]); 

    iput(inode);
}