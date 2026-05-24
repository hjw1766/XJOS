#ifndef XJOS_NET_NETIF_H
#define XJOS_NET_NETIF_H

#include <xjos/list.h>
#include <net/types.h>

enum {
    NETIF_LOOPBACK = 0x00000001,

    NETIF_IP_TX_CHECKSUM_OFFLOAD = 0x00010000,
    NETIF_IP_RX_CHECKSUM_OFFLOAD = 0x00020000,
    NETIF_UDP_RX_CHECKSUM_OFFLOAD = 0x00040000,
    NETIF_UDP_TX_CHECKSUM_OFFLOAD = 0x00080000,
    NETIF_TCP_RX_CHECKSUM_OFFLOAD = 0x00100000,
    NETIF_TCP_TX_CHECKSUM_OFFLOAD = 0x00200000,
};

typedef struct netif_t {
    list_node_t node;
    char name[16];

    list_t rx_pbuf_list;    // 接收缓冲区链表
    list_t tx_pbuf_list;    // 发送缓冲区链表

    eth_addr_t hwaddr;      // Mac

    ip_addr_t ipaddr;       // IP 地址
    ip_addr_t netmask;      // 子网掩码
    ip_addr_t gateway;      // 默认网关

    void *nic;              // 网卡设备指针
    void (*nic_output)(struct netif_t *netif, pbuf_t *pbuf);

    u32 flags;
} netif_t;

// 创建虚拟网卡
netif_t *netif_create();

// 初始虚拟网卡
netif_t *netif_setup(void *nic, eth_addr_t hwaddr, void *output);

netif_t *netif_get();

// ip 路由选择
netif_t *netif_route(ip_addr_t addr);

void netif_remove(netif_t *netif);

void netif_input(netif_t *netif, pbuf_t *pbuf);

void netif_output(netif_t *netif, pbuf_t *pbuf);


#endif /* !XJOS_NET_NETIF_H */