#include <xjos/string.h>
#include <xjos/types.h>
#include <xjos/cpu.h>
#include <xjos/printk.h>
#include <xjos/debug.h>
#include <xjos/errno.h>
#include <xjos/net.h>
#include <xjos/string.h>
#include <fs/stat.h>
#include <fs/fs.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

extern int sys_socket(int domain, int type, int protocol);
extern void sys_close(fd_t fd);

err_t sys_test() {
    int fd = sys_socket(0, 0, 0);

    sys_close(fd);

    return EOK;
}
