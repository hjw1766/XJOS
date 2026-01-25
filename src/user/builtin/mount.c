#include <xjos/types.h>
#include <xjos/stdio.h>
#include <xjos/syscall.h>

int cmd_mount(int argc, char **argv, char **envp) {
    (void)envp;

    if (argc < 3) {
        printf("mount: missing operand\n");
        printf("Usage: mount <source> <target>\n");
        return EOF;
    }

    return mount(argv[1], argv[2], 0);
}

#ifndef XJOS_BUSYBOX_APPLET
int main(int argc, char **argv, char **envp) {
    return cmd_mount(argc, argv, envp);
}
#endif
