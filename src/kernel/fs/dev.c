#include <drivers/device.h>
#include <fs/stat.h>
#include <xjos/stdio.h>
#include <xjos/assert.h>
#include <fs/fs.h>

extern file_t file_table[];

extern int sys_mkdir(char *pathname, mode_t mode);
extern int sys_mknod(char *pathname, int mode, int dev);
extern int sys_link(char *oldname, char *newname);

void dev_init() {
    sys_mkdir("/dev", 0755);

    device_t *device = NULL;

    device = device_find(DEV_RAMDISK, 0);
    assert(device);
    devmkfs(device->dev, 0);

    // 将 mba设备挂载到/dev目录下，后续/dev目录下的所有设备节点均在该设备上创建
    super_block_t *sb = read_super(device->dev);
    sb->iroot = iget(device->dev, 1);
    sb->imount = namei("/dev");
    sb->imount->mount = device->dev;

    device = device_find(DEV_CONSOLE, 0);
    sys_mknod("/dev/console", IFCHR | 0600, device->dev);

    device = device_find(DEV_KEYBOARD, 0);
    sys_mknod("/dev/keyboard", IFCHR | 0400, device->dev);

    char name[32];

    for (size_t i = 0; true; i++) {
        device = device_find(DEV_IDE_DISK, i);
        if (!device)
            break;
        sprintf(name, "/dev/%s", device->name);
        sys_mknod(name, IFBLK | 0600, device->dev);
    }

    for (size_t i = 0; true; i++) {
        device = device_find(DEV_IDE_PART, i);
        if (!device)
            break;
        sprintf(name, "/dev/%s", device->name);
        sys_mknod(name, IFBLK | 0600, device->dev);
    }

    // serial devices
    for (size_t i = 0; true; i++) {
        device = device_find(DEV_SERIAL, i);
        if (!device)
            break;
        sprintf(name, "/dev/%s", device->name);
        sys_mknod(name, IFCHR | 0600, device->dev);
    }

    // RAMDISK devices
    for (size_t i = 1; true; i++) {
        device = device_find(DEV_RAMDISK, i);
        if (!device)
            break;
        sprintf(name, "/dev/%s", device->name);
        sys_mknod(name, IFBLK | 0600, device->dev);
    }

    sys_link("/dev/console", "/dev/stdout");
    sys_link("/dev/console", "/dev/stderr");
    sys_link("/dev/keyboard", "/dev/stdin");

    file_t *file;
    inode_t *inode;
    file = &file_table[STDIN_FILENO];
    inode = namei("/dev/stdin");
    file->inode = inode;
    file->mode = inode->desc->mode;
    file->flags = O_RDONLY;
    file->offset = 0;

    file = &file_table[STDOUT_FILENO];
    inode = namei("/dev/stdout");
    file->inode = inode;
    file->mode = inode->desc->mode;
    file->flags = O_WRONLY;
    file->offset = 0;

    file = &file_table[STDERR_FILENO];
    inode = namei("/dev/stderr");
    file->inode = inode;
    file->mode = inode->desc->mode;
    file->flags = O_WRONLY;
    file->offset = 0;
}