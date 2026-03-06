#include <xjos/syscall.h>
#include <xjos/signal.h>
#include <xjos/stdlib.h>


int cmd_kill(int argc, char **argv, char **envp) {
    if (argc < 2)
        return 1;

    return kill(atoi(argv[1]), SIGTERM);
}

#ifndef XJOS_BUSYBOX_APPLET
int main(int argc, char const *argv[], char const *envp[]) {
    return cmd_kill(argc, (char **)argv, (char **)envp);
}
#endif