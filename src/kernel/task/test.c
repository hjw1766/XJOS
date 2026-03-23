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

    memcpy(pbuf->eth->dst, "\xff\xff\xff\xff\xff\x00", 6);
    memcpy(pbuf->eth->src, "\x5a\xab\xcc\x5a\x5a\x33", 6);
    pbuf->eth->type = 0x0033;

    int len = 1500;
    pbuf->length = len + sizeof(eth_t);
    memset(pbuf->eth->payload, 'A', len);

    netif_output(netif, pbuf);
    return EOK;
}