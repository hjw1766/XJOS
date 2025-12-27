#ifndef XJOS_TASK_H
#define XJOS_TASK_H

#include <xjos/types.h>
#include <xjos/bitmap.h>
#include <xjos/list.h>
#include <xjos/rbtree.h>

#define KERNEL_USER 0
#define NORMAL_USER 1000

#define TASK_NAME_LEN 16
#define TASK_FILE_NR 16

// 任务内核栈魔数，用于检测栈溢出
#define XJOS_MAGIC 0x20250901

// --------------------------------------------------------
// 调度器常量 (CFS 相关)
// --------------------------------------------------------
#define NICE_0_WEIGHT 1024
#define NICE_MIN (-20)
#define NICE_MAX (19)
#define NICE_DEFAULT (0)

// --------------------------------------------------------
// 任务状态定义
// --------------------------------------------------------
typedef enum {
    TASK_INIT,          // 初始化中
    TASK_RUNNING,       // 正在CPU上运行
    TASK_READY,         // 就绪 (在 CFS 红黑树中)
    TASK_BLOCKED,       // 阻塞 (等待 IO 或锁)
    TASK_SLEEPING,      // 睡眠 (等待时间到达)
    TASK_WAITING,       // 等待子进程退出 (waitpid)
    TASK_DIED,          // 僵尸状态
} task_state_t;

/* +---------------------+ <--- Page End (High Address, e.g., 0x2000)
|                     |
|   Kernel Stack      |
|         |           |
|         v           |
|                     |
|---------------------| <--- 碰撞边界 (Collision Boundary)
|   magic (Canary)    | <--- 应该在这里！
|---------------------|
|   ...               |
|   other members     |
|   ...               |
|---------------------|
|   stack (esp ptr)   | <--- Offset 0
+---------------------+ <--- Page Start (Low Address, e.g., 0x1000) */

typedef void target_t(); // 任务入口函数类型

// --------------------------------------------------------
// 任务控制块 (PCB)
// 注意：task_t 位于内核栈所在页的低地址，栈从高地址向下增长。
// --------------------------------------------------------
typedef struct task_t {
    // === 1. 核心上下文 (Offset 0) ===
    // 必须在最前面，因为汇编(task_switch)中通常假定它是偏移0
    u32 *stack;              // 内核栈顶指针 (esp)
    
    // === 2. 标识符与状态 ===
    pid_t pid;               // 进程 ID
    pid_t ppid;              // 父进程 ID
    task_state_t state;      // 当前状态
    u32 uid;                 // 用户 ID
    u32 gid;                 // 组 ID
    char name[TASK_NAME_LEN];// 任务名称
    int32 status;            // 退出状态码 (exit code)
    pid_t waitpid;           // 正在等待的子进程 PID
    char *pwd;              // 当前工作目录路径字符串

    // === 3. 内存管理 ===
    u32 pde;                 // 页目录表物理地址 (CR3)
    bitmap_t *vmap;          // 虚拟内存位图 (用户态内存分配)
    u32 brk;                 // 用户堆顶 (Heap Top)

    // === 4. 文件系统 ===
    struct inode_t *ipwd;    // 当前工作目录
    struct inode_t *iroot;   // 根目录
    u16 umask;               // 创建文件权限掩码
    struct file_t *files[TASK_FILE_NR]; // 进程文件表

    // === 5. 调度器 (CFS) ===
    int nice;                // 静态优先级 (-20 ~ 19)
    u32 weight;              // 权重 (由 nice 导出)
    u64 vruntime;            // 虚拟运行时间 (调度核心依据)
    u32 sched_slice;         // 物理时间片 (ms)
    int ticks;               // 剩余时间片 (tick)
    u32 wakeup_time;         // 睡眠唤醒时间 (jiffies)
    struct rb_node cfs_node; // 红黑树节点 (连接到 cfs_ready_root)

    // === 6. 链表关系 ===
    list_node_t node;        // 通用链表节点 (用于 sleep_list, block_list 等)
    list_t children;         // 子进程链表头
    list_node_t sibling;     // 兄弟进程节点 (连接到父进程的 children 链表)
    
    // === 7. 边界哨兵 (必须在最后) ===
    // 如果内核栈溢出，栈是从高地址向低地址增长的，最先覆盖的就是这个字段。
    u32 magic;               

} task_t;

// --------------------------------------------------------
// 栈帧定义
// --------------------------------------------------------

typedef struct {
    u32 edi;
    u32 esi;
    u32 ebx;
    u32 ebp;
    void (*eip)(void);
} task_frame_t;


// 中断栈帧 (Interrupt Stack)
// 对应中断进入时的压栈顺序
typedef struct {
    u32 vector;         // 中断向量号
    u32 edi;
    u32 esi;
    u32 ebp;
    u32 esp_dummy;      // pusha 压入的 esp (被忽略)
    u32 ebx;
    u32 edx;
    u32 ecx;
    u32 eax;
    u32 gs;
    u32 fs;
    u32 es;
    u32 ds;
    u32 vector0;        // 某些中断/异常可能会有的额外错误码
    u32 error;          // 错误码
    u32 eip;
    u32 cs;
    u32 eflags;
    u32 esp;            // 特权级切换时压入的 esp
    u32 ss;             // 特权级切换时压入的 ss
} intr_frame_t;


// --------------------------------------------------------
// 函数声明
// --------------------------------------------------------

void task_init();
task_t *running_task();

void schedule();
void task_yield();
void task_sleep(u32 ms);
bool task_wakeup();

void task_block(task_t *task, list_t *blist, task_state_t state);
void task_unblock(task_t *task);
void task_activate(task_t *task);

pid_t task_fork();
void task_exit(int status);
pid_t task_waitpid(pid_t pid, int32 *status);

pid_t sys_getpid();
pid_t sys_getppid();
void task_to_user_mode(target_t target);

fd_t task_get_fd(task_t *task);
void task_put_fd(task_t *task, fd_t fd);

#endif /* XJOS_TASK_H */