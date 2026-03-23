#ifndef XJOS_NET_NETIF_H
#define XJOS_NET_NETIF_H

#include <xjos/list.h>
#include <net/types.h>

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
} netif_t;

// 初始虚拟网卡
netif_t *netif_setup(void *nic, eth_addr_t hwaddr, void *output);

netif_t *netif_get();

void netif_remove(netif_t *netif);

void netif_input(netif_t *netif, pbuf_t *pbuf);

void netif_output(netif_t *netif, pbuf_t *pbuf);


#endif /* !XJOS_NET_NETIF_H */