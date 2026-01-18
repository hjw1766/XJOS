#include <xjos/types.h>
#include <xjos/string.h>
#include <xjos/stdio.h>
#include <xjos/syscall.h>

#include "applets.h"

static const char *bb_basename(const char *path) {
    const char *ptr = strrchr(path, '/');
    return ptr ? (ptr + 1) : path;
}

static int bb_streq(const char *a, const char *b) {
    return strcmp(a, b) == 0;
}

typedef int (*applet_fn_t)(int argc, char **argv, char **envp);

typedef struct {
    const char *name;
    applet_fn_t fn;
} applet_t;

static const applet_t applets[] = {
    {"ls", cmd_ls},
    {"cat", cmd_cat},
    {"echo", cmd_echo},
    {"env", cmd_env},
    {"sh", cmd_sh},
    {NULL, NULL},
};

static void usage(void) {
    printf("usage:\n");
    printf("  busybox <applet> [args...]\n");
    printf("  <applet> [args...]   (via hardlink name)\n");
    printf("applets: ls cat echo env sh\n");
}

int main(int argc, char **argv, char **envp) {
    if (argc <= 0 || argv == NULL || argv[0] == NULL) {
        usage();
        return EOF;
    }

    const char *self = bb_basename(argv[0]);
    const char *applet = self;

    // If invoked as busybox, the applet is argv[1].
    if (bb_streq(self, "busybox") || bb_streq(self, "busybox.out")) {
        if (argc < 2) {
            usage();
            return EOF;
        }
        applet = argv[1];
        argv += 1;
        argc -= 1;
    }

    for (const applet_t *it = applets; it->name; it++) {
        if (bb_streq(applet, it->name)) {
            return it->fn(argc, argv, envp);
        }

        // accept applet.out
        size_t an = strlen(it->name);
        if (strlen(applet) == an + 4 &&
            memcmp(applet, it->name, an) == 0 &&
            memcmp(applet + an, ".out", 4) == 0) {
            return it->fn(argc, argv, envp);
        }
    }

    printf("busybox: unknown applet: %s\n", applet);
    usage();
    return EOF;
}
