#include <xjos/net.h>
#include <xjos/list.h>
#include <xjos/arena.h>
#include <xjos/string.h>
#include <xjos/debug.h>
#include <xjos/errno.h>
#include <xjos/assert.h>
#include <xjos/task.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

// ICMP 查询响应
static err_t icmp_echo_reply(netif_t *netif, pbuf_t *pbuf) {
    ip_t *ip = pbuf->eth->ip;
    // 广播和多播检测
    if (ip_addr_isbroadcast(ip->dst, netif->netmask) || ip_addr_ismulticast(ip->dst))
        return -EPROTO;
    if (!ip_addr_cmp(ip->dst, netif->ipaddr))
        return -EPROTO;

    /*
        原地修改包，构造echo reply
    */
    icmp_t *icmp = ip->icmp;
    icmp->type = ICMP_ER;
    icmp->chksum = 0;

    u16 len = ip->length - sizeof(ip_t);
    icmp->chksum = ip_chksum(icmp, len);

    LOGK("IP ICMP ECHO REPLY: %r -> %r \n", ip->dst, ip->src);

    pbuf->count++;
    return ip_output(netif, pbuf, ip->src, IP_PROTOCOL_ICMP, len);
}

err_t icmp_input(netif_t *netif, pbuf_t *pbuf) {
    ip_t *ip = pbuf->eth->ip;
    icmp_t *icmp = ip->icmp;

    switch (icmp->type) {
        case ICMP_ER:
            LOGK("IP ICMP REPLY: %r -> %r \n", ip->src, ip->dst);
            break;
        case ICMP_ECHO:
            return icmp_echo_reply(netif, pbuf);
        default:
            LOGK("IP ICMP other: %r -> %r\n", ip->src, ip->dst);
            break;
    }

    return EOK;
}

err_t icmp_echo(netif_t *netif, pbuf_t *pbuf, ip_addr_t dst) {
    ip_t *ip = pbuf->eth->ip;
    icmp_echo_t *echo = ip->echo;

    echo->code = 0;
    echo->type = ICMP_ECHO;
    echo->id = 1;
    echo->seq = 1;

    char message[] = "XJOS ICMP ECHO REQUEST";

    strcpy((char *)echo->payload, message);

    u32 len = sizeof(icmp_echo_t) + sizeof(message);

    echo->checksum = 0;
    echo->checksum = ip_chksum(echo, len);

    LOGK("IP ICMP ECHO: %r \n", dst);
    return ip_output(netif, pbuf, dst, IP_PROTOCOL_ICMP, len);
}

void icmp_init() {
    
}