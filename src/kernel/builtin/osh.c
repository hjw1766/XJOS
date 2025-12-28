#include <xjos/syscall.h>
#include <libc/string.h>
#include <xjos/stdlib.h>
#include <libc/stdio.h>
#include <libc/assert.h>
#include <fs/fs.h>


#define MAX_CMD_LEN 256
#define MAX_ARG_NR 16
#define MAX_PATH_LEN 1024
#define BUFLEN 1024

static char cwd[MAX_PATH_LEN];
static char cmd[MAX_CMD_LEN];
static char *argv[MAX_ARG_NR];
static char buf[BUFLEN];


typedef void (*cmd_handler_t)(int argc, char *argv[]);

typedef struct {
    const char *name;
    cmd_handler_t handler;
    const char *desc;   // help cmd
} cmd_t;

// ---------- helper function ----------

const char *basename(const char *path) {

    const char *ptr = strrchr(path, '/');
    if (!ptr) return path;
    return ptr + 1;
}

void print_prompt() {
    if (getcwd(cwd, MAX_PATH_LEN) < 0) {
        strcpy(cwd, "unknown");
    }
    const char *base = basename(cwd);
    if (*base == '\0') base = "/";
    printf("[root %s]# ", base);
}

// -------- builtin functions --------

void builtin_logo(int argc, char *argv[]) {
    clear();

    static const char *logo[] = {
        "__  __   _  _____ ____ ",
        "\\ \\/ /  | |/ _ \\ / ___|",
        " \\  /_  | | | | |\\___ \\",
        " /  \\ |_| | |_| |___) |",
        "/_/\\_\\\\___/ \\___/|____/ "
    };

    int terminal_width = 80; // VGA 文本模式标准宽度
    int logo_width = 23;     // 上述字符画中最长一行的长度
    int padding = (terminal_width - logo_width) / 2;


    for (int i = 0; i < 5; i++)
    {
        for (int j = 0; j < padding; j++)
        {
            printf(" ");
        }
        printf("%s\n", logo[i]);
    }

    printf("\n");
}

void builtin_test(int argc, char *argv[]) { test(); }
void builtin_pwd(int argc, char *argv[]) { getcwd(cwd, MAX_PATH_LEN); printf("%s\n", cwd); }
void builtin_clear(int argc, char *argv[]) { clear(); }

void builtin_mkdir(int argc, char *argv[]) {
    if (argc < 2) {
        printf("mkdir: missing operand\n");
        printf("Usage: mkdir <directory>\n");
        return;
    }
    
    if (mkdir(argv[1], 0755) == EOF) {
        printf("mkdir: cannot create directory '%s': ", argv[1]);
        // exists check
        fd_t fd = open(argv[1], O_RDONLY, 0);
        if (fd != EOF) {
            close(fd);
            printf("File exists\n");
        } else {
            printf("No such file or directory\n");
        }
    }
}

void builtin_rmdir(int argc, char *argv[]) {
    if (argc < 2) {
        printf("rmdir: missing operand\n");
        printf("Usage: rmdir <directory>\n");
        return;
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
    }
}

void builtin_rm(int argc, char *argv[]) {
    if (argc < 2) {
        printf("rm: missing operand\n");
        printf("Usage: rm <file>\n");
        return;
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
    }
}

void builtin_cd(int argc, char *argv[]) { 
    if (argc < 2) return;

    if (chdir(argv[1]) == EOF) {
        printf("cd: %s: No such file or directory\n", argv[1]);
    }
}

void builtin_ls(int argc, char *argv[]) {
    char *target = (argc > 1) ? argv[1] : cwd;
    fd_t fd = open(target, O_RDONLY, 0);
    if (fd == EOF) return;

    dentry_t entry;
    while (readdir(fd, &entry, 1) != EOF) {
        if (!entry.nr) continue; // skip empty entry
        if (!strcmp(entry.name, ".") || !strcmp(entry.name, "..")) continue;
        printf("%s ", entry.name);
    }
    printf("\n");
    close(fd);
}

void builtin_cat(int argc, char *argv[]) {
    if (argc < 2) return;
    fd_t fd = open(argv[1], O_RDONLY, 0);
    if (fd == EOF) {
        printf("cat: %s: No such file\n", argv[1]);
        return;
    }

    int len;
    while ((len = read(fd, buf, BUFLEN)) > 0) {
        write(stdout, buf, len);
    }
    close(fd);
}

void builtin_exit(int argc, char *argv[]) {
    int code = 0;
    if (argc == 2) code = atoi(argv[1]); // string to int
    exit(code);
}

// -------- command table --------

static const cmd_t cmd_table[] = {
    {"test", builtin_test, "Run system test"},
    {"logo", builtin_logo, "Display system logo"},
    {"pwd",  builtin_pwd,  "Print working directory"},
    {"clear",builtin_clear,"Clear the screen"},
    {"cd",   builtin_cd,   "Change directory"},
    {"mkdir",builtin_mkdir,"Make directory"},
    {"rmdir",builtin_rmdir,"Remove directory"},
    {"rm",   builtin_rm,   "Remove file"},
    {"ls",   builtin_ls,   "List directory contents"},
    {"cat",  builtin_cat,  "Concatenate and display file content"},
    {"exit", builtin_exit, "Exit the shell"},
    {NULL, NULL, NULL}
};

static void execute(int argc, char *argv[]) {
    if (argc == 0) return;
    char *cmd_name = argv[0];

    // find inner command
    const cmd_t *ptr = cmd_table;
    while (ptr->name) {
        if (!strcmp(cmd_name, ptr->name)) {
            ptr->handler(argc, argv);
            return;
        }
        ptr++;
    }

    // todo: outer command

    // not found
    printf("osh: command not found: %s\n", cmd_name);
}

void readline(char *buf, int count) {
    char *ptr = buf;
    u32 idx = 0;

    while (idx < count - 1) {
        if (read(stdin, ptr + idx, 1) == -1) break;

        char ch = ptr[idx];

        if (ch == '\n' || ch == '\r') {
            ptr[idx] = '\0';
            write(stdout, "\n", 1);
            return;
        } else if ( ch == '\b' || ch == 0x7F) {
            if (idx > 0) {
                idx--;
                write(stdout, "\b \b", 3);  // left, printf ' ', left  
            }
        } else if (ch == '\t') {
            continue;
        } else {
            write(stdout, &ch, 1);
            idx++;
        }
    }
    buf[idx] = '\0';    // cos count - 1
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

    argv[argc] = NULL;   // end flag
    return argc;
}

int osh_main() {
    memset(cmd, 0, sizeof(cmd));
    
    getcwd(cwd, MAX_PATH_LEN);

    builtin_logo(0, NULL);

    while (true) {
        print_prompt();
        readline(cmd, sizeof(cmd));

        if (cmd[0] == 0) continue;  // empty command '\n'
        
        int argc = cmd_parse(cmd, argv, ' ');
        if (argc > 0) {
            execute(argc, argv);
        }
    }

    return 0;
}