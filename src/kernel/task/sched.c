#include <xjos/sched.h>
#include <xjos/task.h>
#include <xjos/printk.h>
#include <xjos/debug.h>
#include <xjos/memory.h>
#include <libc/assert.h>
#include <xjos/interrupt.h>
#include <xjos/rbtree.h>
#include <libc/string.h>

// === Internal CFS Ready Queue ===
static rb_root_t cfs_ready_root = RB_ROOT; // rbtree root
static u32 cfs_task_count = 0; // ready task count
static u64 cfs_min_vruntime = 0ULL; // min vruntime in tree
static u32 cfs_total_weight = 0; // total weight

// === External Dependencies (from task.c) ===
extern task_t *idle_task;
extern void task_switch(task_t *next);
extern void task_activate(task_t *task);
extern task_t *running_task(void);


// ===================================
//     CFS Weight/Priority Management
// ===================================

// (Cleaned array to avoid "unrecognized token" errors)
static const u32 prio_to_weight[40] = {
    88761, 71755, 56864, 45169, 36357, 29110, 23358, 18788, 15122, 12173,
    9809,  7915,  6387,  5169,  4194,  3355,  2684,  2157,  1737,  1399,
    1024,  820,   655,   524,   420,   335,   268,   215,   172,   137,
    110,   87,    70,    56,    45,    36,    29,    23,    18,    15
};

/**
 * @brief (Public API) nice to weight conversion
 */
u32 sched_nice_to_weight(int nice) {
    if (nice < NICE_MIN) nice = NICE_MIN;
    if (nice > NICE_MAX) nice = NICE_MAX;
    return prio_to_weight[nice - NICE_MIN]; 
}

/**
 * @brief (Internal) Calculate and set task timeslice
 */
static void set_timeslice(task_t *task, u32 total_weight) {
    u32 slice_ms = MIN_TIMESLICE_MS; // default min

    if (total_weight > 0) {
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


// ===================================
//     CFS Core Rbtree Operations
// ===================================

/**
 * @brief (Internal) Raw insert task into CFS ready rbtree.
 * Normalizes vruntime against cfs_min_vruntime.
 */
static void __sched_enqueue_rbt(task_t *task) {
    // vruntime check, update to min if smaller
    if (task->vruntime < cfs_min_vruntime) {
        task->vruntime = cfs_min_vruntime;
    }

    struct rb_node **link = &cfs_ready_root.rb_node;
    struct rb_node *parent = NULL;
    task_t *entry;

    // 1. find insert position
    while (*link) {
        parent = *link;
        entry = rb_entry(parent, task_t, cfs_node);
        
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
 * @brief (Internal) Remove task from CFS ready rbtree
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
 * @brief (Internal) Pick next best task (tree leftmost node)
 */
static task_t* cfs_pick_next() {
    struct rb_node *leftmost = rb_first(&cfs_ready_root);
    if (!leftmost) {
        return NULL; // ready queue empty
    }

    task_t* task = rb_entry(leftmost, task_t, cfs_node);
    
    // KEY: update global min vruntime
    cfs_min_vruntime = task->vruntime;
    
    return task;
}


// ===================================
//     Public Scheduler API Implementation
// ===================================

/**
 * @brief (Public API) Initializes the scheduler subsystem.
 */
void _inline sched_init() {
    cfs_ready_root = RB_ROOT;
    cfs_task_count = 0;
    cfs_min_vruntime = 0ULL;
    cfs_total_weight = 0; 
}

/**
 * @brief (Public API) Enqueues a new/yielded task. (No bonus)
 */
void _inline sched_enqueue_task(task_t *task) {
    __sched_enqueue_rbt(task);
}

/**
 * @brief (Public API) Wakes up a task, applies bonus, and enqueues.
 */
void sched_wakeup_task(task_t *task) {
    // Apply "sleeper fairness" bonus
    u32 gran_ms = SCHED_WAKEUP_GRAN_MS;
    u64 bonus = ((u64)gran_ms * NICE_0_WEIGHT) / task->weight;
    if (task->vruntime > bonus) {
        task->vruntime -= bonus;
    } else {
        task->vruntime = 0;
    }

    // Enqueue after bonus adjustment
    __sched_enqueue_rbt(task);
}

/**
 * @brief (Public API) Gets the current minimum vruntime.
 */
u64 _inline sched_get_min_vruntime(void) {
    return cfs_min_vruntime;
}

/**
 * @brief (Public API) Gets the current ready task count.
 */
u32 _inline sched_get_task_count(void) {
    return cfs_task_count;
}

/**
 * @brief (Public API) Core Scheduler
 */
void schedule() {
    assert(!get_interrupt_state());

    task_t *current = running_task();
    task_t *next = NULL;

    // --- 1. Update current task vruntime (if not idle) ---
    if (current != idle_task) {
        u32 total_ticks = current->sched_slice / jiffy;
        if (total_ticks == 0) total_ticks = 1;

        u32 ran_ticks = total_ticks - current->ticks;
        u32 delta_exec_ms = ran_ticks * jiffy;

        if (delta_exec_ms > 0) {
            if (current->weight == 0) {
                // Safeguard against weight 0
                current->weight = NICE_0_WEIGHT;
            }

            // calc vruntime delta
            u64 vruntime_delta = ((u64)delta_exec_ms * NICE_0_WEIGHT) / current->weight;
            current->vruntime += vruntime_delta;
        }
    }

    // --- 2. Put current task back (if RUNNING) ---
    if (current->state == TASK_RUNNING && current != idle_task) {
        current->state = TASK_READY;
        // Use the internal enqueue (no "wakeup" bonus for yielding)
        __sched_enqueue_rbt(current);
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
    task_activate(next);
    task_switch(next);
}