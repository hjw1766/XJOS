#include <xjos/types.h>
#include <xjos/cpu.h>
#include <xjos/printk.h>
#include <xjos/debug.h>
#include <xjos/errno.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

err_t sys_test() {
    LOGK("Test syscall called\n");

    cpu_vendor_t vendor;

    cpu_vendor_id(&vendor);
    printk("CPU vendor id: %s\n", vendor.info);
    printk("CPU max value: 0x%X\n", vendor.max_value);

    cpu_version_t ver;

    cpu_version(&ver);
    printk("FPU support state: %d\n", ver.FPU);
    printk("APIC support state: %d\n", ver.APIC);
    return EOK;
}