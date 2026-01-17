#include <xjos/spinlock.h>
#include <xjos/interrupt.h>
#include <xjos/assert.h>


void spin_init(spinlock_t *lock, const char *name) {
    lock->locked = 0;
    lock->name = name;
    lock->holder_cpu = -1;
    lock->intr_state = true;
}


/*
 * @brief get spinlock
 * 1. turn off interrupt
 * 2. atomic operation get lock
 * 3. wait until lock get success
*/
void spin_lock(spinlock_t *lock) {
    lock->intr_state = interrupt_disable();

    assert(lock->holder_cpu != 0);

    // spin
    while (__sync_lock_test_and_set(&lock->locked, 1) != 0) {
        asm volatile("pause");
    }

    lock->holder_cpu = 0;
}


/*
 * @brief release spinlock
 * 1. safe check lock holder
 * 2. atomic operation release lock
 * 3. turn on interrupt
*/
void spin_unlock(spinlock_t *lock) {
    assert(lock->locked == 1);
    assert(lock->holder_cpu == 0);

    lock->holder_cpu = -1;

    __sync_lock_release(&lock->locked);

    set_interrupt_state(lock->intr_state);
}
