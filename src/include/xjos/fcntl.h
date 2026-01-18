#ifndef XJOS_FCNTL_H
#define XJOS_FCNTL_H

#include <xjos/types.h>

// File open flags
#define O_RDONLY 00
#define O_WRONLY 01
#define O_RDWR 02
#define O_ACCMODE 03
#define O_CREAT 00100
#define O_EXCL 00200
#define O_NOCTTY 00400
#define O_TRUNC 01000
#define O_APPEND 02000
#define O_NONBLOCK 04000

// lseek whence
typedef enum whence_t {
    SEEK_SET = 1,
    SEEK_CUR,
    SEEK_END
} whence_t;

#endif /* XJOS_FCNTL_H */
