#ifndef XJOS_DIRENT_H
#define XJOS_DIRENT_H

#include <xjos/types.h>

#ifndef NAME_LEN
#define NAME_LEN 14
#endif

typedef struct dentry_t {
    u16 nr;
    char name[NAME_LEN];
} dentry_t;

#endif /* XJOS_DIRENT_H */
