#include <xjos/types.h>
#include <xjos/stdio.h>
#include <xjos/syscall.h>
#include <xjos/string.h>
#include <fs/fs.h>
#include <xjos/net.h>

#define BUFLEN 0x1000

static char buf[BUFLEN];

int cmd_pkt(int argc, char **argv, char **envp) {
    int fd = socket(AF_PACKET, SOCK_RAW, 0);
    if (fd < EOK) {
        printf("open socket error\n");
        goto rollback;
    }

    msghdr_t msg;
    iovec_t iov;

    sockaddr_ll_t saddr;
    eth_addr_copy(saddr.addr, "\x5a\x5a\x5a\x5a\x5a\x33");
    saddr.family = AF_PACKET;

    int ret = bind(fd, (sockaddr_t *)&saddr, sizeof(sockaddr_ll_t));
    if (ret < EOK) {
        printf("bind error\n");
        goto rollback;
    }

    msg.name = (sockaddr_t *)&saddr;
    msg.namelen = sizeof(saddr);

    msg.iov = &iov;
    msg.iovlen = 1;

    iov.base = buf;
    iov.size = sizeof(buf);

    printf("receiving...\n");
    ret = recvmsg(fd, &msg, 0);
    printf("recvmsg %d\n", ret);

    eth_t *eth = (eth_t *)iov.base;

    printf("recv eth %m -> %m : %#x\n", eth->src, eth->dst, ntohs(eth->type));

    eth_addr_t addr;
    eth_addr_copy(addr, eth->dst);
    eth_addr_copy(eth->dst, eth->src);
    eth_addr_copy(eth->src, addr);

    char payload[] = "this is ack message"; // acknowledgement

    strcpy(eth->payload, payload);

    iov.size = sizeof(eth_t) + sizeof(payload);

    sendmsg(fd, &msg, 0);

rollback:
    if (fd > 0) {
        close(fd);
    }
    return EOK;
}



#ifndef XJOS_BUSYBOX_APPLET
int main(int argc, char **argv, char **envp) {
    return cmd_pkt(argc, argv, envp);
}
#endif