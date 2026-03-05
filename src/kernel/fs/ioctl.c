#include <xjos/types.h>
#include <xjos/errno.h>
#include <fs/stat.h>
#include <xjos/task.h>
#include <drivers/device.h>
#include <fs/fs.h>


// 控制设备输入输出
int sys_ioctl(fd_t fd, int cmd, void *args) {
    if (fd >= TASK_FILE_NR)
        return -EBADF;

    task_t *task = running_task();
    file_t *file = task->files[fd];
    if (!file)
        return -EBADF;
    
    // fd 一定是某种设备
    int mode = file->inode->desc->mode;
    if (!ISCHR(mode) && !ISBLK(mode))
        return -EINVAL;

    // get device number
    dev_t dev = file->inode->desc->zones[0];
    if (dev >= DEVICE_NR)
        return -ENOTTY;

    return device_ioctl(dev, cmd, args, 0);
}