#include <xjos/net.h>
#include <xjos/list.h>
#include <xjos/arena.h>
#include <xjos/string.h>
#include <xjos/task.h>
#include <xjos/debug.h>
#include <xjos/assert.h>
#include <xjos/errno.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

// ARP 缓存队列
static list_t arp_entry_list;

// ARP 刷新任务
static task_t *arp_task;

typedef struct arp_entry_t {
    list_node_t node;       // 链表节点
    eth_addr_t hwaddr;      // MAC 地址
    ip_addr_t ipaddr;       // IP 地址
    u32 expires;            // 失效时间
    u32 used;               // 使用次数
    u32 query;              // 查询时间
    u32 retry;              // 重试次数
    list_t pbuf_list;       // 等待队列
    netif_t *netif;         // 关联的网络接口
} arp_entry_t;

// get arp entry
static arp_entry_t *arp_entry_get(netif_t *netif, ip_addr_t addr) {
    arp_entry_t *entry = (arp_entry_t *)kmalloc(sizeof(arp_entry_t));
    entry->netif = netif;
    ip_addr_copy(entry->ipaddr, addr);
    eth_addr_copy(entry->hwaddr, (u8 *)ETH_BROADCAST);

    entry->expires = 0;
    entry->retry = 0;
    entry->used = 1;

    list_init(&entry->pbuf_list);
    list_insert_sort(&arp_entry_list, &entry->node, 
        list_node_offset(arp_entry_t, node, expires));

    return entry;
}

static void arp_entry_put(arp_entry_t *entry) {
    list_t *list = &entry->pbuf_list;
    while (!list_empty(list)) {
        pbuf_t *pbuf = list_entry(list_popback(list), pbuf_t, node);
        assert(pbuf->count == 1);
        pbuf_put(pbuf);
    }

    list_remove(&entry->node);
    kfree(entry);
}

static arp_entry_t *arp_lookup(netif_t *netif, ip_addr_t addr) {
    ip_addr_t query;
    if (!ip_addr_maskcmp(netif->ipaddr, addr, netif->netmask)) {
        ip_addr_copy(query, netif->gateway);
    } else {
        ip_addr_copy(query, addr);
    }

    list_t *list = &arp_entry_list;
    arp_entry_t *entry = NULL;

    // 遍历 ARP 缓存列表，查找匹配的 ARP 条目
    list_for_each_entry(entry, list, node) {
        if (ip_addr_cmp(entry->ipaddr, query) && entry->netif == netif) {
            return entry;
        }
    };

    // 如果没有找到匹配的 ARP 条目，则创建一个新的 ARP 条目并返回
    entry = arp_entry_get(netif, query);
    return entry;
}

extern time_t sys_time();

static err_t arp_query(arp_entry_t *entry) {
    if (entry->query + ARP_DELAY > sys_time()) {
        return -ETIME;
    }

    LOGK("ARP query %r...\n", entry->ipaddr);

    entry->query = sys_time();
    entry->retry++;

    // 查询次数超过限制，将 MAC 地址设置为广播地址
    if (entry->retry > ARP_RETRY) {
        eth_addr_copy(entry->hwaddr, (u8 *)ETH_BROADCAST);
    }

    // 重新构建 ARP 请求报文并发送
    pbuf_t *pbuf = pbuf_get();
    arp_t *arp = pbuf->eth->arp;

    arp->opcode = htons(ARP_OP_REQUEST);
    arp->hwtype = htons(ARP_HARDWARE_ETH);
    arp->hwlen = ARP_HARDWARE_ETH_LEN;
    arp->proto = htons(ARP_PROTOCOL_IP);
    arp->protolen = ARP_PROTOCOL_IP_LEN;

    eth_addr_copy(arp->hwdst, entry->hwaddr);
    ip_addr_copy(arp->ipdst, entry->ipaddr);
    ip_addr_copy(arp->ipsrc, entry->netif->ipaddr);
    eth_addr_copy(arp->hwsrc, entry->netif->hwaddr);

    eth_output(entry->netif, pbuf, entry->hwaddr, ETH_TYPE_ARP, sizeof(arp_t));
    return EOK;
}

static err_t arp_refresh(netif_t *netif, pbuf_t *pbuf) {
    arp_t *arp = pbuf->eth->arp;

    if (!ip_addr_maskcmp(arp->ipsrc, netif->ipaddr, netif->netmask)) {
        return -EADDR; // 源 IP 地址不匹配，丢弃 ARP 报文
    }

    arp_entry_t *entry = arp_lookup(netif, arp->ipsrc);

    // 拿到 ARP 条目，更新 MAC 地址和失效时间
    eth_addr_copy(entry->hwaddr, arp->hwsrc);
    entry->expires = sys_time() + ARP_ENTRY_TIMEOUT;
    entry->retry = 0;
    entry->used = 0;

    // 重新插入，确保列表按照失效时间排序
    list_remove(&entry->node);
    list_insert_sort(&arp_entry_list, &entry->node,
        list_node_offset(arp_entry_t, node, expires));

    list_t *list = &entry->pbuf_list;
    while (!list_empty(list)) {
        pbuf_t *pbuf = element_entry(pbuf_t, node, list_popback(list));
        eth_addr_copy(pbuf->eth->dst, entry->hwaddr);
        netif_output(netif, pbuf);
    }
    
    LOGK("ARP reply %r -> %m \n", arp->ipsrc, arp->hwsrc);
    return EOK;
}

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
            return arp_refresh(netif, pbuf);
        default:
            return -EPROTO; // 不支持的 ARP 操作类型
    }

    return EOK;
}

// 发送数据包到指定 IP 地址
err_t arp_eth_output(netif_t *netif, pbuf_t *pbuf, u8 *addr, u16 type, u32 len) {
    pbuf->eth->type = htons(type);
    eth_addr_copy(pbuf->eth->src, netif->hwaddr);
    pbuf->length = sizeof(eth_t) + len;

    arp_entry_t *entry = arp_lookup(netif, addr);
    if (entry->expires > sys_time()) {
        entry->used += 1;
        // 填入目的 MAC 地址并发送
        eth_addr_copy(pbuf->eth->dst, entry->hwaddr);
        netif_output(netif, pbuf);
        return EOK;
    }

    // ARP 条目不存在或已过期，将数据包加入等待队列并发送 ARP 请求
    list_push(&entry->pbuf_list, &pbuf->node);

    arp_query(entry);

    return EOK;
}

static void arp_thread() {
    list_t *list = &arp_entry_list;
    while (true) {
        task_sleep(ARP_REFRESH_DELAY);

        arp_entry_t *entry, *next;
        list_for_each_entry_safe_reverse(entry, next, list, node) {
            /* 1. 时间未过期 */
            if (entry->expires > sys_time()) {
                continue;
            }

            /* 2. 重试次数过多或未使用(清理) */
            if (entry->retry > ARP_RETRY || entry->used == 0) {
                arp_entry_put(entry);
            } else {
                arp_query(entry);   /* 3. 刷新 */
            }
        }
    }
}

void arp_init() {
    LOGK("Address Resolution Protocol init...\n");
    list_init(&arp_entry_list);
    arp_task = task_create_packet(arp_thread, "arp", NICE_DEFAULT);
}