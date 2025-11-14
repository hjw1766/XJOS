#ifndef XJOS_TASK_H
#define XJOS_TASK_H

#include <xjos/types.h>
#include <xjos/bitmap.h>
#include <xjos/list.h>
#include <xjos/rbtree.h> // <-- include rbtree header

#define KERNEL_USER 0
#define NORMAL_USER 1

#define TASK_NAME_LEN 16

// default weight (for nice 0)
#define NICE_0_WEIGHT 1024

// (!!!!) NEW: Nice value defines (!!!!)
#define NICE_MIN (-20)
#define NICE_MAX (19)
#define NICE_DEFAULT (0)


typedef void target_t();

typedef enum {
    TASK_INIT,          // Initial state
    TASK_RUNNING,       // executing
    TASK_READY,         // ready to run (in cfs_ready_root)
    TASK_BLOCKED,       // blockage
    TASK_SLEEPING,      // sleeping
    TASK_WAITING,       // waiting for a resource
    TASK_DIED,          // task has died
}task_state_t;

typedef struct task_t {
    u32 *stack;              // kernel stack
    list_t children;         // child tasks list
    list_node_t sibling;     // parent task sibling node
    task_state_t state;      // state   
    char name[TASK_NAME_LEN]; // task name
    u32 uid;                 // user id
    pid_t pid;
    pid_t ppid;
    u32 pde;                 // page directory entry
    bitmap_t *vmap;          // process virtual memory bitmap
    u32 brk;                 // process heap top
    int status;             // exit status
    pid_t waitpid;          // process waitpid result
    u32 magic;               // kernel magic number

    // === List Node ===
    // for sleep_list, block_list, and sync (semaphore) waiters list
    list_node_t node; 	 

    // === CFS Scheduler Fields ===
    int nice; 			    // (!!!!) NEW: Nice value (-20 to +19) (!!!!)
    u64 vruntime; 			// virtual runtime (u64 avoid overflow)
    u32 weight; 			// task weight (from nice)
    u32 sched_slice; 		// calculated total timeslice (ms)
    int ticks; 				// remaining timeslice (clock ticks)
    u32 wakeup_time; 		// wakeup time (for sleep, in jiffies)

    struct rb_node cfs_node; // CFS ready queue rbtree node

}task_t;

typedef struct {
    u32 edi;
    u32 esi;
    u32 ebx;
    u32 ebp;
    void (*eip)(void);
}task_frame_t;

// interrupt frame
typedef struct {
    u32 vector;

    u32 edi;
    u32 esi;
    u32 ebp;
    u32 esp_dummy;      // not used

    u32 ebx;
    u32 edx;
    u32 ecx;
    u32 eax;

    u32 gs;
    u32 fs;
    u32 es;
    u32 ds;

    u32 vector0;
    u32 error;

    u32 eip;
    u32 cs;
    u32 eflags;
    u32 esp;
    u32 ss;
}intr_frame_t;


void task_init();
task_t *running_task();
void schedule();

void task_yield();
void task_block(task_t *task, list_t *blist, task_state_t state);
void task_unblock(task_t *task);

void task_sleep(u32 ms);
bool task_wakeup(); // <-- return type changed to bool

void task_to_user_mode(target_t target);

pid_t task_waitpid(pid_t pid, int32 *status);
pid_t sys_getpid();
pid_t sys_getppid();

pid_t task_fork();
void task_exit(int status);


#endif /* _XJOS_TASK_H_ */