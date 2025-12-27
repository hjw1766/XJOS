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


static u32 sys_test() {
    return 255;
}

extern mode_t sys_umask();


int sys_sync() {
    bsync();
    return 0;
}

extern int sys_mkdir();
extern int sys_rmdir();

extern int sys_link();
extern int sys_unlink();

extern int sys_read();
extern int sys_write();
extern int sys_lseek();

extern int sys_chdir();
extern int sys_chroot();
extern char *sys_getcwd();

extern int sys_create();
extern fd_t sys_open();
extern void sys_close();

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

    syscall_table[SYS_NR_OPEN] = sys_open;
    syscall_table[SYS_NR_CLOSE] = sys_close;
    syscall_table[SYS_NR_CREAT] = sys_create;

    syscall_table[SYS_NR_MKDIR] = sys_mkdir;
    syscall_table[SYS_NR_RMDIR] = sys_rmdir;

    syscall_table[SYS_NR_LINK] = sys_link;
    syscall_table[SYS_NR_UNLINK] = sys_unlink;

    syscall_table[SYS_NR_READ] = sys_read;
    syscall_table[SYS_NR_WRITE] = sys_write;
    syscall_table[SYS_NR_LSEEK] = sys_lseek;

    syscall_table[SYS_NR_TIME] = sys_time;

    syscall_table[SYS_NR_UMASK] = sys_umask;
    syscall_table[SYS_NR_SYNC] = sys_sync;

    syscall_table[SYS_NR_CHDIR] = sys_chdir;
    syscall_table[SYS_NR_CHROOT] = sys_chroot;
    syscall_table[SYS_NR_GETCWD] = sys_getcwd;
}