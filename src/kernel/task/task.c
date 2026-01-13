#include <xjos/task.h>
#include <xjos/sched.h>
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
#include <fs/fs.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

#define NR_TASKS 64

extern u32 volatile jiffies;
extern u32 jiffy;
extern bitmap_t kernel_map;
extern tss_t tss;
extern file_t file_table[];

extern void task_switch(task_t *next);
extern void interrupt_exit();

static task_t *tasks_table[NR_TASKS];
task_t *idle_task;

static list_t block_list;
static list_t sleep_list;

// 时间比较宏
#define time_after(a, b) ((int32)(a) - (int32)(b) > 0)
#define time_before(a, b) ((int32)(a) - (int32)(b) < 0)
#define time_after_eq(a, b) ((int32)(a) - (int32)(b) >= 0)

// ----------------------------------------------------------------------------
// 基础辅助函数
// ----------------------------------------------------------------------------

// 获取当前任务 (利用 PCB 位于 4KB 页底的对齐特性)
task_t *running_task() {
    u32 esp;
    asm volatile("movl %%esp, %0" : "=r"(esp));
    return (task_t *)(esp & 0xfffff000);
}

// 寻找空闲任务槽
static task_t *get_free_task() {
    for (int i = 0; i < NR_TASKS; i++) {
        if (tasks_table[i] == NULL) {
            // 分配 1 页 (4KB) 用于 PCB + 内核栈
            u32 page = alloc_kpage(1);
            if (page == 0) panic("OOM: task creation");
            
            task_t *task = (task_t *)page;
            memset(task, 0, PAGE_SIZE);
            
            task->pid = i;
            tasks_table[i] = task;
            return task;
        }
    }
    panic("No free task slot");
    return NULL;
}

// ----------------------------------------------------------------------------
// 调度状态控制 (Sleep, Wakeup, Block)
// ----------------------------------------------------------------------------

// schedule 在 sched.c 中定义
extern void schedule(); 

void task_sleep(u32 ms) {
    // 睡眠必须在关中断下操作，防止竞态
    assert(!get_interrupt_state());
    
    u32 ticks = ms / jiffy;
    if (ticks == 0) ticks = 1;

    task_t *current = running_task();
    current->wakeup_time = jiffies + ticks;
    current->state = TASK_SLEEPING;

    // 插入有序链表 (按唤醒时间排序)
    bool inserted = false;
    task_t *pos;
    list_for_each_entry(pos, &sleep_list, node) {
        if (time_before(current->wakeup_time, pos->wakeup_time)) {
            list_insert_before(&pos->node, &current->node);
            inserted = true;
            break;
        }
    }
    if (!inserted) list_pushback(&sleep_list, &current->node);

    schedule();
}

// 系统时钟中断会调用此函数检查唤醒
bool task_wakeup() {
    assert(!get_interrupt_state());
    bool woken = false;
    list_node_t *ptr = sleep_list.head.next;
    
    while (ptr != &sleep_list.head) {
        list_node_t *next = ptr->next;
        task_t *task = list_entry(ptr, task_t, node);

        if (time_after_eq(jiffies, task->wakeup_time)) {
            task_unblock(task);
            task->wakeup_time = 0;
            woken = true;
            ptr = next;
        } else {
            // 因为是有序链表，如果当前这个没到时间，后面的肯定也没到
            break;
        }
    }
    return woken;
}

void task_block(task_t *task, list_t *blist, task_state_t state) {
    assert(!get_interrupt_state());
    if (!blist) blist = &block_list;
    list_push(blist, &task->node);
    task->state = state;
    
    if (task == running_task()) schedule();
}

void task_unblock(task_t *task) {
    assert(!get_interrupt_state());
    if (task->node.next) list_remove(&task->node);
    task->state = TASK_READY;

    // CFS 唤醒补偿: 防止睡眠太久的任务获得过多的时间片打击当前任务
    u64 bonus = ((u64)SCHED_WAKEUP_GRAN_MS * NICE_0_WEIGHT) / (task->weight ? task->weight : 1);
    if (task->vruntime > bonus) task->vruntime -= bonus;
    else task->vruntime = 0;

    sched_wakeup_task(task);
}

void task_yield() {
    bool intr = interrupt_disable();
    schedule();
    set_interrupt_state(intr);
}

void task_activate(task_t *task) {
    assert(task->magic == XJOS_MAGIC);
    
    // 切换页目录 (如果是用户进程)
    if (task->pde != get_cr3()) set_cr3(task->pde);
    
    // 如果是用户进程，需要更新 TSS 的 ESP0，以便下次中断能正确切回内核栈
    if (task->uid != KERNEL_USER) tss.esp0 = (u32)task + PAGE_SIZE;
}

// ----------------------------------------------------------------------------
// 核心系统调用: Fork, Exit, Waitpid
// ----------------------------------------------------------------------------

/**
 * sys_fork 的核心实现
 * 注意：必须在用户态(Ring3)下调用 fork 才能正常工作，因为需要 SS/ESP 压栈
 */
pid_t task_fork() {
    assert(!get_interrupt_state());
    task_t *parent = running_task();
    task_t *child = get_free_task();
    pid_t pid = child->pid;

    // 1. 复制 PCB (包含内核栈数据)
    // 此时 child->stack 指向的是 parent 的旧地址，是错误的，下面会修正
    memcpy(child, parent, PAGE_SIZE); 

    // -----------------------------------------------------------------------
    // [重构内核栈]
    // 目标：让子进程被调度时，跳过 task_fork 的函数体，直接从中断出口(interrupt_exit)返回
    // -----------------------------------------------------------------------
    
    // A. 定位父进程栈底的 intr_frame (保存了用户态上下文)
    intr_frame_t *parent_iframe = (intr_frame_t *)((u32)parent + PAGE_SIZE - sizeof(intr_frame_t));
    intr_frame_t *child_iframe  = (intr_frame_t *)((u32)child + PAGE_SIZE - sizeof(intr_frame_t));
    
    // B. 复制用户态现场
    // 这一步确保子进程回到用户态时，EIP/ESP/寄存器都和父进程一模一样
    *child_iframe = *parent_iframe;
    
    // C. [核心差异] 子进程返回值为 0
    child_iframe->eax = 0;

    // D. 构造用于 switch_to 切换的 task_frame
    // 我们把它放在 intr_frame 的下面
    task_frame_t *child_task_frame = (task_frame_t *)((u32)child_iframe - sizeof(task_frame_t));
    
    // E. 初始化 task_frame
    memset(child_task_frame, 0, sizeof(task_frame_t));
    
    // F. 设置调度返回地址
    // 当 schedule() 选中子进程并执行 ret 时，CPU 将跳转到 interrupt_exit
    child_task_frame->eip = interrupt_exit; 
    
    // 调试魔数
    child_task_frame->ebp = 0x44444444; 
    child_task_frame->ebx = 0x55555555;

    // G. 修正子进程的栈指针 (ESP)
    // 此时 child->stack 指向我们刚刚伪造好的 task_frame
    child->stack = (u32 *)child_task_frame;

    // -----------------------------------------------------------------------

    // 2. 修正 PCB 属性
    child->pid = pid;
    child->ppid = parent->pid;
    child->state = TASK_READY;
    child->magic = XJOS_MAGIC;

    // add, fix ref count
    for (int i = 0; i < TASK_FILE_NR; i++) {
        file_t *file = child->files[i];
        if (file) {
            file->count++;  // 增加引用计数
        }
    }

    // dir ref count
    if (child->ipwd) child->ipwd->count++;
    if (child->iroot) child->iroot->count++;
    if (child->iexec) child->iexec->count++;

    // 深拷贝 pwd
    child->pwd = kmalloc(MAX_PATH_LEN);
    strcpy(child->pwd, parent->pwd);


    // 3. 调度初始化
    child->vruntime = sched_get_min_vruntime();
    child->ticks = child->weight; 

    list_init(&child->children);
    list_node_init(&child->sibling);
    list_node_init(&child->node);

    // 红黑树内部在插入前会初始化节点，这里清零以防万一
    memset(&child->cfs_node, 0, sizeof(child->cfs_node));

    // 4. 内存空间深拷贝
    // 必须为子进程分配独立的 vmap 结构，但初始内容继承自父进程
    child->vmap = kmalloc(sizeof(bitmap_t));
    if (parent->vmap) {
        memcpy(child->vmap, parent->vmap, sizeof(bitmap_t));
        // 关键：必须深拷贝位图缓冲区，否则父子进程会争抢同一个物理页分配状态
        if (parent->vmap->bits) {
            void *buf = (void *)alloc_kpage(1);
            memcpy(buf, parent->vmap->bits, PAGE_SIZE);
            child->vmap->bits = buf;
        }
    }
    
    // 拷贝页目录 (COW 或 深拷贝由内存管理模块决定)
    child->pde = (u32)copy_pde();

    // 5. 加入就绪队列
    list_pushback(&parent->children, &child->sibling);
    sched_enqueue_task(child);

    // 父进程返回子进程 PID
    return child->pid; 
}

// 创建内核线程的辅助函数
static task_t *task_create(target_t target, const char *name, int nice, u32 uid) {
    task_t *task = get_free_task();

    strlcpy(task->name, name, TASK_NAME_LEN);
    task->uid = uid;
    task->gid = 0;  
    task->state = TASK_READY;
    task->magic = XJOS_MAGIC; 
    
    // 内核线程共享内核页表和位图
    task->vmap = &kernel_map;
    task->pde = KERNEL_PAGE_DIR;
    
    task->brk = USER_EXEC_ADDR;     // 待分配
    task->text = USER_EXEC_ADDR;
    task->data = USER_EXEC_ADDR;
    task->end  = USER_EXEC_ADDR;
    task->iexec = NULL;
    task->iroot = task->ipwd = get_root_inode();
    task->iroot->count += 2; // 引用计数增加

    task->pwd = kmalloc(MAX_PATH_LEN);
    strcpy(task->pwd, "/");
    
    task->umask = 0022;
    
    task->files[STDIN_FILENO] = &file_table[STDIN_FILENO];
    task->files[STDOUT_FILENO] = &file_table[STDOUT_FILENO];
    task->files[STDERR_FILENO] = &file_table[STDERR_FILENO];
    file_table[STDIN_FILENO].count++;
    file_table[STDOUT_FILENO].count++;
    file_table[STDERR_FILENO].count++;

    task->nice = nice;
    task->weight = sched_nice_to_weight(nice);
    task->vruntime = sched_get_min_vruntime();

    list_init(&task->children);
    list_node_init(&task->sibling);
    list_node_init(&task->node);

    // 构造内核线程的初始栈
    u32 stack = (u32)task + PAGE_SIZE;
    stack -= sizeof(task_frame_t);
    task_frame_t *frame = (task_frame_t *)stack;
    memset(frame, 0, sizeof(task_frame_t));
    
    frame->eip = (void *)target; // 线程入口函数
    frame->ebx = 0x11111111;
    frame->esi = 0x22222222;
    frame->edi = 0x33333333;
    frame->ebp = 0x44444444;
    
    task->stack = (u32 *)frame;

    if (strcmp(name, "idle") != 0) {
        sched_enqueue_task(task);
    }

    return task;
}

extern void sys_close();

void task_exit(int status) {
    task_t *task = running_task();
    task->state = TASK_DIED;
    task->status = status;

    // 1. 释放页表资源
    free_pde();

    // 2. 释放虚拟内存位图 (重要修复: 绝不能释放内核的位图)
    if (task->vmap != &kernel_map) {
        if (task->vmap->bits) {
            free_kpage((u32)task->vmap->bits, 1);
        }
        kfree(task->vmap);
    }

    kfree(task->pwd);
    iput(task->ipwd);
    iput(task->iroot);
    iput(task->iexec);    

    for (size_t i = 0; i < TASK_FILE_NR; i++) {
        file_t *file = task->files[i];
        if (file) {
            sys_close(i);
        }
    }


    // 3. 处理孤儿进程：将当前进程的子进程过继给 init (PID 1)
    task_t *parent = tasks_table[task->ppid];
    if (!parent) parent = tasks_table[1]; // Fallback to Init

    while (!list_empty(&task->children)) {
        list_node_t *node = list_pop(&task->children);
        task_t *child = list_entry(node, task_t, sibling);
        child->ppid = parent->pid;
        list_pushback(&parent->children, &child->sibling);
    }

    // 4. 通知父进程
    // 如果父进程正在等待 (WAITING) 且条件满足，则唤醒它
    if (parent->state == TASK_WAITING && 
       (parent->waitpid == -1 || parent->waitpid == task->pid)) {
        task_unblock(parent);
    }
    
    // 5. 调度 (永不返回)
    schedule();
}

pid_t task_waitpid(pid_t pid, int32 *status) {
    task_t *task = running_task();
    task_t *child = NULL; 

    while (true) {
        bool has_child = false;
        
        list_node_t *node;
        // 遍历所有子进程
        list_for_each(node, &task->children) {
            task_t *ptr = list_entry(node, task_t, sibling);
            
            // 如果指定了 PID 且不匹配，跳过
            if (pid != -1 && ptr->pid != pid) continue;
            
            has_child = true;
            
            // 发现僵尸进程 (ZOMBIE/DIED)
            if (ptr->state == TASK_DIED) {
                child = ptr;
                goto found; 
            }
        }
        
        // 如果有子进程但都在运行，则阻塞当前进程等待
        if (has_child) {
            task->waitpid = pid;
            task_block(task, NULL, TASK_WAITING);
            continue; // 被唤醒后重新扫描
        }
        
        // 没有符合条件的子进程 (ECHILD)
        break;
    }
    return -1;

found:
    if (status) *status = child->status;
    pid_t ret = child->pid;
    
    // 彻底销毁子进程 PCB
    tasks_table[child->pid] = NULL;
    list_remove(&child->sibling);
    free_kpage((u32)child, 1);
    
    return ret;
}

// ----------------------------------------------------------------------------
// 其他辅助
// ----------------------------------------------------------------------------

void task_to_user_mode(target_t target) {
    task_t *task = running_task();
    
    // 重置内核栈顶
    // 注意：这里只是为了清理栈空间，真正切换靠 iret
    
    task->nice = NICE_DEFAULT;
    task->weight = sched_nice_to_weight(task->nice);

    // 为用户进程分配独立的内存位图
    task->vmap = kmalloc(sizeof(bitmap_t));
    void *buf = (void *)alloc_kpage(1);
    bitmap_init(task->vmap, buf, USER_MMAP_SIZE / PAGE_SIZE / 8, USER_MMAP_ADDR / PAGE_SIZE);

    // 复制页表并切换
    task->pde = (u32)copy_pde();
    set_cr3(task->pde);

    // 构造 Ring3 中断帧
    u32 stack_top = (u32)task + PAGE_SIZE;
    stack_top -= sizeof(intr_frame_t);
    intr_frame_t *iframe = (intr_frame_t *)stack_top;
    
    memset(iframe, 0, sizeof(intr_frame_t));
    
    iframe->vector = 0x20; // 这里的 vector 意义不大，主要是为了对齐
    iframe->cs = USER_CODE_SELECTOR | 3;
    iframe->ds = USER_DATA_SELECTOR | 3;
    iframe->es = USER_DATA_SELECTOR | 3;
    iframe->fs = USER_DATA_SELECTOR | 3;
    iframe->gs = USER_DATA_SELECTOR | 3;
    iframe->ss = USER_DATA_SELECTOR | 3;
    
    iframe->eip = (u32)target;
    iframe->esp = USER_STACK_TOP; // 用户栈顶
    iframe->eflags = (0x200 | 0x2); // IF=1 (开中断), IOPL=0

    // 暴力修改 ESP 并跳转到中断退出函数
    // 这样执行 iret 时就会弹出我们构造的 Ring3 上下文
    asm volatile (
        "movl %0, %%esp \n"
        "jmp interrupt_exit \n"
        : : "r"(iframe) : "memory"
    );
}

pid_t sys_getpid() { return running_task()->pid; }
pid_t sys_getppid() { return running_task()->ppid; }

fd_t task_get_fd(task_t *task) {
    fd_t i;
    
    for (i = 0; i < TASK_FILE_NR; i++) {
        if (!task->files[i])
            break;
    }

    if (i == TASK_FILE_NR) {
        panic("Too many open files");
    }

    return i;
}

void task_put_fd(task_t *task, fd_t fd) {
    // if (fd < 3)  // 内核不应该替进程做决定
    //     return;
    
    assert(fd < TASK_FILE_NR);
    task->files[fd] = NULL;
}


extern void idle_thread();
extern void init_thread();
extern void test_thread();
extern void sync_thread();

// 修复启动时的崩溃：初始化 0号任务 (Boot Task)
static void task_setup() {
    // 1. 获取当前(Boot)任务的 PCB 地址
    // Loader 阶段的栈通常设在 0xf000 (或其他页对齐地址)，所以可以直接用 running_task 获取
    task_t *task = running_task();
    
    // 2. 初始化 Magic，防止 schedule 检查 assert 失败
    task->magic = XJOS_MAGIC;
    
    // 3. 标记为活跃，防止被清理
    task->ticks = 1;
    
    // 4. 清空任务表
    memset(tasks_table, 0, sizeof(tasks_table));
}

void task_init() {
    list_init(&block_list);
    list_init(&sleep_list);
    sched_init();

    // 1. 初始化当前所在的 Boot 任务
    task_setup();

    // 2. 创建系统级任务
    idle_task = task_create(idle_thread, "idle", NICE_MAX, KERNEL_USER);
    task_create(init_thread, "init", NICE_DEFAULT, NORMAL_USER);
    task_create(test_thread, "test", NICE_DEFAULT, NORMAL_USER);
    task_create(sync_thread, "sync", NICE_DEFAULT, NORMAL_USER);
}