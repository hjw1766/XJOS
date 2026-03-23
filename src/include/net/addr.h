#ifndef XJOS_NET_ADDR_H
#define XJOS_NET_ADDR_H

#include <net/types.h>

// mac and ip copy
void eth_addr_copy(eth_addr_t dst, eth_addr_t src);
void ip_addr_copy(ip_addr_t dst, ip_addr_t src);

// str -> ip_addr_t
err_t inet_aton(const char *cp, ip_addr_t addr);

#endif /* !XJOS_NET_ADDR_H */