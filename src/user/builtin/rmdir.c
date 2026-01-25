#include <xjos/types.h>
#include <xjos/stdio.h>
#include <xjos/syscall.h>
#include <xjos/fcntl.h>

int cmd_rmdir(int argc, char **argv, char **envp) {
    (void)envp;

    if (argc < 2) {
        printf("rmdir: missing operand\n");
        printf("Usage: rmdir <directory>\n");
        return EOF;
    }

    if (rmdir(argv[1]) == EOF) {
        printf("rmdir: failed to remove '%s': ", argv[1]);
        fd_t fd = open(argv[1], O_RDONLY, 0);
        if (fd == EOF) {
            printf("No such file or directory\n");
        } else {
            close(fd);
            printf("Directory not empty or not a directory\n");
        }
        return EOF;
    }

    return 0;
}

#ifndef XJOS_BUSYBOX_APPLET
int main(int argc, char **argv, char **envp) {
    return cmd_rmdir(argc, argv, envp);
}
#endif
