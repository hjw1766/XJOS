#include <xjos/assert.h>
#include <xjos/printk.h>
#include <xjos/stdio.h>
#include <xjos/types.h>
#include <xjos/syscall.h>



static u8 buf[1024];


static void spin(char *name) {
    printk("Spinning in %s ...\n", name);
    while (true);
}


void assertion_failed(char *exp, char *file, char *base, int line) {
    printk(
        "\n--> assert(%s) failed!!!\n"
        "--> file: %s\n"
        "--> base: %s\n"
        "--> line: %d\n",
        exp, file, base, line
    );

    spin("assertion_failed()");

    asm volatile("ud2");
}


void panic(const char *fmt, ...) {
    va_list ars;
    va_start(ars, fmt);

    int i = vsprintf(buf, fmt, ars);

    va_end(ars);

    printk("!!! panic !!!\n---->%s \n", buf);
    sync();
    spin("panic()");

    asm volatile("ud2");
}