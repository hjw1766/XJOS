#ifndef XJOS_NET_PBUF_H
#define XJOS_NET_PBUF_H

#include <xjos/types.h>
#include <xjos/list.h>
#include <net/eth.h>

typedef struct pbuf_t {
    list_node_t node; // 链表节点
    size_t length; // 数据长度
    u32 count; // 引用计数

    union {
        u8 payload[0]; // 数据负载
        eth_t eth[0]; // 以太网帧
    };
} pbuf_t;


pbuf_t *pbuf_get();
void pbuf_put(pbuf_t *pbuf);


#endif /* XJOS_NET_PBUF_H */