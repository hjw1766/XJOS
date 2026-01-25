#include <xjos/types.h>
#include <xjos/stdio.h>
#include <xjos/syscall.h>

int cmd_mkfs(int argc, char **argv, char **envp) {
    (void)envp;

    if (argc < 2) {
        printf("mkfs: missing operand\n");
        printf("Usage: mkfs <device>\n");
        return EOF;
    }

    return mkfs(argv[1], 0);
}

#ifndef XJOS_BUSYBOX_APPLET
int main(int argc, char **argv, char **envp) {
    return cmd_mkfs(argc, argv, envp);
}
#endif
