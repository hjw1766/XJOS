#include <xjos/types.h>
#include <xjos/cpu.h>
#include <xjos/printk.h>
#include <xjos/debug.h>
#include <xjos/errno.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

extern void test_e1000_send_packet();

err_t sys_test() {
    test_e1000_send_packet();

    return EOK;
}