#include <fs/fs.h>
#include <fs/stat.h>
#include <libc/assert.h>
#include <xjos/task.h>
#include <drivers/device.h>


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
    if (fd == stdin) {
        device_t *device = device_find(DEV_KEYBOARD, 0);
        return device_read(device->dev, buf, len, 0, 0);
    }

    // no stdin other files

    if (fd < 0 || fd >= TASK_FILE_NR)
        return EOF;

    task_t *task = running_task();
    file_t *file = task->files[fd];
    if (!file)
        return EOF;
    if (len <= 0)
        return EOF;

    if ((file->flags & O_ACCMODE) == O_WRONLY)
        return EOF;

    inode_t *inode = file->inode;
    int len_inode = inode_read(inode, buf, len, file->offset);
    if (len_inode != EOF) {
        file->offset += len_inode;
    }

    return len_inode;
}


int sys_write(fd_t fd, char *buf, int len) {
    if (fd == stdout || fd == stderr) {
        device_t *device = device_find(DEV_CONSOLE, 0);
        return device_write(device->dev, buf, len, 0, 0);
    }

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
    
    inode_t *inode = file->inode;
    int len_inode = inode_write(inode, buf, len, file->offset);
    if (len_inode != EOF) {
        file->offset += len_inode;
    }   

    return len_inode;
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