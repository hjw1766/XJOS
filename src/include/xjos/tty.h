#ifndef XJOS_TTY_H
#define XJOS_TTY_H

#include <xjos/types.h>

typedef struct tty_t {
    dev_t rdev;     // 读设备号
    dev_t wdev;     // 写设备号
    pid_t pgid;     // 进程组ID
} tty_t;

// ioctl 进程组命令
#define TIOCSPGRP 0x5410


#endif /* XJOS_TTY_H */