#include <xjos/interrupt.h>
#include <xjos/debug.h>
#include <xjos/task.h>
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

extern int sys_execve(char *filename, char *argv[], char *envp[]);

extern void dev_init();

void init_thread() {
    dev_init();

    // Enter user mode and run /bin/init (a tiny supervisor that respawns /bin/sh).
    task_prepare_user_mode();
    char *argv[] = {"init", NULL};
    char *envp[] = {"HOME=/", "PATH=/bin", NULL};
    sys_execve("/bin/init", argv, envp);
    panic("init: failed to exec /bin/init");
}  


void test_thread() {
    set_interrupt_state(true);
    while (true) {
        bool intr = interrupt_disable();
        task_sleep(1000);
        set_interrupt_state(intr);
    }
}


void sync_thread() {
    set_interrupt_state(true);
    while (true) {
        bool intr = interrupt_disable();
        bsync();
        task_sleep(5000); // every 5 seconds
        set_interrupt_state(intr);
    }
}