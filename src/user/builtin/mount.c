#include <xjos/types.h>
#include <xjos/stdio.h>
#include <xjos/syscall.h>
#include <xjos/errno.h>

static const char *mount_error(int err) {
    switch (err) {
    case ENOENT:
        return "No such file or directory";
    case EPERM:
        return "Operation not permitted";
    case EBUSY:
        return "Resource busy";
    case EFSUNK:
        return "Unknown filesystem";
    case ENOSYS:
        return "Operation not supported";
    default:
        return "Mount failed";
    }
}

int cmd_mount(int argc, char **argv, char **envp) {
    (void)envp;

    if (argc < 3) {
        printf("mount: missing operand\n");
        printf("Usage: mount <source> <target>\n");
        return EOF;
    }

    int ret = mount(argv[1], argv[2], 0);
    if (ret < 0) {
        printf("mount: %s\n", mount_error(-ret));
    }
    return ret;
}

#ifndef XJOS_BUSYBOX_APPLET
int main(int argc, char **argv, char **envp) {
    return cmd_mount(argc, argv, envp);
}
#endif
