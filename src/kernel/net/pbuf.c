#include <xjos/net.h>
#include <xjos/memory.h>
#include <xjos/arena.h>
#include <xjos/string.h>
#include <xjos/assert.h>
#include <xjos/debug.h>


#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

static list_t free_buf_list;
static size_t pbuf_count = 0;

// get free pbuf
pbuf_t *pbuf_get() {
    pbuf_t *pbuf = NULL;
    if (list_empty(&free_buf_list)) {
        u32 page = alloc_kpage(1);
        pbuf = (pbuf_t *)page;
        list_push(&free_buf_list, &pbuf->node);

        page += PAGE_SIZE / 2;
        pbuf = (pbuf_t *)page;
        list_push(&free_buf_list, &pbuf->node);

        pbuf_count += 2;
        LOGK("pbuf count %d\n", pbuf_count);
    }

    pbuf = element_entry(pbuf_t, node, list_popback(&free_buf_list));

    assert(((u32)pbuf & 0x7ff) == 0);

    pbuf->count = 1;
    return pbuf;
}

// put pbuf
void pbuf_put(pbuf_t *pbuf) {
    assert(((u32)pbuf & 0x7ff) == 0);

    assert(pbuf->count > 0);
    pbuf->count--;
    if (pbuf->count > 0) {
        assert(pbuf->node.next && pbuf->node.prev);
        return;
    }

    list_push(&free_buf_list, &pbuf->node);
}

void pbuf_init() {
    list_init(&free_buf_list);
}