#include <xjos/mutex.h>
#include <xjos/interrupt.h>
#include <xjos/task.h>
#include <xjos/task.h>
#include <xjos/debug.h>
#include <xjos/errno.h>


#define LOGK(fmt, args...) DEBUGK(fmt, ##args)


void sem_init(semaphore_t *sem) {
    sem->value = 1;
    list_init(&sem->waiters);
}


void sem_wait(semaphore_t *sem) {
    bool intr = interrupt_disable();

    task_t *current = running_task();

    while (sem->value == 0) 
        task_block(current, &sem->waiters, TASK_BLOCKED, TIMELESS);

    assert(sem->value == 1);
    
    sem->value--;
    assert(sem->value == 0);

    // task wakeup
    set_interrupt_state(intr);
}


void sem_post(semaphore_t *sem) {
    bool intr = interrupt_disable();

    assert(sem->value == 0);

    sem->value++;
    assert(sem->value == 1);

    if (!list_empty(&sem->waiters)) {
        task_t *task = list_entry(sem->waiters.head.prev, task_t, node);
        assert(task->magic == XJOS_MAGIC);
        task_unblock(task, EOK);

        task_yield();
    }

    set_interrupt_state(intr);
}