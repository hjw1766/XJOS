#ifdef XJOS
#include <fs/fs.h>
#include <xjos/stdio.h>
#include <xjos/syscall.h>
#else
#include <stdio.h>
#endif


int cmd_dup(int argc, char **argv, char **envp) {
    char ch;

    while (true) {
        int n = read(STDIN_FILENO, &ch, 1);
        if (n == EOF) {
            break;
        }

        if (ch == '\n') {
            write(STDOUT_FILENO, &ch, 1);
            break;
        }

        write(STDOUT_FILENO, &ch, 1);
        write(STDOUT_FILENO, &ch, 1);
    }

    return 0;
}

#ifndef XJOS_BUSYBOX_APPLET
int main(int argc, char const *argv[], char const *envp[]) {
    return cmd_dup(argc, (char **)argv, (char **)envp);
}
#endif