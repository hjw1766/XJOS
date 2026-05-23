#ifndef XJOS_NET_PKT_H
#define XJOS_NET_PKT_H

#include <net/types.h>
#include <net/pbuf.h>
#include <xjos/list.h>

typedef struct pkt_pcb_t {
    list_node_t node;
    list_t rx_pbuf_list;
    eth_addr_t laddr;
    eth_addr_t raddr;

    struct task_t *rx_waiter;
} pkt_pcb_t;

err_t pkt_input(netif_t *netif, pbuf_t *pbuf);

#endif /*XJOS_NET_PKT_H*/