#include <xjos/types.h>
#include <xjos/stdio.h>
#include <xjos/syscall.h>

int cmd_umount(int argc, char **argv, char **envp) {
    (void)envp;

    if (argc < 2) {
        printf("umount: missing operand\n");
        printf("Usage: umount <target>\n");
        return EOF;
    }

    return umount(argv[1]);
}

#ifndef XJOS_BUSYBOX_APPLET
int main(int argc, char **argv, char **envp) {
    return cmd_umount(argc, argv, envp);
}
#endif
