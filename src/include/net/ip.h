#ifndef XJOS_NET_IP_H
#define XJOS_NET_IP_H


#include <net/types.h>
#include <net/icmp.h>
#include <net/udp.h>

#define IP_VERSION_4 4
#define IP_TTL 64

enum {
    IP_PROTOCOL_NONE = 0,
    IP_PROTOCOL_ICMP = 1,
    IP_PROTOCOL_TCP = 6,
    IP_PROTOCOL_UDP = 17,
    IP_PROTOCOL_TEST = 254,
};

#define IP_FLAG_NOFRAG 0b10
#define IP_FLAG_NOLAST 0b100

typedef struct ip_t {
    u8 header : 4;      // head length
    u8 version : 4;     // version
    u8 tos;            // type of service
    u16 length;         // data packet length

    // 分片
    u16 id;         // 标识，每发送一个分片该值加 1
    u8 offset0 : 5; // 分片偏移量高 5 位，以 8字节 为单位
    u8 flags : 3;   // 标志位，1：保留，2：不分片，4：不是最后一个分片
    u8 offset1;     // 分片偏移量低 8 位，以 8字节 为单位

    u8 ttl;        // 生存时间 Time To Live，每经过一个路由器该值减一，到 0 则丢弃
    u8 proto;      // 上层协议，1：ICMP 6：TCP 17：UDP
    u16 chksum;    // 校验和
    ip_addr_t src; // 源 IP 地址
    ip_addr_t dst; // 目的 IP 地址

    union {
        u8 payload[0];  // 载荷
        icmp_t icmp[0];     // ICMP 载荷
        icmp_echo_t echo[0];    // ICMP 回显载荷
        udp_t udp[0];           // udp
    };
} _packed ip_t;

err_t ip_input(netif_t *netif, pbuf_t *pbuf);
err_t ip_output(netif_t *netif, pbuf_t *pbuf, ip_addr_t dst, u8 proto, u16 len);

#endif // XJOS_NET_IP_H