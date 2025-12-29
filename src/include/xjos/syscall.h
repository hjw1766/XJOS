#ifndef XJOS_SYSCALL_H
#define XJOS_SYSCALL_H


#include <xjos/types.h>
#include <fs/stat.h>

#if 0
#include <asm/unistd_32.h>
#endif


typedef enum {
    SYS_NR_TEST,
    SYS_NR_EXIT = 1,
    SYS_NR_FORK = 2,
    SYS_NR_READ = 3,
    SYS_NR_WRITE = 4,
    SYS_NR_OPEN = 5,
    SYS_NR_CLOSE = 6,
    SYS_NR_WAITPID = 7,
    SYS_NR_CREAT = 8,
    SYS_NR_LINK = 9,
    SYS_NR_UNLINK = 10,
    SYS_NR_CHDIR = 12,
    SYS_NR_TIME = 13,
    SYS_NR_STAT = 18,
    SYS_NR_LSEEK = 19,
    SYS_NR_GETPID = 20,
    SYS_NR_FSTAT = 28,
    SYS_NR_SYNC = 36,
    SYS_NR_MKDIR = 39,
    SYS_NR_RMDIR = 40,
    SYS_NR_BRK = 45,
    SYS_NR_UMASK = 60,
    SYS_NR_CHROOT = 61,
    SYS_NR_GETPPID = 64,
    SYS_NR_READDIR = 89,
    SYS_NR_YIELD = 158,
    SYS_NR_SLEEP = 162,
    SYS_NR_GETCWD = 183,
    SYS_NR_CLEAR = 200,
}syscall_t;


u32 test();
void yield();
void sleep(u32 ms);

pid_t waitpid(pid_t pid, int32 *status);

pid_t getpid();
pid_t getppid();

pid_t fork();

void exit(int status);

int32 brk(void *addr);

fd_t open(char *filename, int flags, int mode);

fd_t creat(char *filename, int mode);

void close(fd_t fd);

int read(fd_t fd, char *buf, int len);
int write(fd_t fd, char *buf, int len);
int lseek(fd_t fd, off_t offset, int whence);
int readdir(fd_t fd, void *dir, int count);

char *getcwd(char *buf, size_t size);
int chdir(char *pathname);
int chroot(char *pathname);

int mkdir(char *pathname, mode_t mode);
int rmdir(char *pathname);

int link(char *oldname, char *newname);
int unlink(char *filename);

time_t time();

mode_t umask(mode_t mask);

void sync();

void clear();

int stat(char *filename, stat_t *statbuf);
int fstat(fd_t fd, stat_t *statbuf);

#endif /* XJOS_SYSCALL_H */