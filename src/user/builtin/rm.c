#include <xjos/types.h>
#include <xjos/stdio.h>
#include <xjos/syscall.h>
#include <xjos/fcntl.h>

int cmd_rm(int argc, char **argv, char **envp) {
    (void)envp;

    if (argc < 2) {
        printf("rm: missing operand\n");
        printf("Usage: rm <file>\n");
        return EOF;
    }

    if (unlink(argv[1]) == EOF) {
        printf("rm: cannot remove '%s': ", argv[1]);
        fd_t fd = open(argv[1], O_RDONLY, 0);
        if (fd == EOF) {
            printf("No such file or directory\n");
        } else {
            close(fd);
            printf("Is a directory or permission denied\n");
        }
        return EOF;
    }

    return 0;
}

#ifndef XJOS_BUSYBOX_APPLET
int main(int argc, char **argv, char **envp) {
    return cmd_rm(argc, argv, envp);
}
#endif
