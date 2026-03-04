#include <xjos/timer.h>
#include <xjos/interrupt.h>
#include <xjos/task.h>
#include <xjos/mutex.h>
#include <xjos/arena.h>
#include <xjos/errno.h>
#include <xjos/debug.h>
#include <xjos/assert.h>


#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

extern u32 volatile jiffies;
extern u32 jiffy;

static list_t timer_list;

static timer_t *timer_get() {
    timer_t *timer = (timer_t *)kmalloc(sizeof(timer_t));
    return timer;
}

void timer_put(timer_t *timer) {
    list_remove(&timer->node);
    kfree(timer);
}

void default_timeout(timer_t *timer) {
    assert(timer->task->node.next);
    task_unblock(timer->task, -ETIME);
}

timer_t *timer_add(u32 expire_ms, handler_t handler, void *arg, struct task_t *task) {
    timer_t *timer = timer_get();
    timer->task = task;
    timer->expires = jiffies + expire_ms / jiffy;
    timer->handler = handler;
    timer->arg = arg;
    timer->active = false;

    list_insert_sort(&timer_list, &timer->node, list_node_offset(timer_t, node, expires));

    return timer;
}

u32 timer_expires() {
    if (list_empty(&timer_list)){
        return EOF;
    }

    timer_t *timer = element_entry(timer_t, node, timer_list.head.next);
    return timer->expires;
}

void timer_init() {
    LOGK("timer init...\n");
    list_init(&timer_list);
}

// 从定时器链表中找到task相关的定时器
void timer_remove(task_t *task) {
    list_t *list = &timer_list;

    for(list_node_t *ptr = list->head.next; ptr != &list->head;) {
        timer_t *timer = element_entry(timer_t, node, ptr);
        ptr = ptr->next;
        if (timer->task != task)
            continue;
        timer_put(timer);
    }
}

bool timer_wakeup() {
    bool woke = false;
    while (!list_empty(&timer_list)) {
        timer_t *timer = element_entry(timer_t, node, timer_list.head.next);
        if (timer->expires > jiffies) {
            break;
        }

        // 1.先摘链表
        list_remove(&timer->node);
        timer->active = true;

        // 2.切断与任务的关系
        if (timer->task && timer->task->block_timer == timer)
            timer->task->block_timer = NULL;

        // 3.回调
        if (timer->handler) {
            timer->handler(timer);
        } else {
            default_timeout(timer);
        }

        woke = true;
        kfree(timer);
    }
    return woke;
}