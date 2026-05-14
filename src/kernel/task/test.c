#include <xjos/string.h>
#include <xjos/types.h>
#include <xjos/cpu.h>
#include <xjos/printk.h>
#include <xjos/debug.h>
#include <xjos/errno.h>
#include <xjos/net.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)


err_t sys_test() {
    ip_addr_t addr;
    assert(inet_aton("192.168.239.2", addr) == EOK);

    pbuf_t *pbuf = pbuf_get();
    netif_t *netif = netif_route(addr);

    ip_t *ip = pbuf->eth->ip;
    u16 len = 128 - sizeof(ip_t) - sizeof(eth_t);
    memset(ip->payload, 'T', len);

    ip_output(netif, pbuf, addr, 254, len);

    return EOK;
}