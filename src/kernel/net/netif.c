#include <xjos/net.h>
#include <xjos/list.h>
#include <xjos/task.h>
#include <drivers/device.h>
#include <xjos/arena.h>
#include <xjos/stdio.h>
#include <xjos/assert.h>
#include <xjos/debug.h>
#include <xjos/errno.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args);

static list_t netif_list;   // 虚拟网卡链表

static task_t *neti_task;   // rx
static task_t *neto_task;   // tx


netif_t *netif_setup(void *nic, eth_addr_t hwaddr, void *output) {
    netif_t *netif = kmalloc(sizeof(netif_t));
    sprintf(netif->name, "eth%d", list_len(&netif_list));

    list_push(&netif_list, &netif->node);
    list_init(&netif->rx_pbuf_list);
    list_init(&netif->tx_pbuf_list);

    eth_addr_copy(netif->hwaddr, hwaddr);

    assert(inet_aton("192.168.239.44", netif->ipaddr) == EOK);
    assert(inet_aton("255.255.255.0", netif->netmask) == EOK);
    assert(inet_aton("192.168.239.2", netif->gateway) == EOK);

    netif->nic = nic;
    netif->nic_output = output;

    return netif;
}

netif_t *netif_get() {
    list_t *list = &netif_list;
    if (list_empty(list)) {
        return NULL;
    }

    list_node_t *ptr = list->head.next;
    netif_t *netif = element_entry(netif_t, node, ptr);

    return netif;
}

void netif_remove(netif_t *netif) {
    list_remove(&netif->node);
    kfree(netif);
}

void netif_input(netif_t *netif, pbuf_t *pbuf) {
    list_push(&netif->rx_pbuf_list, &pbuf->node);
    if (neti_task->state == TASK_WAITING) {
        task_unblock(neti_task, EOK);
    }
}

void netif_output(netif_t *netif, pbuf_t *pbuf) {
    list_push(&netif->tx_pbuf_list, &pbuf->node);
    if (neto_task->state == TASK_WAITING) {
        task_unblock(neto_task, EOK);
    }
}

// rx task
static void neti_thread() {
    list_t *list = &netif_list;
    pbuf_t *pbuf;
    netif_t *netif;

    while (true) {
        int count = 0;

        list_for_each_entry(netif, &netif_list, node) {
            while (!list_empty(&netif->rx_pbuf_list)) {
                pbuf = element_entry(pbuf_t, node, list_popback(&netif->rx_pbuf_list));

                assert(!pbuf->node.next && !pbuf->node.prev);

                eth_input(netif, pbuf);
                
                pbuf_put(pbuf);
                count++;
            }
        }

        if (count == 0) {
            task_t *task = running_task();
            assert(task == neti_task);
            assert(task_block(task, NULL, TASK_WAITING, TIMELESS) == EOK);
        }
    }
}

// tx task
static void neto_thread() {
    list_t *list = &netif_list;
    pbuf_t *pbuf;
    netif_t *netif;

    while (true) {
        int count = 0;

        list_for_each_entry(netif, &netif_list, node) {
            while (!list_empty(&netif->tx_pbuf_list)) {
                pbuf = element_entry(pbuf_t, node, list_popback(&netif->tx_pbuf_list));
                assert(!pbuf->node.next && !pbuf->node.prev);

                LOGK("ETH SEND [%04X]: %m -> %m %d\n",
                     ntohs(pbuf->eth->type),
                     pbuf->eth->src,
                     pbuf->eth->dst,
                     pbuf->length);

                netif->nic_output(netif, pbuf);
                count++;
            }
        }

        if (count == 0) {
            task_t *task = running_task();
            assert(task == neto_task);
            assert(task_block(task, NULL, TASK_WAITING, TIMELESS) == EOK);
        }
    }
}

void netif_init() {
    list_init(&netif_list);
    neti_task = task_create_packet(neti_thread, "neti", NICE_DEFAULT);
    neto_task = task_create_packet(neto_thread, "neto", NICE_DEFAULT);
}
