#include <hardware/io.h>
#include <xjos/interrupt.h>
#include <libc/assert.h>
#include <xjos/debug.h>
#include <xjos/task.h>
#include <xjos/xjos.h>
extern void time_init();


#define PIT_CHAN0_REG 0x40
#define PIT_CHAN2_REG 0x42
#define PIT_CTRL_REG  0x43  // 8253 control word


#define HZ 100
#define OSCILLATOR 1193182
#define CLOCK_COUNTER (OSCILLATOR / HZ)
#define JIFFY (1000 / HZ)


#define SPEAKER_REG 0x61
#define BEEP_HZ 440
#define BEEP_COUNTER (OSCILLATOR / BEEP_HZ)



u32 volatile jiffies = 0;
u32 jiffy = JIFFY;

u32 volatile beeping = 0;

// (extern declare from task.c idle_task and cfs_task_count)
extern task_t *idle_task;
extern u32 cfs_task_count;
extern bool task_wakeup(); // (!!!!) NEW


void start_beep() {
    if (!beeping) {
        // set bit 0 and bit 1
        outb(SPEAKER_REG, inb(SPEAKER_REG) | 3);
    }

    beeping = jiffies + 5;  // 50ms, once jiffies 10ms
}


void stop_beep() {
    if (beeping && beeping < jiffies) {
        outb(SPEAKER_REG, inb(SPEAKER_REG) & 0xfc);
        beeping = 0;
    }
}


/**
 * @brief (!!!!) Fixed CFS Clock Interrupt (!!!!)
 */
void clock_handler(int vector) {
    assert(vector == 0x20);
    send_eoi(vector);

    jiffies++;

    // 1. wakeup sleeping tasks
    bool woken_up = task_wakeup(); // (!!!!) NEW
    
    // (Original task_aging() removed)
    
    task_t *task = running_task();    
    assert(task->magic == XJOS_MAGIC);

    // 2. idle task check
    if (task == idle_task) {
        // if idle running, but other tasks ready (woken up or existing)
        if (cfs_task_count > 0) {
            schedule();
        }
        return;
    }
    
    // 3. decrement current task's remaining ticks
    task->ticks--;      // time
    
    // 4. (!!!!) Preemption Check (!!!!)
    // if timeslice zero, OR
    // new task woken up (woken_up == true) AND ready queue not empty
    if (task->ticks <= 0 || (woken_up && cfs_task_count > 0)) {
        schedule();
    }
}

extern time_t startup_time;

time_t sys_time() {
    // (jiffies * JIFFY / 1000) Unix -> nowdays time seconds
    return startup_time + (jiffies * JIFFY) / 1000;
}


void pit_init() {
    // mode 2
    outb(PIT_CTRL_REG, 0b00110100);
    outb(PIT_CHAN0_REG, CLOCK_COUNTER & 0xff);
    outb(PIT_CHAN0_REG, (CLOCK_COUNTER >> 8) & 0xff);

    outb(PIT_CTRL_REG, 0b10110110);
    outb(PIT_CHAN2_REG, (u8)BEEP_COUNTER);
    outb(PIT_CHAN2_REG, (u8)(BEEP_COUNTER >> 8));
}


void clock_init() {
    pit_init();
    set_interrupt_handler(IRQ_CLOCK, clock_handler);
    set_interrupt_mask(IRQ_CLOCK, true);
}