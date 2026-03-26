#include <xjos/net.h>
#include <xjos/string.h>
#include <xjos/debug.h>
#include <xjos/errno.h>
#include <xjos/assert.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

//rx frame
err_t eth_input(netif_t *netif, pbuf_t *pbuf) {
    eth_t *eth = (eth_t *)pbuf->eth;
    u16 type = ntohs(eth->type);

    switch (type) {
        case ETH_TYPE_IPV4:
            LOGK("ETH %m -> %m IP4, %d\n", eth->src, eth->dst, pbuf->length);
            break;
        case ETH_TYPE_IPV6:
            LOGK("ETH %m -> %m IP6, %d\n", eth->src, eth->dst, pbuf->length);
            break;
        case ETH_TYPE_ARP:
            LOGK("ETH %m -> %m ARP, %d\n", eth->src, eth->dst, pbuf->length);
            break;
        default:
            LOGK("ETH %m -> %m UNKNOWN, %d\n", eth->src, eth->dst, pbuf->length);
            return -EPROTO;
    }

    return EOK;
}

err_t eth_output(netif_t *netif, pbuf_t *pbuf, eth_addr_t dst, u16 type, u32 len) {
    pbuf->eth->type = htons(type);
    eth_addr_copy(pbuf->eth->dst, dst);
    eth_addr_copy(pbuf->eth->src, netif->hwaddr);

    netif_output(netif, pbuf);

    return EOK;
}

void eth_init() {
    
}