#include <xjos/net.h>
#include <xjos/list.h>
#include <xjos/arena.h>
#include <xjos/string.h>
#include <xjos/task.h>
#include <xjos/debug.h>
#include <xjos/assert.h>
#include <xjos/errno.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

static err_t arp_reply(netif_t *netif, pbuf_t *pbuf) {
    arp_t *arp = pbuf->eth->arp;
    
    LOGK("ARP Request from %r\n", arp->ipsrc);

    arp->opcode = htons(ARP_OP_REPLY);

    eth_addr_copy(arp->hwdst, arp->hwsrc);
    ip_addr_copy(arp->ipdst, arp->ipsrc);

    eth_addr_copy(arp->hwsrc, netif->hwaddr);
    ip_addr_copy(arp->ipsrc, netif->ipaddr);

    pbuf->count++;
    return eth_output(netif, pbuf, arp->hwdst, ETH_TYPE_ARP, sizeof(arp_t));
}

err_t arp_input(netif_t *netif, pbuf_t *pbuf) {
    arp_t *arp = pbuf->eth->arp;

    if (ntohs(arp->hwtype) != ARP_HARDWARE_ETH) {
        LOGK("Unsupported ARP hardware type: %d\n", ntohs(arp->hwtype));
        return -EPROTO;
    }

    if (ntohs(arp->proto) != ARP_PROTOCOL_IP)
        return -EPROTO;   

    if (!ip_addr_cmp(netif->ipaddr, arp->ipdst))
        return -EPROTO; // 目的 IP 地址不匹配，丢弃 ARP 请求       

    u16 type = ntohs(arp->opcode);
    switch (type) {
        case ARP_OP_REQUEST:
            return arp_reply(netif, pbuf);
        case ARP_OP_REPLY:
            LOGK("arp reply %r -> %m\n", arp->ipsrc, arp->hwsrc);
            break;
        default:
            return -EPROTO; // 不支持的 ARP 操作类型
    }

    return EOK;
}

void arp_init() {
    
}