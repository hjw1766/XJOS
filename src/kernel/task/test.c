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

    assert(inet_aton("8.8.8.8", addr) == EOK);

    pbuf_t *pbuf = pbuf_get();

    netif_t *netif = netif_route(addr);

    icmp_echo(netif, pbuf, addr);

    return EOK;
}