#include <fs/fs.h>
#include <fs/buffer.h>
#include <fs/stat.h>
#include <xjos/string.h>
#include <xjos/task.h>
#include <xjos/assert.h>
#include <xjos/debug.h>


#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

extern time_t sys_time();

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
    size_t copy_len = len > MAXNAMELEN ? MAXNAMELEN : len;
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


bool match_name(const char *name, const char *entry_name, char **next) {
    char *lhs = (char *)name;
    char *rhs = (char *)entry_name;
    while (*lhs == *rhs && *lhs != EOS && *rhs != EOS)
    {
        lhs++;
        rhs++;
    }
    if (*rhs)
        return false;
    if (*lhs && !IS_SEPARATOR(*lhs))
        return false;
    if (IS_SEPARATOR(*lhs))
        lhs++;
    *next = lhs;
    return true;
}


// name length
static int minix_name_len(const char * name) {
    int len = 0;
    while (len < MAXNAMELEN && name[len] != EOS) {
        len++;
    }
    return len;
}



// get pathname parent dir inode
inode_t *named(char *pathname, char **next) {
    inode_t *dir = NULL;
    task_t *task = running_task();
    char *name = pathname;
    if (IS_SEPARATOR(name[0])) {
        dir = task->iroot;
        dir->count++;
        name++; // skip '/'
    } else if (name[0]) {
        dir = task->ipwd;
        dir->count++;
    }
    else
        return NULL;

    assert(dir->dev > 0);

    if (!*name)     // "/" or "."
    {
        *next = name;
        return dir;
    }

    char *right = strrsep(name);
    if (!right || right < name) {
        *next = name;
        return dir;  // single component (/home) 
    }

    right++; // skip separator
    
    // [CHANGE] 移除这里的 dentry_t 和 buffer_t 定义，因为 dir_lookup 不返回这些
    // dentry_t *entry = NULL;
    // buffer_t *buf = NULL;

    inode_t *inode = NULL;
    int ret = EOK;

    while (true) {
        if (match_name(name, "..", next) &&
            dir == dir->super->iroot &&
            dir->super->imount) {
            super_t *super = dir->super;
            inode = super->imount;
            inode->count++;
            iput(dir);
            dir = inode;

            char *dummy = NULL;
            ret = dir->op->namei(dir, "..", &dummy, &inode);
            if (ret < EOK)
                goto rollback;

            iput(dir);
            dir = inode;

            if (right == *next) {
                return dir;
            }
            name = *next;
            continue;
        }

        ret = dir->op->namei(dir, name, next, &inode);
        if (ret < EOK)
            goto rollback;

        iput(dir);
        dir = inode;

        if (!ISDIR(dir->mode) || !dir->op->permission(dir, P_EXEC))
            goto rollback;

        if (right == *next) {
            return dir;
        }
        name = *next;
    }

rollback:
    iput(dir);
    return NULL;
}


inode_t *namei(char *pathname) {
    char *next = NULL;
    inode_t *dir = named(pathname, &next);
    if (!dir)
        return NULL;

    if (!(*next))
        return dir;

    char *name = next;
    inode_t *inode = NULL;

    if (match_name(name, "..", &next) &&
        dir == dir->super->iroot &&
        dir->super->imount) {
        super_t *super = dir->super;
        inode = super->imount;
        inode->count++;
        iput(dir);
        dir = inode;

        char *dummy = NULL;
        int ret = dir->op->namei(dir, "..", &dummy, &inode);
        iput(dir);
        if (ret < EOK)
            return NULL;
        return inode;
    }

    int ret = dir->op->namei(dir, name, &next, &inode);

    iput(dir);
    return inode;
}
