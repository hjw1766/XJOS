#ifdef XJOS
#include <xjos/types.h>
#include <xjos/stdio.h>
#include <xjos/syscall.h>
#include <xjos/string.h>
#else
#include <stdio.h>
#include <string.h>
#endif


int cmd_env(int argc, char **argv, char **envp) {
    for (size_t i = 0; i < argc; i++)
    {
        printf("%s\n", argv[i]);
    }

    for (size_t i = 0; 1; i++)
    {
        if (!envp[i])
            break;
        int len = strlen(envp[i]);
        if (len >= 1022)
            continue;
        printf("%s\n", envp[i]);
    }
    return 0;
}

#ifndef XJOS_BUSYBOX_APPLET
int main(int argc, char const *argv[], char const *envp[]) {
    return cmd_env(argc, (char **)argv, (char **)envp);
}
#endif
