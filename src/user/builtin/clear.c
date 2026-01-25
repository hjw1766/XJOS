#include <xjos/types.h>
#include <xjos/syscall.h>

int cmd_clear(int argc, char **argv, char **envp) {
    (void)argc;
    (void)argv;
    (void)envp;
    clear();
    return 0;
}

#ifndef XJOS_BUSYBOX_APPLET
int main(int argc, char **argv, char **envp) {
    return cmd_clear(argc, argv, envp);
}
#endif
