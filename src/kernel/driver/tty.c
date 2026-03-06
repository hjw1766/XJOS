#include <xjos/tty.h>
#include <drivers/device.h>
#include <xjos/assert.h>
#include <xjos/fifo.h>
#include <xjos/task.h>
#include <xjos/mutex.h>
#include <xjos/debug.h>
#include <xjos/errno.h>


#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

extern task_t *tasks_table[TASK_NR];
extern int sys_kill();

static tty_t typewriter;



// 向前台进程组发送 SIGINT 信号
int tty_intr() {
    tty_t *tty = &typewriter;
    if (!tty->pgid)
        return 0;

    for (size_t i = 0; i < TASK_NR; i++) {
        task_t *task = tasks_table[i];
        if (!task) continue;
        if (task->pgid != tty->pgid) continue;

        sys_kill(task->pid, SIGINT);
    }

    return 0;
}

int tty_rx_notify(char *ch, bool ctrl, bool shift, bool alt) {
    switch (*ch) {
        case '\r':
            *ch = '\n';
            break;
        default:
            break;
    }

    if (!ctrl)
        return 0;

    tty_t *tty = &typewriter;
    switch (*ch) {
        case 'c':
        case 'C':
            LOGK("CTRL + C Pressed, sending SIGINT to process group %d\n", tty->pgid);
            tty_intr();
            *ch = '\0'; // 将输入字符置为0，表示已处理
            return 0;
        case 'l':
        case 'L':
            device_write(tty->wdev, "\x1b[2J\x1b[0;0H\r", 12, 0, 0);
            *ch = '\r';
            return 0;
        default:
            return 1;
    }
}

int tty_read(tty_t *tty, char *buf, u32 count) {
    return device_read(tty->rdev, buf, count, 0, 0);
}

int tty_write(tty_t *tty, char *buf, u32 count) {
    return device_write(tty->wdev, buf, count, 0, 0);
}

int tty_ioctl(tty_t *tty, int cmd, void *args, int flags) {
    switch (cmd) {
        case TIOCSPGRP:
            tty->pgid = (pid_t)args;
            return 0;
        default:
            break;
    }

    return -EINVAL;
}

int sys_stty() {
    return -ENOSYS;
}

int sys_gtty() {
    return -ENOSYS;
}

void tty_init() {
    device_t *device = NULL;

    tty_t *tty = &typewriter;

    device = device_find(DEV_KEYBOARD, 0);
    tty->rdev = device->dev;

    device = device_find(DEV_CONSOLE, 0);
    tty->wdev = device->dev;

    device_install(DEV_CHAR, DEV_TTY, tty, "tty",
        0, tty_ioctl, tty_read, tty_write);
}