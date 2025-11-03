#ifndef XJOS_SCHED_H
#define XJOS_SCHED_H

#include <xjos/task.h> // Required for task_t definition
#include <xjos/types.h>

// === CFS Scheduler Constants ===
// Sched period: assume jiffy = 10ms, 10 * 10ms = 100ms
#define SCHED_LATENCY_MS (10 * jiffy) 
// Min timeslice: 10ms
#define MIN_TIMESLICE_MS (1 * jiffy) 
// Wakeup bonus: 2ms (for sleeper fairness)
#define SCHED_WAKEUP_GRAN_MS (MIN_TIMESLICE_MS / 5)

// === Public Scheduler API ===

/**
 * @brief Initializes the scheduler subsystem.
 */
void sched_init(void);

/**
 * @brief The main scheduler function. Finds and switches to the next task.
 */
void schedule(void);

/**
 * @brief Enqueues a task that is ready to run (e.g., new, forked, or yielding).
 * This path does *not* apply a wakeup bonus.
 * @param task The task to enqueue.
 */
void sched_enqueue_task(task_t *task);

/**
 * @brief Wakes up a task from sleep or block state and enqueues it.
 * This path *applies* the "sleeper fairness" bonus.
 * @param task The task to wake up and enqueue.
 */
void sched_wakeup_task(task_t *task);

/**
 * @brief Converts a 'nice' value (-20 to +19) to a scheduler weight.
 * @param nice The nice value.
 * @return The corresponding weight.
 */
u32 sched_nice_to_weight(int nice);

/**
 * @brief Gets the current minimum vruntime from the CFS tree.
 * @return The global minimum vruntime.
 */
u64 sched_get_min_vruntime(void);

/**
 * @brief Gets the current count of tasks in the ready queue.
 * @return The number of ready tasks.
 */
u32 sched_get_task_count(void);


// === Extern Global Variables ===
// Shared between scheduler and clock interrupt
extern u32 volatile jiffies;
extern u32 jiffy;

#endif /* _XJOS_SCHED_H_ */