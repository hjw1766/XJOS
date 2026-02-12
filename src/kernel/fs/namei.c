#include <fs/fs.h>
#include <fs/buffer.h>
#include <fs/stat.h>
#include <xjos/string.h>
#include <xjos/task.h>
#include <xjos/assert.h>
#include <xjos/debug.h>


#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

extern time_t sys_time();


bool permission(inode_t *inode, u16 mask) {
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
        
        if (*ptr++ == EOS)
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
        entry->dev = 0;
        entry->p_nr = 0;
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
        if (entry->dev != dir->dev || entry->p_nr != dir->nr)
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

    // from tail (least recently used)
    list_node_t *node = list_popback(&dcache_lru_list);
    dcache_entry_t *entry = list_entry(node, dcache_entry_t, lru_node);

    if (entry->nr != 0)
        list_remove(&entry->hnode); // remove from hash table
    
    // 2. fill entry
    entry->nr = nr;
    entry->dev = dir->dev;
    entry->p_nr = dir->nr;
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


// del cache entry
void dcache_delete(inode_t *dir, const char *name, size_t len) {
    u32 hash = str_hash(name, len);
    u32 idx = hash % DCACHE_HASH_SIZE;

    list_t *head = &dcache_hash_table[idx];
    dcache_entry_t *entry;

    list_for_each_entry(entry, head, hnode) {
        if (entry->hash != hash)
            continue;
        if (entry->dev != dir->dev || entry->p_nr != dir->nr)
            continue;
        if (memcmp(entry->name, name, len) == 0 && entry->name[len] == EOS) {
            // Remove hash table and LRU list
            list_remove(&entry->hnode);
            list_remove(&entry->lru_node);

            // reset entry
            entry->dev = 0;
            entry->p_nr = 0;
            entry->nr = 0;
            // Add back to free list
            list_push(&dcache_lru_list, &entry->lru_node);
            return;
        }
    }
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

    /**
     * 1. 是否 ".." 目录
     * 2. 是否为根目录 (nr == 1)
     */
    if (match_name(name, "..", 2, next) && (*dir)->nr == 1) {
        super_block_t *sb = get_super((*dir)->dev);

        // 超级块有挂载点
        if (sb->imount) {
            inode_t *inode = *dir;

            (*dir) = sb->imount;
            (*dir)->count++;
            iput(inode); // release old dir inode
        }
    }

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
            if (entry->nr && match_name(name, entry->name, entry_len, next)) {
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
            bdirty(dir->buf, true); // 标记目录 inode 脏
        }

        if (entry->nr == 0) {
            // 找到空位了！写入数据
            
            // 安全复制 (处理 Minix 14字节 无\0 问题)
            // 必须把 name 区域清零再 copy，防止脏数据
            memset(entry->name, 0, NAME_LEN); 
            strlcpy(entry->name, name, NAME_LEN);
            
            dir->desc->mtime = sys_time(); // 更新目录修改时间
            bdirty(dir->buf, true); // 标记目录 inode 脏
            
            bdirty(buf, true); // 标记目录块为脏
            *result = entry;
            return buf;
        }

        entry++;
    }
}


// 高级查找：供外部通过路径获取 inode
idx_t dir_lookup(inode_t **dir, const char *name, size_t len) {
    // 1. 查缓存
    idx_t nr = dcache_lookup(*dir, name, len);
    if (nr != 0) return nr;

    // 2. 查磁盘
    dentry_t *entry = NULL;
    char *next = NULL;
    
    // 调用提取出来的 find_entry
    buffer_t *buf = find_entry(dir, name, &next, &entry);
    if (buf) {
        nr = entry->nr;
        
        // 确保匹配长度一致 (防止 find_entry 匹配了前缀)
        // 这里的校验是为了双重保险，因为 find_entry 的 next 逻辑
        int entry_len = minix_name_len(entry->name);
        if (entry_len == len) {
            dcache_add(*dir, name, len, nr);
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
    
    // [CHANGE] 移除这里的 dentry_t 和 buffer_t 定义，因为 dir_lookup 不返回这些
    // dentry_t *entry = NULL;
    // buffer_t *buf = NULL;

    while (true) {
        // [CHANGE] 原有逻辑删除：brelse(buf); buf = find_entry(...)
        
        // [NEW] 手动解析当前路径分量 (例如 "usr" from "usr/bin")
        char *p = left;
        while (*p && !IS_SEPARATOR(*p)) {
            p++;
        }
        size_t len = p - left;

        // [NEW] 使用 dir_lookup 替代 find_entry
        // dir_lookup 内部封装了 dcache 查找 -> 失败查盘 -> 自动填充 dcache
        idx_t nr = dir_lookup(&inode, left, len);
        
        if (nr == 0) // not found
            goto failure;
        
        // [CHANGE] 获取子目录 inode
        dev_t dev = inode->dev;
        iput(inode);    // release parent dir inode
        inode = iget(dev, nr);

        if (!ISDIR(inode->desc->mode) || !permission(inode, P_EXEC))
            goto failure;

        // [NEW] 更新指针位置，跳过分割符
        left = p;
        if (IS_SEPARATOR(*left)) {
            left++;
        }

        // [CHANGE] 检查是否到达终点 (right 指向的是最后一个分量之前的分隔符位置)
        // 注意：这里的比较逻辑需要根据 left 的移动进行调整
        // 当 left 跨过了 right 指向的分隔符，说明我们已经处理到了倒数第二个分量
        if (right <= left) {
            *next = left; // next 指向剩下的部分 (即文件名)
            goto success; 
        }
    }

success:
    // [CHANGE] 移除 brelse(buf)，dir_lookup 已经处理了
    return inode;

failure:
    // [CHANGE] 移除 brelse(buf)
    iput(inode);
    return NULL;
}


static bool is_empty(inode_t *inode) {
    assert(ISDIR(inode->desc->mode));

    int entries = inode->desc->size / sizeof(dentry_t);

    if (entries < 2 || !inode->desc->zones[0]) {
        LOGK("bad directory on dev %d\n", inode->dev);
        return false;
    }

    idx_t i = 0;
    idx_t block = 0;
    buffer_t *buf = NULL;
    dentry_t *entry;
    int count = 0;

    for (; i < entries; i++) {
        // 跨块处理
        if (!buf || (u32)entry >= (u32)buf->data + BLOCK_SIZE) {
            if (buf) brelse(buf);

            block = bmap(inode, i / BLOCK_DENTRIES, false);
            if (block == 0) { // 稀疏文件空洞
                i += BLOCK_DENTRIES - 1;
                entry = NULL;
                continue;
            }
            
            buf = bread(inode->dev, block);
            entry = (dentry_t *)buf->data;
        }
        
        // 统计非空目录项, >2 直接判断非空
        if (entry->nr) {
            count++;
            if (count > 2) {
                if (buf) brelse(buf);
                return false;
            }
        }
        entry++;
    }

    if (buf) brelse(buf);

    if (count < 2) {
        LOGK("bad directory on dev %d\n", inode->dev);
        return false;
    }

    return count == 2; // only . and ..
}


inode_t *namei(char *pathname) {
    char *next = NULL;
    inode_t *dir = named(pathname, &next);
    if (!dir)
        return NULL;
    if (!(*next))
        return dir; // exact match '/'
    
    char *name = next;
    // [CHANGE] 移除 buffer_t *buf = find_entry(...)
    
    // [NEW] 计算文件名长度
    char *p = name;
    while (*p && !IS_SEPARATOR(*p)) p++;
    size_t len = p - name;

    // [NEW] 使用 dir_lookup 查找最终目标
    // 这样最后的文件名查找也会走缓存！
    idx_t nr = dir_lookup(&dir, name, len);
    
    if (nr == 0) {     // not found
        iput(dir);
        return NULL;
    }

    inode_t *inode = iget(dir->dev, nr);

    iput(dir);
    // [CHANGE] 移除 brelse(buf)

    return inode;
}


static _inline size_t path_len(const char *path) {
    size_t len = 0;
    while (path[len] && !IS_SEPARATOR(path[len])) {
        len++;
    }
    return len;
}


int sys_mkdir(char *pathname, mode_t mode) {
    char *next = NULL;
    buffer_t *ebuf = NULL;

    // parse path: return parent dir inode, next -> filename
    inode_t *dir = named(pathname, &next);
    inode_t *inode = NULL;
    int ret = EOF;

    // 1. base checks
    // parent dir not found
    if (!dir)
        goto rollback;
    // path/to/(empty)
    if (!*next)
        goto rollback;
    
    if (!permission(dir, P_WRITE))
        goto rollback;

    // 2. caclulate name length
    char *name = next;
        
    // 3. check if file exists
    if (dir_lookup(&dir, name, path_len(name)) != 0) {
        LOGK("mkdir: cannot create directory \"%s\": File exists\n", name);
        goto rollback;
    }

    // 4. add new entry
    dentry_t *entry = NULL;
    ebuf = add_entry(dir, name, &entry);
    if (!ebuf)
        goto rollback;
    
    // 5. new inode
    idx_t nr = ialloc(dir->dev);
    if (nr == 0)
        goto rollback;
    entry->nr = nr;
    bdirty(ebuf, true);
    // 6. init inode
    task_t *task = running_task();
    inode = new_inode(dir->dev, entry->nr);
    if (!inode) {
        ifree(dir->dev, nr);
        goto rollback;
    }


    inode->desc->mode = (mode & 0777 & ~task->umask) | IFDIR;
    inode->desc->nlinks = 2; // . and ..
    inode->desc->size = 2 * sizeof(dentry_t); // . and ..
    bdirty(inode->buf, true);

    // 7. write new dir entries: . and ..
    buffer_t *zbuf = bread(inode->dev, bmap(inode, 0, true));
    memset(zbuf->data, 0, BLOCK_SIZE);

    dentry_t *zentry = (dentry_t *)zbuf->data;

    memset(zentry->name, 0, NAME_LEN);
    strcpy(zentry->name, "."); // current dir
    zentry->nr = inode->nr;
    zentry++;

    memset(zentry->name, 0, NAME_LEN);
    strcpy(zentry->name, ".."); // parent dir
    zentry->nr = dir->nr;

    bdirty(zbuf, true);
    brelse(zbuf);    // sync at the end

    // 8. update parent dir link count
    dir->desc->nlinks++;
    bdirty(dir->buf, true);
    
    // 9. update Dcache
    dcache_add(dir, name, path_len(name), nr);
    
    ret = 0; // success

rollback:
    brelse(ebuf);
    iput(dir);
    iput(inode);
    return ret;
}


int sys_rmdir(char *pathname) {
    char *next = NULL;
    buffer_t *ebuf = NULL;
    inode_t *dir = named(pathname, &next); // parent dir
    inode_t *inode = NULL;
    int ret = EOF;

    // 1. base checks
    if (!dir) goto rollback;
    if (!*next) goto rollback;
    if (!permission(dir, P_WRITE)) goto rollback;

    // Calculate name length for dcache_delete later
    char *name = next;
    size_t name_len = path_len(name);   // invoke find_entry before cal len

    // 2. find target dentry
    // Must use find_entry (not dir_lookup) because we need 'ebuf' to modify it
    dentry_t *entry;
    ebuf = find_entry(&dir, name, &next, &entry);
    if (!ebuf) goto rollback; // not found

    // 3. get target inode
    inode = iget(dir->dev, entry->nr);
    if (!inode) goto rollback;

    // 4. Validation
    if (inode == dir) goto rollback;       // cannot remove '.'
    if (!ISDIR(inode->desc->mode)) goto rollback; // must be dir

    // Check sticky bit (optional, based on your logic)
    task_t *task = running_task();
    if ((dir->desc->mode & ISVTX) && task->uid != inode->desc->uid)
        goto rollback;

    if (dir->dev != inode->dev || inode->count > 1)
        goto rollback; // mount point or busy

    // 5. check if empty
    if (!is_empty(inode))
        goto rollback;

    // 6. Execution
    assert(inode->desc->nlinks == 2);

    inode_truncate(inode);
    ifree(inode->dev, inode->nr);

    inode->desc->nlinks = 0;
    bdirty(inode->buf, true);
    inode->nr = 0;

    // update parent
    dir->desc->nlinks--; // remove '..' count
    dir->ctime = dir->atime = dir->desc->mtime = sys_time();
    bdirty(dir->buf, true);
    assert(dir->desc->nlinks > 0);

    // clear entry in parent dir
    entry->nr = 0;
    bdirty(ebuf, true);

    // 7. Clear Dcache (Important!)
    dcache_delete(dir, name, name_len);

    ret = 0;

rollback:
    iput(inode);
    iput(dir);
    brelse(ebuf);
    return ret;
}


int sys_link(char *oldname, char *newname) {
    int ret = EOF;
    buffer_t *buf = NULL;
    inode_t *dir = NULL;
    inode_t *inode = namei(oldname);
    if (!inode)
        goto rollback;

    if (ISDIR(inode->desc->mode))
        goto rollback;

    char *next = NULL;
    dir = named(newname, &next);    // parent dir
    if (!dir)       // 
        goto rollback;
    if (!*next)     // empty newname
        goto rollback;
    if (dir->dev != inode->dev)
        goto rollback; // cross-device link
    if (!permission(dir, P_WRITE))
        goto rollback;

    char *name = next;
    
    // [opt.] dir_lookup
    if (dir_lookup(&dir, name, path_len(name)) != 0)
        goto rollback;

    dentry_t *entry;
    // add new entry
    buf = add_entry(dir, name, &entry);
    entry->nr = inode->nr;      // point to the same inode
    bdirty(buf, true);

    inode->desc->nlinks++;      // increase link count
    inode->ctime = sys_time();
    bdirty(inode->buf, true);
    ret = 0;

    // update Dcache
    dcache_add(dir, name, path_len(name), inode->nr);

rollback:
    brelse(buf);
    iput(inode);
    iput(dir);
    return ret;
}


int sys_unlink(char *filename) {
    int ret = EOF;
    char *next = NULL;
    buffer_t *ebuf = NULL;
    inode_t *inode = NULL;

    inode_t *dir = named(filename, &next); // parent dir
    if (!dir) goto rollback;
    if (!*next) goto rollback;
    if (!permission(dir, P_WRITE)) goto rollback;

    char *name = next;

    // find target dentry
    dentry_t *entry;
    ebuf = find_entry(&dir, name, &next, &entry);
    if (!ebuf) goto rollback; // not found

    inode = iget(dir->dev, entry->nr);
    if (!inode) goto rollback;

    // check, dont use unlink delete dir(should use rmdir)
    if (ISDIR(inode->desc->mode)) {
        LOGK("unlink: cannot unlink directory \"%s\"\n", name);
        goto rollback;
    }

    task_t *task = running_task();
    if ((dir->desc->mode & ISVTX) && task->uid != inode->desc->uid && task->uid != KERNEL_USER)
        goto rollback;
    
    // execute unlink
    entry->nr = 0;
    bdirty(ebuf, true);

    inode->desc->nlinks--;
    inode->ctime = sys_time();
    bdirty(inode->buf, true);

    // if link count == 0, free inode
    if (inode->desc->nlinks == 0) {
        inode_truncate(inode);  // 物理块
        ifree(inode->dev, inode->nr);   // inode
    }

    // sync dcache
    dcache_delete(dir, name, path_len(name));

    ret = 0;


rollback:
    brelse(ebuf);
    iput(dir);
    iput(inode);
    return ret;
}


inode_t *inode_open(char *pathname, int flag, int mode) {
    inode_t *dir = NULL;
    inode_t *inode = NULL;
    buffer_t *buf = NULL;
    dentry_t *entry = NULL;
    char *next = NULL;

    dir = named(pathname, &next); // parent dir
    if (!dir)
        goto rollback;
    if (!*next)   // exact match
        return dir;
    
    if ((flag & O_TRUNC) && ((flag & O_ACCMODE) == O_RDONLY))
        goto rollback; // cannot truncate read-only file
    
    char *name = next;
    // [opt.] use dir_lookup
    idx_t nr = dir_lookup(&dir, name, path_len(name));
    if (nr != 0) {
        // file exists
        inode = iget(dir->dev, nr);
        if (!inode)
            goto rollback;
        goto makeup;
    }

    // file not exists
    if (!(flag & O_CREAT))
        goto rollback; // not create flag
    if (!permission(dir, P_WRITE))
        goto rollback; // no write permission

    // add new entry
    buf = add_entry(dir, name, &entry);
    if (!buf)
        goto rollback;

    // 从inode位图中分配新inode，然后再拿到物理块号
    entry->nr = ialloc(dir->dev);   // * ialloc may fail?
    if (entry->nr == 0)
        goto rollback;

    bdirty(buf, true);

    inode = new_inode(dir->dev, entry->nr);
    if (!inode) {
        ifree(dir->dev, entry->nr);
        goto rollback;
    }

    
    task_t *task = running_task();
    mode &= (0777 & ~task->umask);
    mode |= IFREG; // regular file
    inode->desc->mode = mode;
    bdirty(inode->buf, true);

    dcache_add(dir, name, path_len(name), inode->nr);

makeup:
    // convert flag to permission mask
    int acc_mode = flag & O_ACCMODE;
    u16 mask = 0;
    if (acc_mode == O_RDONLY) mask = P_READ;
    else if (acc_mode == O_WRONLY) mask = P_WRITE;
    else if (acc_mode == O_RDWR) mask = P_READ | P_WRITE;

    if (!permission(inode, mask))
        goto rollback; // no permission
    
    if (ISDIR(inode->desc->mode) && (flag & O_ACCMODE) != O_RDONLY)
        goto rollback; // open dir in read-only mode
    
    inode->atime = sys_time();

    if (flag & O_TRUNC)
        inode_truncate(inode);

    brelse(buf);
    iput(dir);
    return inode;

rollback:
    brelse(buf);
    iput(dir);
    iput(inode);
    return NULL;
}


char *sys_getcwd(char *buf, size_t size) {
    task_t *task = running_task();
    strlcpy(buf, task->pwd, size);

    size_t len = strlen(buf);
    if (len > 1 && buf[len - 1] == '/')
        buf[len - 1] = '\0'; // remove trailing '/'

    return buf;
}


void abspath(char *pwd, const char *pathname) {
    char *cur = NULL;
    char *ptr = NULL;
    if (IS_SEPARATOR(pathname[0])) {
        cur = pwd + 1; // skip '/'
        *cur = 0;   // truncate
        pathname++; // skip '/'
    } else {
        // exp. pwd = /usr/bin/, cur -> l = '\0'
        // pwd = /usr/'\0'in
        cur = strrsep(pwd) + 1; // skip to end '/'
        *cur = 0;   // truncate
    }

    while (pathname[0]) {
        ptr = strsep(pathname);
        if (!ptr)
            break;
        
        // 第一个/之前的部分
        int len = (ptr - pathname) + 1;
        *ptr = '/';

        if (!memcmp(pathname, "./", 2)) {
            // skip
        } else if (!memcmp(pathname, "../", 3)) {
            // back to parent dir

            // cur - 1 = '/', dont delate root '/'
            if (cur - 1 != pwd) {
                //exp. pwd = /home/usr/, cur -> /' '
                *(cur - 1) = 0; //pwd = /home/usr'\0'
                
                // cur -> 'u'
                cur = strrsep(pwd) + 1; // move cur to end

                // pwd = /home/'\0'
                *cur = 0; // truncate
            }
        } else {
            // normal name

            //exp.  pathname = "bin/"
            // copy to pwd
            strlcpy(cur, pathname, len + 1);

            cur += len; // move cur
        }

        pathname += len; // move pathname
    }

    // while break, handle last part
    if (!pathname[0])
        return;
    if (!strcmp(pathname, "."))
        return;

    if (strcmp(pathname, "..")) {
        // dont ..
        strcpy(cur, pathname);

        cur += strlen(pathname);
        *cur = '/'; // append '/'
        return;
    }

    if (cur - 1 != pwd) {
        //back to parent dir
        *(cur - 1) = 0;
        cur = strrsep(pwd) + 1;
        *cur = 0;
    }
}


int sys_chdir(char *pathname) {
    task_t *task = running_task();
    inode_t *inode = namei(pathname);
    if (!inode)
        goto rollback;
    if (!ISDIR(inode->desc->mode))
        goto rollback;
    if (!permission(inode, P_EXEC))
        goto rollback;

    if (inode == task->ipwd) {  // same dir
        iput(inode);
        return 0;
    }

    // update pwd
    abspath(task->pwd, pathname);
    iput(task->ipwd);
    task->ipwd = inode;

    return 0;

rollback:
    iput(inode);
    return EOF;
}


int sys_chroot(char *pathname) {
    task_t *task = running_task();
    inode_t *inode = namei(pathname);
    if (!inode)
        goto rollback;
    if (!ISDIR(inode->desc->mode) || inode == task->iroot)
        goto rollback;
    if (!permission(inode, P_EXEC))
        goto rollback;

    if (inode == task->iroot) {  // same dir
        iput(inode);
        return 0;
    }

    iput(task->iroot);
    task->iroot = inode;    // change root inode

    return 0;

rollback:
    iput(inode);
    return EOF;    
}

int sys_mknod(char *filename, int mode, int dev) {
    char *next = NULL;
    inode_t *dir = NULL;
    buffer_t *buf = NULL;
    inode_t *inode = NULL;
    int ret = EOF;

    dir = named(filename, &next); // parent dir
    if (!dir)
        goto rollback;
    if (!*next)
        goto rollback;
    if (!permission(dir, P_WRITE))
        goto rollback;
    
    char *name = next;
    if (dir_lookup(&dir, name, path_len(name)) != 0)
        goto rollback; // file exists
    
    // add new entry
    dentry_t *entry = NULL;
    buf = add_entry(dir, name, &entry);
    if (!buf)
        goto rollback;
    
    // allocate new inode
    entry->nr = ialloc(dir->dev);
    if (entry->nr == 0)
        goto rollback;
        
    bdirty(buf, true);

    inode = new_inode(dir->dev, entry->nr);
    if (!inode) {
        ifree(dir->dev, entry->nr);
        goto rollback;
    }

    inode->desc->mode = mode;
    if (ISCHR(mode) || ISBLK(mode))
        inode->desc->zones[0] = dev; // store device number

    bdirty(inode->buf, true);

    dcache_add(dir, name, path_len(name), entry->nr);

    ret = 0;

rollback:
    brelse(buf);
    iput(dir);
    iput(inode);
    return ret;
}