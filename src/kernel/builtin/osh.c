#include <xjos/syscall.h>
#include <libc/string.h>
#include <xjos/stdlib.h>
#include <libc/stdio.h>
#include <libc/assert.h>
#include <xjos/time.h>
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

static const cmd_t cmd_table[];

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

static void strftime(time_t stamp, char *buf) {
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

static void parsemode(int mode, char *buf) {
    memset(buf, '-', 10);
    buf[10] = '\0';
    char *ptr = buf;

    switch (mode & IFMT) {
        case IFREG: *ptr = '-'; break;
        case IFBLK: *ptr = 'b'; break;
        case IFDIR: *ptr = 'd'; break;
        case IFCHR: *ptr = 'c'; break;
        case IFIFO: *ptr = 'p'; break;
        case IFLNK: *ptr = 'l'; break;
        case IFSOCK: *ptr = 's'; break;
        default: *ptr = '?'; break;
    }
    ptr++;

    for (int i = 6; i >= 0; i -= 3) {
        int fmt = (mode >> i) & 0x7;
        *ptr++ = (fmt & 0b100) ? 'r' : '-';
        *ptr++ = (fmt & 0b010) ? 'w' : '-';
        *ptr++ = (fmt & 0b001) ? 'x' : '-';
    }
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

    int terminal_width = 80; 
    int logo_width = 23;     
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

void builtin_help(int argc, char *argv[]) {
    printf("Available commands:\n");
    const cmd_t *ptr = cmd_table;
    while (ptr->name) {
        printf("  %-8s - %s\n", ptr->name, ptr->desc);
        ptr++;
    }
}

void builtin_date(int argc, char *argv[]) {
    strftime(time(), buf);
    printf("System time: %s\n", buf);
}

void builtin_mount(int argc, char *argv[]) {
    if (argc < 3) {
        printf("mount: missing operand\n");
        printf("Usage: mount <source> <target>\n");
        return;
    }
    mount(argv[1], argv[2], 0);
}

void builtin_umount(int argc, char *argv[]) {
    if (argc < 2) {
        printf("umount: missing operand\n");
        printf("Usage: umount <target>\n");
        return;
    }
    umount(argv[1]);
}

void builtin_mkfs(int argc, char *argv[]) {
    if (argc < 2) {
        printf("mkfs: missing operand\n");
        printf("Usage: mkfs <device>\n");
        return;
    }
    mkfs(argv[1], 0);
}

void builtin_mkdir(int argc, char *argv[]) {
    if (argc < 2) {
        printf("mkdir: missing operand\n");
        printf("Usage: mkdir <directory>\n");
        return;
    }
    
    if (mkdir(argv[1], 0755) == EOF) {
        printf("mkdir: cannot create directory '%s': ", argv[1]);
        // exists check
        stat_t statbuf;
        if (stat(argv[1], &statbuf) == 0) {
            printf("Directory exists\n");
            return;
        }

        printf("Permission denied or parent directory does not exist\n");
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
    if (argc < 2) return;   // todo cd ~

    if (chdir(argv[1]) == EOF) {
        printf("cd: %s: No such file or directory\n", argv[1]);
    }
}

void builtin_ls(int argc, char *argv[]) {
    bool list = false;
    char *target = NULL;
    
    // parse args
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-l")) {
            list = true;
        } else {
            target = argv[i];
        }
    }

    // default target: cwd
    if (!target) {
        getcwd(cwd, MAX_PATH_LEN);
        target = cwd;
    }

    fd_t fd = open(target, O_RDONLY, 0);
    if (fd == EOF) {
        printf("ls: cannot access '%s': No such file or directory\n", target);
        return;
    }


    dentry_t entry;
    while (readdir(fd, &entry, 1) != EOF) {
        if (!entry.nr) continue; // skip empty entry
        if (!strcmp(entry.name, ".") || !strcmp(entry.name, "..")) continue;
        if (!list) {
            printf("%s  ", entry.name);
            continue;
        }
        // -l
        stat_t statbuf;
        stat(entry.name, &statbuf);
        parsemode(statbuf.mode, buf);
        printf("%s ", buf);

        strftime(statbuf.ctime, buf);
        printf("% 2d % 2d % 2d % 2d %s %s\n",
               statbuf.nlinks,
               statbuf.uid,
               statbuf.gid,
               statbuf.size,
               buf,
               entry.name);
    }
    if (!list) printf("\n");
    close(fd);
}

void builtin_cat(int argc, char *argv[]) {
    if (argc < 2) {
        printf("cat: missing operand\n");
        printf("Usage: cat <file>\n");
        return;
    }

    stat_t statbuf;
    if (stat(argv[1], &statbuf) == 0) {
        if ((statbuf.mode & IFMT) == IFDIR) {
            printf("cat: %s: Is a directory\n", argv[1]);
            return;
        }
    }

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
    {"date", builtin_date, "Display current system date and time"},
    {"help", builtin_help, "Display this help message"},
    {"mount",builtin_mount,"Mount a filesystem"},
    {"umount",builtin_umount,"Unmount a filesystem"},
    {"mkfs", builtin_mkfs, "Create a filesystem"},
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