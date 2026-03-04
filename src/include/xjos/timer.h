#ifndef XJOS_TIMER_H
#define XJOS_TIMER_H


#include <xjos/types.h>
#include <xjos/list.h>


typedef void *handler_t;

typedef struct timer_t {
    list_node_t node;                  // 链表节点
    struct task_t *task;               // 相关任务
    u32 expires;                       // 超时时间
    void (*handler)(struct timer_t *); // 超时处理函数
    void *arg;                         // 参数
    bool active;                       // 激活状态
} timer_t;

// 添加定时器
timer_t *timer_add(u32 expire_ms, handler_t handler, void *arg, struct task_t *task);
// 释放定时器
void timer_put(timer_t *timer);
// 唤醒定时器
bool timer_wakeup();
// 移除 task 相关的全部定时器
void timer_remove(struct task_t *task);


#endif // XJOS_TIMER_H
