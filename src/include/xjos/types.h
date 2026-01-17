#ifndef ONIX_TYPES_H
#define ONIX_TYPES_H

#include <xjos/xjos.h>

#define EOF (-1) // END OF FILE
#define EOS '\0'  // END OF STRING

#define NULL ((void*)0) // NULL pointer

// reserved(__LINE__)
#define CONCAT(x, y) x##y
#define RESERVED_TOKEN(x, y) CONCAT(x, y)
#define RESERVED RESERVED_TOKEN(reserved, __LINE__)

#ifndef __cplusplus
#define bool _Bool
#define true (1)
#define false (0)
#endif

#define weak __attribute__((__weak__))

#define noreturn __attribute__((__noreturn__))

#define _packed __attribute__((packed))  // * disable byte alignment 

#define _ofp __attribute__((optimize("omit-frame-pointer")))

#define _inline __attribute__((always_inline)) inline

typedef unsigned int size_t;

typedef char int8;
typedef short int16;
typedef int int32;
typedef long long int64;

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

typedef int32 pid_t;
typedef u32 time_t;
typedef u32 idx_t;
typedef int32 dev_t;

typedef u16 mode_t;

typedef int32 fd_t;
typedef enum {
    STDIN_FILENO,
    STDOUT_FILENO,
    STDERR_FILENO,
}std_fd_t;

typedef int32 off_t;    // file offset

#endif