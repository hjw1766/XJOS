#ifndef XJOS_NET_CHKSUM_H
#define XJOS_NET_CHKSUM_H

#include <net/types.h>

u32 eth_fcs(void *data, int len);
u16 chksum(void *data, int len);

#endif  // XJOS_NET_CHKSUM_H