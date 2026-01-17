#include <xjos/mutex.h>
#include <xjos/interrupt.h>
#include <xjos/assert.h>


void mutex_init(mutex_t *lock) {
    lock->holder = NULL;
    lock->repeat = 0;
    sem_init(&lock->sem);
}


void mutex_lock(mutex_t *lock) {
    bool intr = interrupt_disable();

    // try get lock
    task_t *current = running_task();
    if (lock->holder != current) {
        sem_wait(&lock->sem);
        // get success, set holder
        assert(lock->repeat == 0 && lock->holder == NULL);
        lock->holder = current;
        lock->repeat = 1;
    } else {
        lock->repeat++;       // ref count
    }

    set_interrupt_state(intr);
}


void mutex_unlock(mutex_t *lock) {
    bool intr = interrupt_disable();

    task_t *current = running_task();

    assert(lock->holder == current);

    if (lock->repeat > 1) {
        lock->repeat--;
    } else {
        assert(lock->repeat == 1);
        // only one holder, release lock
        lock->holder = NULL;
        lock->repeat = 0;
        sem_post(&lock->sem);
    }

    set_interrupt_state(intr);
}