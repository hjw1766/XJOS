#include <fs/fs.h>
#include <fs/stat.h>
#include <xjos/assert.h>
#include <xjos/task.h>
#include <drivers/device.h>
#include <fs/stat.h>


#define FILE_NR 128

file_t file_table[FILE_NR];


file_t *get_file() {
    for (size_t i = 0; i < FILE_NR; i++) {
        file_t *file = &file_table[i];
        if (!file->count) {
            file->count++;
            return file;
        }
    }

    panic("Exceed max open files\n");
}


void put_file(file_t *file)
 {  
    assert(file->count > 0);
    file->count--;
    if (!file->count) 
    {  
        iput(file->inode);
    }
}


void file_init() {
    for (size_t i = 0; i < FILE_NR; i++) {
        file_t *file = &file_table[i];
        file->mode = 0;
        file->count = 0;
        file->flags = 0;
        file->offset = 0;
        file->inode = NULL;
    }
}


fd_t sys_open(char *filename, int flags, int mode) {
    inode_t *inode = inode_open(filename, flags, mode);
    if (!inode)
        return EOF;

    task_t *task = running_task();
    fd_t fd = task_get_fd(task);
    if (fd == EOF) {
        iput(inode);
        return EOF;
    }

    file_t *file = get_file();
    assert(task->files[fd] == NULL);
    task->files[fd] = file;

    file->inode = inode;
    file->flags = flags;
    file->offset = 0;
    file->mode = inode->desc->mode;

    if (flags & O_APPEND) {
        file->offset = file->inode->desc->size; // file end
    }

    return fd;
}


int sys_create(char *filename, int mode) {
    return sys_open(filename, O_CREAT | O_TRUNC, mode);
}


int sys_read(fd_t fd, char *buf, int len) {
    // no stdin other files

    if (fd < 0 || fd >= TASK_FILE_NR)
        return EOF;

    task_t *task = running_task();
    file_t *file = task->files[fd];
    if (!file)
        return EOF;
    if (len <= 0)
        return EOF;
    int r_len = 0;

    if ((file->flags & O_ACCMODE) == O_WRONLY)
        return EOF;

    inode_t *inode = file->inode;
    
    // 1. 多态分发
    if (inode->pipe) {
        r_len = pipe_read(inode, buf, len);
        return r_len;
    } else if (ISCHR(inode->desc->mode)) {
        assert(inode->desc->zones[0]);
        r_len = device_read(inode->desc->zones[0], buf, len, 0, 0);
    } else if (ISBLK(inode->desc->mode)) {
        assert(inode->desc->zones[0]);
        
        device_t *device = device_get(inode->desc->zones[0]);
        assert(file->offset % BLOCK_SIZE == 0);
        assert(len % BLOCK_SIZE == 0);

        r_len = device_read(inode->desc->zones[0], buf, len, file->offset / BLOCK_SIZE, 0);
    } else {
        r_len = inode_read(inode, buf, len, file->offset);
    }

    // 2.统一更新
    if (r_len > 0)
        file->offset += r_len;

    return r_len;
}


int sys_write(fd_t fd, char *buf, int len) {
    if (fd < 0 || fd >= TASK_FILE_NR)
        return EOF;

    task_t *task = running_task();
    file_t *file = task->files[fd];
    if (!file)
        return EOF;
    if (len <= 0)
        return EOF;

    if ((file->flags & O_ACCMODE) == O_RDONLY)
        return EOF;
    
    int w_len = 0;
    inode_t *inode = file->inode;
    assert(inode);
    if (inode->pipe) {
        w_len = pipe_write(inode, buf, len);
        return w_len;
    } else if (ISCHR(inode->desc->mode)) {
        assert(inode->desc->zones[0]);
        w_len = device_write(inode->desc->zones[0], buf, len, 0, 0);
    } else if (ISBLK(inode->desc->mode)) {
        assert(inode->desc->zones[0]);
        
        device_t *device = device_get(inode->desc->zones[0]);
        assert(file->offset % BLOCK_SIZE == 0);
        assert(len % BLOCK_SIZE == 0);

        w_len = device_write(inode->desc->zones[0], buf, len, file->offset / BLOCK_SIZE, 0);
    } else {
        w_len = inode_write(inode, buf, len, file->offset);
    }

    if (w_len > 0)
        file->offset += w_len;

    return w_len;
}


void sys_close(fd_t fd) {
    assert(fd < TASK_FILE_NR);
    task_t *task = running_task();
    file_t *file = task->files[fd];
    if (!file)
        return;
    
    assert(file->inode);
    put_file(file);
    task_put_fd(task, fd);
}


int sys_lseek(fd_t fd, int offset, int whence) {
    if (fd < 0 || fd >= TASK_FILE_NR)
        return EOF;

    task_t *task = running_task();
    file_t *file = task->files[fd];

    if (!file || !file->inode)
        return EOF;
    
    int new_offset;
    switch (whence) {
        case SEEK_SET:
            new_offset = offset;
            break;
        case SEEK_CUR:
            new_offset = file->offset + offset;
            break;
        case SEEK_END:
            new_offset = file->inode->desc->size + offset;
            break;
        default:
            return EOF;   
        }

    if (new_offset < 0)
        return EOF;
    file->offset = new_offset;
        
    return file->offset;
}

int sys_readdir(fd_t fd, dirent_t *dir, u32 count) {
    if (fd < 0 || fd >= TASK_FILE_NR)
        return EOF;

    task_t *task = running_task();
    file_t *file = task->files[fd];
    
    if (!file || !file->inode)
        return EOF;
    if (!ISDIR(file->inode->desc->mode))
        return EOF;

    u32 size = count * sizeof(dirent_t);
    return sys_read(fd, (char *)dir, size);
}

static int dupfd(fd_t fd, fd_t arg) {
    task_t *task = running_task();
    if (fd >= TASK_FILE_NR || !task->files[fd])
        return EOF;

    for (; arg < TASK_FILE_NR; arg++) {
        if (!task->files[arg]) {
            break;
        }
    }

    if (arg >= TASK_FILE_NR)
        return EOF;
    task->files[arg] = task->files[fd];
    task->files[arg]->count++;

    return arg;
}

fd_t sys_dup(fd_t oldfd) {
    return dupfd(oldfd, 0);
}

fd_t sys_dup2(fd_t oldfd, fd_t newfd) {
    if (oldfd == newfd)
        return newfd;

    task_t *task = running_task();
    if (newfd < 0 || newfd >= TASK_FILE_NR || !task->files[oldfd])
        return EOF;

    sys_close(newfd);
    return dupfd(oldfd, newfd);
}