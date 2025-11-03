#include <xjos/interrupt.h>
#include <xjos/syscall.h>
#include <xjos/debug.h>
#include <xjos/mutex.h>
#include <xjos/spinlock.h>
#include <libc/stdio.h>
#include <xjos/arena.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)


void idle_thread() {
    set_interrupt_state(true); // ensure idle thread interrupt enabled
    while (true) {
        // hlt: stop CPU until next interrupt (like clock)
        // so idle loop not 100% CPU
        asm volatile(
            "sti\n" // (Original)
            "hlt\n"
        );
        
        // (remove yield();)
        // clock_handler will handle hlt wakeup
        // check cfs_task_count and call schedule()
    }
}


static void user_init_thread() {
}


void init_thread() {
    // char temp[100];
    // task_to_user_mode(user_init_thread);    

    set_interrupt_state(true);
    while (true) {
        LOGK("init thread\n");
    }
} 


void test_thread() {
    set_interrupt_state(true);
    u32 count = 0;
    while (true) {
        LOGK("test thread\n");
    }
}