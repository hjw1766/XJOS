#include <xjos/syscall.h>
#include <xjos/stdio.h>
#include <xjos/signal.h>


static int signal_handler(int sig) {
    printf("pid %d when %d signal %d happened \a\n", getpid(), time(), sig);
    signal(SIGALRM, (int)signal_handler);
    alarm(2);
}

int cmd_alarm(int argc, char **argv, char **envp) {
    signal(SIGALRM, (int)signal_handler);
    alarm(2);

    while (1) {
        printf("pid %d sleep 1 second %d\n", getpid(), time());
        sleep(1000);
    }

    return 0;
}

#ifndef XJOS_BUSYBOX_APPLET
int main(int argc, char const *argv[], char const *envp[]) {
    return cmd_alarm(argc, (char **)argv, (char **)envp);
}
#endif