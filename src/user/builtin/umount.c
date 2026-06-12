#include <xjos/types.h>
#include <xjos/stdio.h>
#include <xjos/syscall.h>
#include <xjos/errno.h>

static const char *umount_error(int err) {
    switch (err) {
    case ENOENT:
        return "No such file or directory";
    case ENOTBLK:
        return "Not a mount target";
    case EBUSY:
        return "Target is busy";
    default:
        return "Umount failed";
    }
}

int cmd_umount(int argc, char **argv, char **envp) {
    (void)envp;

    if (argc < 2) {
        printf("umount: missing operand\n");
        printf("Usage: umount <target>\n");
        return EOF;
    }

    int ret = umount(argv[1]);
    if (ret < 0) {
        printf("umount: %s\n", umount_error(-ret));
    }
    return ret;
}

#ifndef XJOS_BUSYBOX_APPLET
int main(int argc, char **argv, char **envp) {
    return cmd_umount(argc, argv, envp);
}
#endif
