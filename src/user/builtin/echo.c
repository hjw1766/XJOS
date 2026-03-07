#ifdef XJOS
#include <xjos/types.h>
#include <xjos/stdio.h>
#include <xjos/syscall.h>
#include <xjos/string.h>
#else
#include <stdio.h>
#include <string.h>
#endif

int cmd_echo(int argc, char **argv, char **envp) {
    (void)envp;
    for (size_t i = 1; i < argc; i++) {
        printf("%s", argv[i]);
        if (i < argc - 1) {
            printf(" ");
        }
    }
    printf("\n");
    return 0;
}

#ifndef XJOS_BUSYBOX_APPLET
int main(int argc, char const *argv[], char const *envp[]) {
    return cmd_echo(argc, (char **)argv, (char **)envp);
}
#endif
