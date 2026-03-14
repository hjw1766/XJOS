#ifndef XJOS_NET_ETH_H
#define XJOS_NET_ETH_H


#include <net/types.h>
#include <xjos/types.h>


#define ETH_CRC_LEN 4

typedef struct eth_t {
    eth_addr_t dst; // 源 MAC 地址
    eth_addr_t src; // 目的 MAC 地址
    u16 type;       // 以太网帧类型，表示上层协议类型，如 IPv4、ARP 等
    u8 payload[0]; // 以太网帧的有效载荷，长度为 (总长度 - 14 - CRC 长度)
} _packed eth_t;



#endif  // XJOS_NET_ETH_H