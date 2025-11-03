#include <xjos/task.h>
#include <xjos/printk.h>
#include <xjos/debug.h>
#include <xjos/memory.h>
#include <libc/assert.h>
#include <xjos/interrupt.h>
#include <libc/string.h>
#include <xjos/bitmap.h>
#include <xjos/syscall.h>
#include <xjos/list.h>
#include <xjos/global.h>
#include <xjos/arena.h>
#include <xjos/rbtree.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

#define NR_TASKS (64)

// === CFS Scheduler Constants ===
// Sched period: assume jiffy = 10ms, 10 * 10ms = 100ms
#define SCHED_LATENCY_MS (10 * jiffy) 
// Min timeslice: 10ms
#define MIN_TIMESLICE_MS (1 * jiffy) 
// Wakeup bonus: 2ms (for sleeper fairness, improve interactive response, reduce jitter)
#define SCHED_WAKEUP_GRAN_MS (MIN_TIMESLICE_MS / 5)

extern u32 volatile jiffies;
extern u32 jiffy;
extern bitmap_t kernel_map;
extern void task_switch(task_t *next);
extern tss_t tss;

static task_t *tasks_table[NR_TASKS];   // task table
task_t *idle_task;               // idle (removed static)

static list_t block_list;               // blocked task list
static list_t sleep_list;               // sleep list

// === CFS Ready Queue ===
static rb_root_t cfs_ready_root = RB_ROOT; // rbtree root
u32 cfs_task_count = 0; // ready task count
static u64 cfs_min_vruntime = 0ULL; // min vruntime in tree
static u32 cfs_total_weight = 0; // total weight

// ===================================
//     CFS Helper Functions
// ===================================

/**
 * @brief (!!!!) NEW: nice to weight map (!!!!)
 * (Approx Linux 2.6.23, NICE_0_WEIGHT = 1024 as base)
 * Index 0 maps to nice -20
 * Index 20 maps to nice 0
 * Index 39 maps to nice +19
 */
static const u32 prio_to_weight[40] = {
 /* -20 */ 88761, 71755, 56864, 45169, 36357, 29110, 23358, 18788, 15122, 12173,
 /* -10 */  9809,  7915,  6387,  5169,  4194,  3355,  2684,  2157,  1737,  1399,
 /* 0 */   1024,   820,   655,   524,   420,   335,   268,   215,   172,   137,
 /* +10 */   110,    87,    70,    56,    45,    36,    29,    23,    18,    15
};

/**
 * @brief (!!!!) NEW: nice to weight conversion (!!!!)
 */
static u32 nice_to_weight(int nice) {
    if (nice < NICE_MIN) nice = NICE_MIN;
    if (nice > NICE_MAX) nice = NICE_MAX;
    
    // nice -20 map to index 0
    // nice 0   map to index 20
    // nice +19 map to index 39
    return prio_to_weight[nice - NICE_MIN]; 
}


/**
 * @brief (!!!!) NEW: Calculate and set task timeslice (based on weight) (!!!!)
 */
static void set_timeslice(task_t *task, u32 total_weight) {
    u32 slice_ms = MIN_TIMESLICE_MS; // default min

    if (total_weight > 0) {
        // (task->weight / cfs_total_weight) * SCHED_LATENCY_MS
        u64 temp = ((u64)task->weight * SCHED_LATENCY_MS) / total_weight;
        slice_ms = (u32)temp;
    }

    if (slice_ms < MIN_TIMESLICE_MS) { // ensure min
        slice_ms = MIN_TIMESLICE_MS;
    }

    task->sched_slice = slice_ms; // save total slice (ms)
    task->ticks = slice_ms / jiffy; // convert to clock ticks
    if (task->ticks == 0) { // at least 1 tick
        task->ticks = 1;
    }
}

/**
 * @brief (!!!!) NEW: Insert task into CFS ready rbtree (!!!!)
 */
static void cfs_enqueue(task_t *task) {
    struct rb_node **link = &cfs_ready_root.rb_node;
    struct rb_node *parent = NULL;
    task_t *entry;

    // vruntime check, update to min if smaller
    if (task->vruntime < cfs_min_vruntime) {
        task->vruntime = cfs_min_vruntime;
    }

    // 1. find insert position
    while (*link) {
        parent = *link;
        entry = rb_entry(parent, task_t, cfs_node);
        
        // key is vruntime
        if (task->vruntime < entry->vruntime) {
            link = &(*link)->rb_left;
        } else {
            // equal vruntime, insert right
            link = &(*link)->rb_right;
        }
    }

    // 2. link new node
    rb_set_parent(&task->cfs_node, parent);
    task->cfs_node.rb_left = NULL;
    task->cfs_node.rb_right = NULL;
    rb_set_red(&task->cfs_node); // new node is red
    
    *link = &task->cfs_node;

    // 3. fix rbtree balance
    rb_insert_color(&task->cfs_node, &cfs_ready_root);
    
    // 4. update counters
    cfs_task_count++;
    cfs_total_weight += task->weight; 
}

/**
 * @brief (!!!!) NEW: Remove task from CFS ready rbtree (!!!!)
 */
static task_t* cfs_dequeue(task_t *task) {
    if (cfs_task_count > 0) {
        rb_erase(&task->cfs_node, &cfs_ready_root); // remove from tree
        cfs_task_count--;
        cfs_total_weight -= task->weight; 
    }
    // clear node pointers
    task->cfs_node.rb_parent_color = 0;
    task->cfs_node.rb_left = NULL;
    task->cfs_node.rb_right = NULL;
    return task;
}

/**
 * @brief (!!!!) NEW: Pick next best task (tree leftmost node) (!!!!)
 */
static task_t* cfs_pick_next() {
    struct rb_node *leftmost = rb_first(&cfs_ready_root);
    if (!leftmost) {
        return NULL; // ready queue empty
    }

    task_t* task = rb_entry(leftmost, task_t, cfs_node);
    
    // (!!!!) KEY: update global min vruntime (!!!!)
    cfs_min_vruntime = task->vruntime;
    
    return task;
}

// ===================================
//     Core Task API Modify
// ===================================


void task_sleep(u32 ms) {
    // 1. disable interrupt, save state
    // bool intr = interrupt_disable();
    assert(!get_interrupt_state()); // (Original check)

    u32 ticks = ms / jiffy;             // jiffy 10ms
    ticks = ticks > 0 ? ticks : 1;      // at least 1 jiffy

    task_t *current = running_task();
    current->wakeup_time = jiffies + ticks;   // set wakeup time
    
    // check if already in list (should be NULL)
    assert(current->node.next == NULL);
    assert(current->node.prev == NULL);

    // 2. (atomic) insert task to sleep_list (sorted)
    bool inserted = false;
    task_t *task_cursor = NULL; // (Original: task_curosr)


    list_for_each_entry(task_cursor, &sleep_list, node) {
        // (!!!!) FIX: use wakeup_time
        if (task_cursor->wakeup_time > current->wakeup_time) {
            list_insert_before(&task_cursor->node, &current->node);
            inserted = true;
            break;
        }
    }

    if (!inserted)
        list_pushback(&sleep_list, &current->node);
    
    // 3. (atomic) change state and schedule
    current->state = TASK_SLEEPING;
    
    // 4. (!!!!) After wakeup: schedule() returns here
    schedule(); 

    // restore interrupt state
    // set_interrupt_state(intr);
}


bool task_wakeup() { // (!!!!) FIX: return bool
    assert(!get_interrupt_state());
    
    list_node_t *ptr = sleep_list.head.next;
    list_node_t *next;
    bool woken_up = false; // <-- 1. add flag

    while (ptr != &sleep_list.head) {
        next = ptr->next;

        task_t *task = element_entry(task_t, node, ptr);
        // (!!!!) FIX: use wakeup_time
        if (task->wakeup_time > jiffies)
            break;

        // (!!!!) FIX: use wakeup_time
        task->wakeup_time = 0;
        task_unblock(task);
        woken_up = true; // <-- 2. set flag

        ptr = next;
    }
    
    return woken_up; // <-- 3. return flag
}


static task_t *get_free_task() {
    for (int i = 0; i < NR_TASKS; i++) {
        if (tasks_table[i] == NULL) {
            task_t *task = (task_t *)alloc_kpage(1);        // onec page for task_t
            memset(task, 0, PAGE_SIZE);
            task->pid = i;
            tasks_table[i] = task;
            return task;
        }
    }

    panic("No free task");
}


pid_t sys_getpid() {
    return running_task()->pid;
}


pid_t sys_getppid() {
    return running_task()->ppid;
}

extern void interrupt_exit();

static void task_build_stack(task_t *task) {
    u32 addr = (u32)task + PAGE_SIZE;
    addr -= sizeof(intr_frame_t);

    intr_frame_t *iframe = (intr_frame_t *)addr;
    iframe->eax = 0;

    addr -= sizeof(task_frame_t);
    task_frame_t *frame = (task_frame_t *)addr;

    frame->ebp = 0x44444444; // ebp
    frame->ebx = 0x11111111; // ebx
    frame->edi = 0x33333333; // edi
    frame->esi = 0x22222222; // esi

    frame->eip = interrupt_exit; // eip
    // * schedule -> eip(interrupt_exit) -> eax = 0(fork)

    task->stack = (u32 *)frame;
}


pid_t task_waitpid(pid_t pid, int32 *status) {
    task_t *task = running_task();
    task_t *child = NULL;

    while (true) {
        bool has_child = false;
        list_node_t *node;
        for (node = task->children.head.next; node != &task->children.head; node = node->next) {
            task_t *ptr = list_entry(node, task_t, sibling);

            // find child( pid == -1 or pid == ptr->pid)
            if (pid != ptr->pid && pid != -1) {
                continue;
            }

            // match child
            if (ptr->state == TASK_DIED) {
                child = ptr;
                goto rollback;
            }

            // child liveing
            has_child = true;
        }

        if (has_child) {
            // pid liveing or -1
            task->waitpid = pid;
            // wait stoppage
            task_block(task, NULL, TASK_WAITING);
            
            // renew execute while span
            continue;
        }
        
        break;
    }
    // parent process has no matching child process
    return -1;

// child died, rollback
rollback:
    // copy Zombie Process status to parent process
    *status = child->status;
    int32 ret = child->pid;
    
    // clear child process, and remove from children list
    tasks_table[child->pid] = NULL;
    list_remove(&child->sibling);

    free_kpage((u32)child, 1);
    return ret;
}


void task_exit(int status) {
    task_t *task = running_task();

    // (!!!!) FIX: use node
    assert(task->node.next == NULL && task->node.prev == NULL && task->state == TASK_RUNNING);

    task->state = TASK_DIED;
    task->status = status;

    // FIFO-like release order
    free_pde();

    free_kpage((u32)task->vmap->bits, 1);
    kfree(task->vmap);

    task_t *parent = tasks_table[task->ppid];
    list_node_t *node;

    // Traversal task child list
    while (!list_empty(&task->children)) {
        node = list_pop(&task->children);
        task_t *child = list_entry(node, task_t, sibling);
        
        child->ppid = parent->pid;

        list_pushback(&parent->children, &child->sibling);
    }

    LOGK("task 0x%p exit...\n", task);
    
    // wakeup parent process
    if (parent->state == TASK_WAITING &&
         (parent->waitpid == -1 || parent->waitpid == task->pid)) {
        task_unblock(parent);
    }

    schedule();
}


pid_t task_fork() {
    task_t *task = running_task();

    // no stoppage, excute current task
    assert(task->state == TASK_RUNNING);

    // copy kernel stack and PCB
    task_t *child = get_free_task();
    pid_t pid = child->pid; // store pid before memcpy
    memcpy(child, task, PAGE_SIZE);

    child->pid = pid;
    child->ppid = task->pid;
    child->state = TASK_READY;

    // === CFS fields inherited by memcpy ===
    // child->nice = task->nice; 
    // child->weight = task->weight;
    // child->vruntime = task->vruntime; 
    // (vruntime same as parent, is fair)

    // === Fix lists/nodes ===
    // fix child list, becasue memcpy copy parent list
    list_init(&child->children);
    list_node_init(&child->sibling);
    // (!!!!) FIX: use node (for sleep/block)
    list_node_init(&child->node); 
    // cfs_node no init needed (will be set in enqueue)

    // add the child to the "children" linked list of the parent process("task")
    list_pushback(&task->children, &child->sibling);

    // copy user process vmap
    child->vmap = kmalloc(sizeof(bitmap_t));
    memcpy(child->vmap, task->vmap, sizeof(bitmap_t));

    // copy vmap cache
    void *buf = (void *)alloc_kpage(1);
    memcpy(buf, task->vmap->bits, PAGE_SIZE);
    child->vmap->bits = buf;
    
    // copy page directory
    child->pde = (u32)copy_pde();

    // build child kernel stack
    task_build_stack(child);    // * ROP

    // === Add to CFS ready queue ===
    cfs_enqueue(child);

    return child->pid;
}


void task_yield() {
    // (!!!!) FIX: yield must be atomic (!!!!)
    bool intr = interrupt_disable();
    // schedule() will auto-return current RUNNING task
    // to cfs_ready_root (rbtree)
    schedule();
    // after wakeup, restore interrupt
    set_interrupt_state(intr);
}


// task stoppage
void task_block(task_t *task, list_t *blist, task_state_t state) {
    assert(!get_interrupt_state());
    // (!!!!) FIX: use node
    assert(task->node.next == NULL && task->node.prev == NULL);
    assert(state != TASK_READY && state != TASK_RUNNING);

    if (blist == NULL)
        blist = &block_list;

    // (!!!!) FIX: use node
    list_push(blist, &task->node);
    
    task->state = state;

    task_t *current = running_task();
    if (current == task)
        schedule();
}


void task_unblock(task_t *task) {
    assert(!get_interrupt_state());

    // (!!!!) FIX: use node
    list_remove(&task->node);

    // (!!!!) FIX: use node
    assert(task->node.next == NULL && task->node.prev == NULL);

    task->state = TASK_READY;

    // === OPTIMIZE: give wakeup task vruntime bonus (sleeper fairness) ===
    // subtract weighted gran, improve interactive response, reduce jitter
    u32 gran_ms = SCHED_WAKEUP_GRAN_MS;
    // (!!!!) FIX: use NICE_0_WEIGHT as base
    u64 bonus = ((u64)gran_ms * NICE_0_WEIGHT) / task->weight;
    if (task->vruntime > bonus) {
        task->vruntime -= bonus;
    } else {
        task->vruntime = 0;
    }

    // === Add to CFS ready queue ===
    cfs_enqueue(task);
}


void task_activate(task_t *task) {
    assert(task->magic == XJOS_MAGIC);

    // Process switching pde
    if (task->pde != get_cr3())
        set_cr3(task->pde);

    if (task->uid != KERNEL_USER)
        tss.esp0 = (u32)task + PAGE_SIZE;
}


task_t *running_task() {
    asm volatile(
        "movl %esp, %eax\n"
        "andl $0xfffff000, %eax\n");    // clear page offset, return 0x1000 or 0x2000
}


/**
 * @brief (!!!!) Core Scheduler Refactor (!!!!)
 * * Based on CFS (rbtree) scheduler
 */
void schedule() {
    assert(!get_interrupt_state());

    task_t *current = running_task();
    task_t *next = NULL;

    // --- 1. Update current task vruntime (if not idle) ---
    if (current != idle_task) {
        // calc real runtime (ms)
        u32 total_ticks = current->sched_slice / jiffy;
        if (total_ticks == 0) total_ticks = 1;

        u32 ran_ticks = total_ticks - current->ticks;
        u32 delta_exec_ms = ran_ticks * jiffy;

        if (delta_exec_ms > 0) {
            // (!!!!) FIX: use NICE_0_WEIGHT as base
            if (current->weight == 0) {
                // (if weight is 0, is bug, should avoid at create)
                // (but as safeguard, reset to default)
                current->weight = NICE_0_WEIGHT;
            }

            // calc vruntime delta
            u64 vruntime_delta = ((u64)delta_exec_ms * NICE_0_WEIGHT) / current->weight;
            current->vruntime += vruntime_delta;
        }
    }

    // --- 2. Put current task back (if RUNNING) ---
    if (current->state == TASK_RUNNING && current != idle_task) { // (Original had bug, checked idle here)
        current->state = TASK_READY;
        cfs_enqueue(current);
    }

    // --- 3. Pick next task (min vruntime) ---
    task_t *candidate = cfs_pick_next();

    u32 current_total_weight = cfs_total_weight;

    if (candidate) {
        // --- 4. Remove from ready queue ---
        next = cfs_dequeue(candidate);
    } else {
        next = idle_task;
    }

    // --- 5. Set new task timeslice ---
    if (next != idle_task) {
        set_timeslice(next, current_total_weight);
    }

    // --- 6. Switch to next task ---
    assert(next != NULL);
    next->state = TASK_RUNNING;
    // next->ticks = next->sched_slice / jiffy; // (Already set in set_timeslice)

    task_activate(next);
    task_switch(next);
}


/**
 * @brief (!!!!) MODIFIED: task_create now accepts nice value (!!!!)
 * @param nice range -20 (high) to +19 (low)
 */
static task_t *task_create(target_t target, const char *name, int nice, u32 uid) {
    task_t *task = get_free_task();

    u32 stack = (u32)task + PAGE_SIZE;

    // // * exp. 0x2000 - 0x14 = 0x1fec
    stack -= sizeof(task_frame_t);
    task_frame_t *frame = (task_frame_t *)stack;
    frame->ebx = 0x11111111; // 0x1fec
    frame->esi = 0x22222222; // 0x1ff0
    frame->edi = 0x33333333; // 0x1ff4
    frame->ebp = 0x44444444; // 0x1ff8
    // * pop eip, so eip point -> target
    frame->eip = (void *)target;    // 0x1ffc

    assert(strlen(name) < 16);
    strcpy((char*)task->name, name);

    task->stack = (u32 *)stack;         // esp pointer
    task->state = TASK_READY;
    task->uid = uid;
    task->vmap = &kernel_map;
    task->pde = KERNEL_PAGE_DIR;
    task->brk = KERNEL_MEMORY_SIZE;
    task->magic = XJOS_MAGIC;       // canary
    
    // === (!!!!) Init CFS fields (based on nice) (!!!!) ===
    task->nice = nice;
    task->weight = nice_to_weight(nice); // use helper
    assert(task->weight > 0); // weight must be > 0
    task->vruntime = cfs_min_vruntime; // start at min
    
    // === Init lists ===
    // parent child list
    list_init(&task->children);
    list_node_init(&task->sibling);
    // (!!!!) FIX: use node (for sleep/block)
    list_node_init(&task->node);


    if (strcmp(task->name, "idle") != 0) {
        // === Add to CFS queue ===
        cfs_enqueue(task);
    }

    task->wakeup_time = 0; // init wakeup time

    return task;
}


void task_to_user_mode(target_t target) {
    task_t *task = running_task();

    // (!!!!) ADD: user mode task start with default prio
    task->nice = NICE_DEFAULT;
    task->weight = nice_to_weight(task->nice);

    task->vmap = kmalloc(sizeof(bitmap_t));
    void *buf = (void *)alloc_kpage(1);
    // 8M / 0x1000 = 0x800
    bitmap_init(task->vmap, buf, PAGE_SIZE, KERNEL_MEMORY_SIZE / PAGE_SIZE);

    // user process page directory
    task->pde = (u32)copy_pde();
    set_cr3(task->pde);

    u32 addr = (u32)task + PAGE_SIZE;
    addr -= sizeof(intr_frame_t);
    intr_frame_t *iframe = (intr_frame_t *)addr;

    iframe->vector = 0x20;
    iframe->edi = 1;
    iframe->esi = 2;
    iframe->ebp = 3;
    iframe->esp_dummy = 4;
    iframe->ebx = 5;
    iframe->edx = 6;
    iframe->ecx = 7;
    iframe->eax = 8;

    iframe->gs = 0;
    iframe->ds = USER_DATA_SELECTOR;
    iframe->es = USER_DATA_SELECTOR;
    iframe->fs = USER_DATA_SELECTOR;
    iframe->ss = USER_DATA_SELECTOR;
    iframe->cs = USER_CODE_SELECTOR;

    iframe->error = XJOS_MAGIC;


    iframe->eip = (u32)target;
    iframe->eflags = (0 << 12 | 0b10 | 1 << 9);
    iframe->esp = USER_STACK_TOP;

    interrupt_disable();    // cli <-> iret
    // esp -> iframe
    asm volatile (
        "movl %0, %%esp\n"
        "jmp interrupt_exit\n" ::"m"(iframe)
    );
}


static void task_setup() {
    task_t *task = running_task();
    task->magic = XJOS_MAGIC;
    task->ticks = 1;

    memset(tasks_table, 0, sizeof(tasks_table));
}


extern void idle_thread();
extern void init_thread();
extern void test_thread();

/**
 * @brief (!!!!) MODIFIED: task_init uses nice values (!!!!)
 */
void task_init() {
    list_init(&block_list);
    list_init(&sleep_list);
    
    // === Init CFS ===
    cfs_ready_root = RB_ROOT;
    cfs_task_count = 0;
    cfs_min_vruntime = 0ULL;
    cfs_total_weight = 0; 
    
    // (Original: init ready_queues removed)

    task_setup();

    // (Original: bitmap_init removed)

    // 1. idle thread (lowest prio)
    // nice +19 (weight 15)
    idle_task = task_create(idle_thread, "idle", NICE_MAX, KERNEL_USER); // NICE_MAX = +19

    // 2. observer thread (init)
    // medium weight, most time sleep
    // nice 0 (weight 1024)
    task_create(init_thread, "init", NICE_MIN, NORMAL_USER); // NICE_DEFAULT = 0

    task_create(test_thread, "test", NICE_MIN + 1, KERNEL_USER);
}