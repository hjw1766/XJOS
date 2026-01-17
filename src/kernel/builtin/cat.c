#ifdef XJOS
#include <xjos/types.h>
#include <xjos/stdio.h>
#include <xjos/syscall.h>
#include <xjos/string.h>
#include <fs/fs.h>
#else
#include <stdio.h>
#include <string.h>
#include <sys/file.h>
#endif


#define BUFLEN 1024

char buf[BUFLEN];

int main(int argc, char const *argv[]) {
    if (argc < 2) {
        return EOF;
    }

    int fd = open((char *)argv[1], O_RDONLY, 0);
    if (fd == EOF) {
        printf("file %s not exists.\n", argv[1]);
        return EOF;
    }

    while (1) {
        int len = read(fd, buf, BUFLEN);
        if (len == EOF) {
            break;
        }
        write(1, buf, len);
    }
    close(fd);
    return 0;
}