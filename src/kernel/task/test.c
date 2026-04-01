#include <xjos/string.h>
#include <xjos/types.h>
#include <xjos/cpu.h>
#include <xjos/printk.h>
#include <xjos/debug.h>
#include <xjos/errno.h>
#include <xjos/net.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)


err_t sys_test() {
    pbuf_t *pbuf = pbuf_get();
    netif_t *netif = netif_get();    

    int len = 1500;
    memset(pbuf->eth->payload, 'A', len);

    ip_addr_t addr;
    // Keep test destination aligned with host bridge IP in src/utils/net.mk.
    assert(inet_aton("192.168.239.1", addr) == EOK);
    arp_eth_output(netif, pbuf, addr, 0x9000, len);

    return EOK;
}