#ifndef XJOS_NET_ADDR_H
#define XJOS_NET_ADDR_H

#include <net/types.h>

// mac and ip copy
void eth_addr_copy(eth_addr_t dst, eth_addr_t src);
void ip_addr_copy(ip_addr_t dst, ip_addr_t src);

// 判断地址是否全为 0
bool eth_addr_isany(eth_addr_t addr);

// 比较两 mac 地址是否相等
bool eth_addr_cmp(eth_addr_t addr1, eth_addr_t addr2);

// str -> ip_addr_t
err_t inet_aton(const char *cp, ip_addr_t addr);

bool ip_addr_cmp(ip_addr_t addr1, ip_addr_t addr2);

// 同一子网
bool ip_addr_maskcmp(ip_addr_t addr, ip_addr_t addr2, ip_addr_t mask);

// 判断地址是否是广播地址
bool ip_addr_isbroadcast(ip_addr_t addr, ip_addr_t mask);

// 判断地址是否全为 0
bool ip_addr_isany(ip_addr_t addr);

// 判断地址是否为多播地址
bool ip_addr_ismulticast(ip_addr_t addr);

#endif /* !XJOS_NET_ADDR_H */