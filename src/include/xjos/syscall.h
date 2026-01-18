#ifndef XJOS_SYSCALL_H
#define XJOS_SYSCALL_H
#include <xjos/syscall_nr.h>

// Syscall wrapper declarations are user-space only.
// Kernel code should call internal sys_* handlers directly.
#ifndef __KERNEL__
u32 test();
void yield();
void sleep(u32 ms);

pid_t waitpid(pid_t pid, int32 *status);

pid_t getpid();
pid_t getppid();

pid_t fork();

void exit(int status);

int brk(void *addr);
void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
int munmap(void *addr, size_t length);

fd_t open(char *filename, int flags, int mode);

fd_t creat(char *filename, int mode);

void close(fd_t fd);

int read(fd_t fd, char *buf, int len);
int write(fd_t fd, char *buf, int len);
int lseek(fd_t fd, off_t offset, int whence);
int readdir(fd_t fd, void *dir, int count);

int execve(char *filename, char *argv[], char *envp[]);

char *getcwd(char *buf, size_t size);
int chdir(char *pathname);
int chroot(char *pathname);

int mkdir(char *pathname, mode_t mode);
int rmdir(char *pathname);

int link(char *oldname, char *newname);
int unlink(char *filename);

int mount(char *devname, char *dirname, int flags);
int umount(char *target);

int mknod(char *filename, int mode, int dev);

time_t time();

mode_t umask(mode_t mask);

void sync();

void clear();

int stat(char *filename, stat_t *statbuf);
int fstat(fd_t fd, stat_t *statbuf);

int mkfs(char *devname, int icount);
#endif

#endif /* XJOS_SYSCALL_H */