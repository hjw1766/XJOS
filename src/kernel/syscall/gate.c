#include <xjos/interrupt.h>
#include <libc/assert.h>
#include <xjos/debug.h>
#include <xjos/syscall.h>
#include <xjos/task.h>
#include <xjos/memory.h>
#include <drivers/device.h>
#include <libc/string.h>
#include <fs/buffer.h>

extern void link_page(u32 vaddr);
extern void unlink_page(u32 vaddr);


#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

#define SYSTEM_SIZE (256)

handler_t syscall_table[SYSTEM_SIZE];


void syscall_check(u32 nr) {
    if (nr >= SYSTEM_SIZE)
        panic("Invalid system call number %d", nr);
}


static void sys_default() {
    panic("Ssycall not implemented");
}

extern void dir_test();

static u32 sys_test() {
    dir_test();

    char ch;
    device_t *device;

    device = device_find(DEV_KEYBOARD, 0);
    assert(device);
    device_read(device->dev, &ch, 1, 0, 0);

    device = device_find(DEV_CONSOLE, 0);
    assert(device);
    device_write(device->dev, &ch, 1, 0, 0);

    return 255;
}

extern int32 console_write();
extern mode_t sys_umask();

int32 sys_write(fd_t fd, const char *buf, u32 len) {
    if (fd == stdout || fd == stderr) {
        return console_write(NULL, buf, len);
    }

    panic("Invalid file descriptor %d", fd);
    return 0;
}


int sys_sync() {
    bsync();
    return 0;
}

extern time_t sys_time();

void syscall_init() {
    for (size_t i = 0; i < SYSTEM_SIZE; i++) {
        syscall_table[i] = sys_default;
    }

    syscall_table[SYS_NR_TEST] = sys_test;
    syscall_table[SYS_NR_EXIT] = task_exit;
    syscall_table[SYS_NR_SLEEP] = task_sleep;
    syscall_table[SYS_NR_YIELD] = task_yield;

    syscall_table[SYS_NR_WAITPID] = task_waitpid;
    syscall_table[SYS_NR_GETPID] = sys_getpid;
    syscall_table[SYS_NR_GETPPID] = sys_getppid;

    syscall_table[SYS_NR_FORK] = task_fork;

    syscall_table[SYS_NR_BRK] = sys_brk;

    syscall_table[SYS_NR_WRITE] = sys_write;
    syscall_table[SYS_NR_TIME] = sys_time;

    syscall_table[SYS_NR_UMASK] = sys_umask;
    syscall_table[SYS_NR_SYNC] = sys_sync;
}