#include <xjos/types.h>
#include <xjos/stdio.h>
#include <xjos/syscall.h>

#define MAX_PATH_LEN 1024

int cmd_pwd(int argc, char **argv, char **envp) {
    (void)argc;
    (void)argv;
    (void)envp;

    char cwd[MAX_PATH_LEN];
    if (getcwd(cwd, MAX_PATH_LEN) == (char *)0) {
        return EOF;
    }
    printf("%s\n", cwd);
    return 0;
}

#ifndef XJOS_BUSYBOX_APPLET
int main(int argc, char **argv, char **envp) {
    return cmd_pwd(argc, argv, envp);
}
#endif
