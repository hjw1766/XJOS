#include <xjos/syscall.h>
#include <xjos/string.h>
#include <xjos/stdlib.h>
#include <xjos/stdio.h>
#include <xjos/assert.h>
#include <xjos/fcntl.h>
#include <fs/stat.h>


#define MAX_CMD_LEN 256
#define MAX_ARG_NR 16
#define MAX_PATH_LEN 1024
#define BUFLEN 1024

static char cwd[MAX_PATH_LEN];
static char cmd[MAX_CMD_LEN];
static char *args[MAX_ARG_NR];
static char buf[BUFLEN];

typedef void (*cmd_handler_t)(int argc, char *argv[]);

typedef struct {
    const char *name;
    cmd_handler_t handler;
    const char *desc;   // help cmd
} cmd_t;

static const cmd_t cmd_table[];

static char *default_envp[] = {
    "HOME=/",
    "PATH=/bin",
    NULL
};

static char **current_envp = default_envp;

static const char *logo[] = {
        "__  __   _  _____ ____ ",
        "\\ \\/ /  | |/ _ \\ / ___|",
        " \\  /_  | | | | |\\___ \\",
        " /  \\ |_| | |_| |___) |",
        "/_/\\_\\\\___/ \\___/|____/ "
};

// ---------- helper function ----------

static const char *basename(const char *path) {
    const char *ptr = strrchr(path, '/');
    if (!ptr) return path;
    return ptr + 1;
}

static void print_prompt() {
    if (getcwd(cwd, MAX_PATH_LEN) < 0) {
        strcpy(cwd, "unknown");
    }
    const char *base = basename(cwd);
    if (*base == '\0') base = "/";
    printf("[root %s]# ", base);
}

static pid_t spawn_process(char *filename, char *argv[], fd_t infd, fd_t outfd, fd_t errfd);

// -------- builtin functions --------
static pid_t lookup_and_spawn(char **cmd_argv, fd_t infd, fd_t outfd, fd_t errfd) {
    char *cmd_name = cmd_argv[0];

    stat_t statbuf;

    if (strchr(cmd_name, '/')) {
        if (stat(cmd_name, &statbuf) != EOF) {
            return spawn_process(cmd_name, cmd_argv, infd, outfd, errfd);
        }
    } else {
        sprintf(buf, "/bin/%s", cmd_name);
        if (stat(buf, &statbuf) != EOF) {
            return spawn_process(buf, cmd_argv, infd, outfd, errfd);
        }
    }

    printf("sh: command not found: %s\n", cmd_name);

    if (infd != EOF && infd != STDIN_FILENO) close(infd);
    if (outfd != EOF && outfd != STDOUT_FILENO) close(outfd);

    return -1;
}

static int dupfile(int argc, char *argv[], fd_t dupfd[3]) {
    for (size_t i = 0; i < 3; i++) {
        dupfd[i] = EOF;
    }

    int outappend = 0;
    int errappend = 0;

    char *infile = NULL;
    char *outfile = NULL;
    char *errfile = NULL;

    for (int i = 0; i < argc; i++) {
        if (!argv[i]) continue;

        if (!strcmp(argv[i], "<") && (i + 1) < argc && argv[i + 1]) {
            infile = argv[i + 1];
            argv[i] = NULL;
            i++;
            continue;
        }
        if (!strcmp(argv[i], ">") && (i + 1) < argc && argv[i + 1]) {
            outfile = argv[i + 1];
            argv[i] = NULL;
            outappend = 0;
            i++;
            continue;
        }
        if (!strcmp(argv[i], ">>") && (i + 1) < argc && argv[i + 1]) {
            outfile = argv[i + 1];
            argv[i] = NULL;
            outappend = O_APPEND;
            i++;
            continue;
        }
        if (!strcmp(argv[i], "2>") && (i + 1) < argc && argv[i + 1]) {
            errfile = argv[i + 1];
            argv[i] = NULL;
            errappend = 0;
            i++;
            continue;
        }
        if (!strcmp(argv[i], "2>>") && (i + 1) < argc && argv[i + 1]) {
            errfile = argv[i + 1];
            argv[i] = NULL;
            errappend = O_APPEND;
            i++;
            continue;
        }
    }

    if (infile != NULL) {
        fd_t fd = open(infile, O_RDONLY, 0);
        if (fd == EOF) {
            printf("sh: open failed: %s\n", infile);
            goto rollback;
        }
        dupfd[0] = fd;
    }

    if (outfile != NULL) {
        int flags = O_WRONLY | O_CREAT | (outappend ? O_APPEND : O_TRUNC);
        fd_t fd = open(outfile, flags, 0755);
        if (fd == EOF) {
            printf("sh: open failed: %s\n", outfile);
            goto rollback;
        }
        dupfd[1] = fd;
    }

    if (errfile != NULL) {
        int flags = O_WRONLY | O_CREAT | (errappend ? O_APPEND : O_TRUNC);
        fd_t fd = open(errfile, flags, 0755);
        if (fd == EOF) {
            printf("sh: open failed: %s\n", errfile);
            goto rollback;
        }
        dupfd[2] = fd;
    }

    return 0;

rollback:
    for (size_t i = 0; i < 3; i++) {
        if (dupfd[i] != EOF) {
            close(dupfd[i]);
            dupfd[i] = EOF;
        }
    }
    return -1;
}

static int apply_redirect_fds(fd_t dupfd[3], fd_t saved[3]) {
    for (size_t i = 0; i < 3; i++) {
        saved[i] = EOF;
    }

    for (fd_t i = 0; i < 3; i++) {
        if (dupfd[i] == EOF) continue;

        saved[i] = dup(i);
        if (saved[i] == EOF) {
            printf("sh: dup failed\n");
            return -1;
        }
        if (dup2(dupfd[i], i) == EOF) {
            printf("sh: dup2 failed\n");
            return -1;
        }
        close(dupfd[i]);
        dupfd[i] = EOF;
    }
    return 0;
}

static void restore_redirect_fds(fd_t saved[3]) {
    for (fd_t i = 0; i < 3; i++) {
        if (saved[i] == EOF) continue;
        (void)dup2(saved[i], i);
        close(saved[i]);
        saved[i] = EOF;
    }
}

static void close_redirect_fds(fd_t dupfd[3]) {
    for (size_t i = 0; i < 3; i++) {
        if (dupfd[i] != EOF) {
            close(dupfd[i]);
            dupfd[i] = EOF;
        }
    }
}

static pid_t spawn_process(char *filename, char *argv[], fd_t infd, fd_t outfd, fd_t errfd) {
    int status;
    pid_t pid = fork();

    if (pid > 0) {
        if (infd != EOF && infd != STDIN_FILENO) close(infd);
        if (outfd != EOF && outfd != STDOUT_FILENO) close(outfd);
        if (errfd != EOF && errfd != STDERR_FILENO) close(errfd);

        return pid;
    }

    if (infd != EOF && infd != STDIN_FILENO) {
        (void)dup2(infd, STDIN_FILENO);
        close(infd);
    }
    if (outfd != EOF && outfd != STDOUT_FILENO) {
        (void)dup2(outfd, STDOUT_FILENO);
        close(outfd);
    }
    if (errfd != EOF && errfd != STDERR_FILENO) {
        (void)dup2(errfd, STDERR_FILENO);
        close(errfd);
    }

    int i = execve(filename, argv, current_envp);
    printf("sh: command not found or execution failed: %s\n", filename);
    exit(i);    // hlt if execve failed
}


static void builtin_logo(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    clear();

    int terminal_width = 80;
    int logo_width = 23;
    int padding = (terminal_width - logo_width) / 2;

    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < padding; j++) {
            printf(" ");
        }
        printf("%s\n", logo[i]);
    }

    printf("\n");
}

static void builtin_test(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    printf("Running system test...\n");
}

static void builtin_help(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    printf("Available commands:\n");
    const cmd_t *ptr = cmd_table;
    while (ptr->name) {
        printf("  %-8s - %s\n", ptr->name, ptr->desc);
        ptr++;
    }
}


static void builtin_cd(int argc, char *argv[]) {
    if (argc < 2) return;   // todo cd ~

    if (chdir(argv[1]) == EOF) {
        printf("cd: %s: No such file or directory\n", argv[1]);
    }
}

static void builtin_exit(int argc, char *argv[]) {
    int code = 0;
    if (argc == 2) code = atoi(argv[1]); // string to int
    exit(code);
}

// -------- command table --------

static const cmd_t cmd_table[] = {
    {"test", builtin_test, "Run system test"},
    {"logo", builtin_logo, "Display system logo"},
    {"cd",   builtin_cd,   "Change directory"},
    {"exit", builtin_exit, "Exit the shell"},
    {"help", builtin_help, "Display this help message"},
    {NULL, NULL, NULL}
};

static void execute(int argc, char *argv[]) {
    if (argc == 0) return;

    // 1. handle redirection
    fd_t dupfd[3];
    if (dupfile(argc, argv, dupfd) < 0) {
        return;
    }

    // 2. check pipe or not
    bool has_pipe = false;
    for (int i = 0; i < argc; i++) {
        if (argv[i] && !strcmp(argv[i], "|")) {
            has_pipe = true;
            break;
        }
    }

    // 3. no pipe, execute single command
    if (!has_pipe) {
        const cmd_t *ptr = cmd_table;
        while (ptr->name) {
            if (!strcmp(argv[0], ptr->name)) {
                fd_t saved[3];
                if (apply_redirect_fds(dupfd, saved) == 0) {
                    ptr->handler(argc, argv);
                    restore_redirect_fds(saved);
                }
                close_redirect_fds(dupfd);
                return;
            }
            ptr++;
        }
    }

    // 4. has pipe, execute piped commands
    fd_t input_fd = (dupfd[0] == EOF) ? STDIN_FILENO : dupfd[0];
    fd_t error_fd = (dupfd[2] == EOF) ? STDERR_FILENO : dupfd[2];

    char **current_cmd = argv;
    int pids[MAX_ARG_NR];
    int pid_count = 0;

    for (int i = 0; i <= argc; i++) {
        if (!argv[i]) continue;

        if (strcmp(argv[i], "|") == 0) {
            argv[i] = NULL;

            fd_t pipefd[2];
            if (pipe(pipefd) == EOF) {
                printf("sh: pipe failed\n");
                return;
            }

            // in -> input_fd, out -> pipefd[1]
            pid_t pid = lookup_and_spawn(current_cmd, input_fd, pipefd[1], error_fd);
            if (pid > 0) pids[pid_count++] = pid;

            input_fd = pipefd[0];

            current_cmd = &argv[i + 1];
        }
    }

    // 5. execute last command
    // in -> prev pipe read, out -> dupfd[1] or STDOUT
    fd_t final_out = (dupfd[1] == EOF) ? STDOUT_FILENO : dupfd[1];

    if (current_cmd[0] != NULL) {
        pid_t pid = lookup_and_spawn(current_cmd, input_fd, final_out, error_fd);
        if (pid > 0) pids[pid_count++] = pid;
    }

    // 6. wait for all child processes
    for (int i = 0; i < pid_count; i++) {
        int status;
        waitpid(pids[i], &status);
    }
}

static void readline(char *buf, int count) {
    char *ptr = buf;
    u32 idx = 0;

    while (idx < (u32)(count - 1)) {
        if (read(STDIN_FILENO, ptr + idx, 1) == -1) break;

        char ch = ptr[idx];

        if (ch == '\n' || ch == '\r') {
            ptr[idx] = '\0';
            write(STDOUT_FILENO, "\n", 1);
            return;
        } else if (ch == '\b' || ch == 0x7F) {
            if (idx > 0) {
                idx--;
                write(STDOUT_FILENO, "\b \b", 3);  // left, printf ' ', left
            }
        } else if (ch == '\t') {
            continue;
        } else {
            write(STDOUT_FILENO, &ch, 1);
            idx++;
        }
    }
    buf[idx] = '\0';
}

static int cmd_parse(char *cmd, char *argv[], char token) {
    int argc = 0;
    char *next = cmd;

    while (*next && argc < MAX_ARG_NR - 1) {
        // skip token
        while (*next == token) next++;
        if (*next == '\0') break;

        argv[argc++] = next;    // store arg ptr

        // exp. "ls -l", next -> ' ', skip word
        while (*next && *next != token) next++;
        if (*next) {
            *next = 0;   // terminate arg, ' ' -> '\0'
            next++;
        }
    }

    argv[argc] = NULL;
    return argc;
}

int cmd_sh(int argc, char **argv, char **envp) {
    (void)argc;
    (void)argv;

    current_envp = envp ? envp : default_envp;

    memset(cmd, 0, sizeof(cmd));
    getcwd(cwd, MAX_PATH_LEN);

    builtin_logo(0, NULL);

    while (true) {
        print_prompt();
        readline(cmd, sizeof(cmd));

        if (cmd[0] == 0) continue;

        int cargc = cmd_parse(cmd, args, ' ');
        if (cargc > 0) {
            execute(cargc, args);
        }
    }

    return 0;
}

#ifndef XJOS_BUSYBOX_APPLET
int main(int argc, char **argv, char **envp) {
    return cmd_sh(argc, argv, envp);
}
#endif
