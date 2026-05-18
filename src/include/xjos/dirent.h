#ifndef XJOS_DIRENT_H
#define XJOS_DIRENT_H

#include <xjos/types.h>

#ifndef MAXNAMELEN
#define MAXNAMELEN 64
#endif

typedef struct dentry_t {
    u32 nr;
    u32 length;
    u32 namelen;
    char name[MAXNAMELEN];
} dentry_t;

#endif /* XJOS_DIRENT_H */
