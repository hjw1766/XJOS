#include <xjos/types.h>
#include <xjos/stdio.h>
#include <xjos/syscall.h>
#include <xjos/time.h>

static void fmt_time(time_t stamp, char *buf) {
    tm time;
    localtime(stamp, &time);
    sprintf(buf, "%04d-%02d-%02d %02d:%02d:%02d",
            time.tm_year + 1900,
            time.tm_mon,
            time.tm_mday,
            time.tm_hour,
            time.tm_min,
            time.tm_sec);
}

int cmd_date(int argc, char **argv, char **envp) {
    (void)argc;
    (void)argv;
    (void)envp;

    char buf[64];
    fmt_time(time(), buf);
    printf("System time: %s\n", buf);
    return 0;
}

#ifndef XJOS_BUSYBOX_APPLET
int main(int argc, char **argv, char **envp) {
    return cmd_date(argc, argv, envp);
}
#endif
