#include <xjos/types.h>
#include <xjos/stdio.h>
#include <xjos/syscall.h>
#include <xjos/string.h>
#include <fs/fs.h>
#include <xjos/net.h>
#include <xjos/errno.h>

#define BUFLEN 2048

static char tx_buf[BUFLEN];
static char rx_buf[BUFLEN];

static int ping_input(ip_t *ip, ip_addr_t addr, size_t bytes) {
    if (ip->proto != IP_PROTOCOL_ICMP)
        return EOK;

    icmp_echo_t *echo = ip->echo;
    printf("%d bytes from %r: icmp_seq=%d ttl=%d icmp=%d\n",
           bytes, ip->src, echo->seq, ip->ttl, echo->type);
    return EOK;
}

int cmd_ping(int argc, char **argv, char **envp) {
    if (argc < 2) {
        printf("no ip address\n");
        return EOF;
    }

    ip_addr_t addr;
    if (inet_aton(argv[1], addr) != EOK) {
        printf("ip address error\n");
        return -EADDR;
    }

    fd_t fd = socket(AF_INET, SOCK_RAW, PROTO_ICMP);
    if (fd < 0) {
        printf("open socket error.\n");
        return -ESOCKET;
    }

    int opt = 2000;
    int ret = setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &opt, 4);
    if (ret < 0) {
        printf("set timeout error.\n");
        goto rollback;
    }

    ip_t *ip = (ip_t *)tx_buf;
    ip_addr_copy(ip->dst, addr);
    ip->proto = IP_PROTOCOL_ICMP;

    icmp_echo_t *echo = ip->echo;

    echo->code = 0;
    echo->type = ICMP_ECHO;
    echo->id = 1;
    echo->seq = 0;

    char message[] = "xjos icmp echo 1234567890 asdfghjkl;'";
    strcpy((char *)echo->payload, message);

    u32 len = sizeof(icmp_echo_t) + sizeof(message);

    int count = 4;
    while (count--) {
        echo->seq += 1;
        echo->checksum = 0;
        echo->checksum = ip_chksum(echo, len);

        ret = send(fd, tx_buf, len + sizeof(ip_t), 0);
        if (ret < 0) {
            printf("send error\n");
            goto rollback;
        }

        ret = recv(fd, rx_buf, sizeof(rx_buf), 0);
        if (ret == -ETIME) {
            printf("ping %r timeout...\n", addr);
            continue;
        }

        if (ret < 0) {
            printf("recv error\n");
            goto rollback;
        }

        ret = ping_input((ip_t *)rx_buf, addr, ret);
        if (ret < 0) {
            printf("input error\n");
            goto rollback;
        }

        sleep(1000);
    }

rollback:
    if (fd > 0)
        close(fd);
    return 0;
}

#ifndef XJOS_BUSYBOX_APPLET
int main(int argc, char **argv, char **envp) {
    return cmd_ping(argc, argv, envp);
}
#endif