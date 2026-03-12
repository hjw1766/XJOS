#include <xjos/types.h>
#include <xjos/cpu.h>
#include <xjos/printk.h>
#include <xjos/debug.h>
#include <xjos/errno.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

err_t sys_test() {
    // 输出二进制
    LOGK("%08b\n", 0x5a);
    // 输出 mac 地址
    LOGK("%m\n", "\x11\x22\x03\x04\x5f\x5a");
    // 输出 ip 地址
    LOGK("%r\n", "\xff\x4e\x03\x04");

    return EOK;
}