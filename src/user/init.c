#include <xjos/types.h>
#include <xjos/stdio.h>
#include <xjos/syscall.h>

static char *default_envp[] = {
    "HOME=/",
    "PATH=/bin",
    NULL,
};

int main(int argc, char **argv, char **envp) {
    (void)argc;
    (void)argv;

    // init is designed to run as PID 1.
    // If launched from an interactive shell, do not start a respawn loop.
    if (getpid() != 1) {
        printf("init: must run as PID 1; use 'sh' instead\n");
        return 0;
    }

    char **use_envp = envp ? envp : default_envp;

    while (true) {
        int32 status = 0;
        pid_t pid = fork();
        if (pid == 0) {
            char *sh_argv[] = {"sh", NULL};
            execve("/bin/sh", sh_argv, use_envp);
            printf("init: exec /bin/sh failed\n");
            exit(127);
        }

        if (pid < 0) {
            // fork failed; back off a bit
            sleep(1000);
            continue;
        }

        (void)waitpid(pid, &status);

        // Avoid tight respawn loops if sh exits immediately.
        sleep(200);
    }
}
