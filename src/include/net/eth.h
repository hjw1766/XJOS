#ifndef XJOS_NET_ETH_H
#define XJOS_NET_ETH_H


#include <net/types.h>
#include <net/arp.h>

#define ETH_FCS_LEN 4

enum {
    ETH_TYPE_IPV4 = 0x0800, // IPv4 协议
    ETH_TYPE_ARP = 0x0806,  // ARP 协议
    ETH_TYPE_IPV6 = 0x86DD, // IPv6 协议
};

typedef struct eth_t {
    eth_addr_t dst; // 源 MAC 地址
    eth_addr_t src; // 目的 MAC 地址
    u16 type;       // 以太网帧类型，表示上层协议类型，如 IPv4、ARP 等

    union {
        u8 payload[0]; // 以太网帧负载，实际长度由上层协议决定
        arp_t arp[0];     // ARP 协议数据
    };

} _packed eth_t;

err_t eth_input(netif_t *netif, pbuf_t *pbuf);
err_t eth_output(netif_t *netif, pbuf_t *pbuf, eth_addr_t dst, u16 type, u32 len);

#endif  // XJOS_NET_ETH_H