#include <xjos/types.h>
#include <xjos/stdio.h>
#include <xjos/syscall.h>
#include <fs/stat.h>

int cmd_mkdir(int argc, char **argv, char **envp) {
    (void)envp;

    if (argc < 2) {
        printf("mkdir: missing operand\n");
        printf("Usage: mkdir <directory>\n");
        return EOF;
    }

    if (mkdir(argv[1], 0755) == EOF) {
        printf("mkdir: cannot create directory '%s': ", argv[1]);

        stat_t statbuf;
        if (stat(argv[1], &statbuf) == 0) {
            printf("Directory exists\n");
            return EOF;
        }

        printf("Permission denied or parent directory does not exist\n");
        return EOF;
    }

    return 0;
}

#ifndef XJOS_BUSYBOX_APPLET
int main(int argc, char **argv, char **envp) {
    return cmd_mkdir(argc, argv, envp);
}
#endif
