#include <xjos/interrupt.h>
#include <xjos/syscall.h>
#include <xjos/debug.h>
#include <xjos/mutex.h>
#include <xjos/spinlock.h>
#include <libc/stdio.h>
#include <xjos/arena.h>
#include <fs/buffer.h>

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

extern int osh_main();

static void user_init_thread() {

    while (true) {
        u32 status;
        pid_t pid = fork();
        if (pid) {
            pid_t child = waitpid(pid, &status);
            printf("wait pid %d status %d %d\n", child, status, time());
        } else {
            osh_main();
        }
    }
}

extern void dev_init();

void init_thread() {
    char temp[100];
    dev_init();
    task_to_user_mode(user_init_thread);    
} 


void test_thread() {
    set_interrupt_state(true);
    while (true) {
        sleep(1000);
    }
}


void sync_thread() {
    set_interrupt_state(true);
    while (true) {
        sync();
        sleep(5000); // every 5 seconds
    }
}