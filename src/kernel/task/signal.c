#include <xjos/signal.h>
#include <xjos/task.h>
#include <xjos/memory.h>
#include <xjos/assert.h>
#include <xjos/debug.h>
#include <xjos/errno.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)


typedef struct signal_frame_t {
    u32 restorer;   // 恢复函数
    u32 sig;        // 信号
    u32 blocked;    // 阻塞的信号集

    // 调用前保存的寄存器状态
    u32 eax;
    u32 ecx;
    u32 edx;
    u32 eflags;
    u32 eip;
} signal_frame_t;

// 获取当前进程被阻塞的信号位图
int sys_sgetmask() {
    task_t *task = running_task();
    return task->blocked;
}

// 设置当前进程被阻塞的信号位图，返回之前的值
int sys_ssetmask(int newmask) {
    if (newmask == EOF)
        return -EPERM;

    task_t *task = running_task();
    int old = task->blocked;
    task->blocked = newmask & ~SIGMASK(SIGKILL);
    return old;
}

// 注册信号处理函数
int sys_signal(int sig, int handler, int restorer) {
    if (sig < MINSIG || sig > MAXSIG || sig == SIGKILL)
        return EOF;    

    task_t *task = running_task();
    sigaction_t *ptr = &task->actions[sig - 1];
    ptr->mask = 0; // 默认不阻塞其他信号
    ptr->handler = (void (*)(int))handler;
    ptr->flags = SIG_ONESHOT | SIG_NOMASK;
    ptr->restorer = (void (*)())restorer;

    return handler;
}

// 注册信号处理函数 (更通用的接口，支持更多选项)
int sys_sigaction(int sig, sigaction_t *action, sigaction_t *oldaction) {
    if (sig < MINSIG || sig > MAXSIG || sig == SIGKILL)
        return EOF;
    
    task_t *task = running_task();
    sigaction_t *ptr = &task->actions[sig - 1];
    if (oldaction)
        *oldaction = *ptr; // 返回旧的处理函数信息

    *ptr = *action; // 设置新的处理函数信息
    if (ptr->flags & SIG_NOMASK)
        ptr->mask = 0; // 如果设置了 SIG_NOMASK，确保不阻塞任何信号
    else
        ptr->mask |= SIGMASK(sig); // 默认阻塞当前信号
    return 0;
}

int sys_kill(pid_t pid, int sig) {
    if (sig < MINSIG || sig > MAXSIG)
        return EOF;

    task_t *task = get_task(pid);
    if (!task)
        return EOF;
    if (task->uid == KERNEL_USER)
        return EOF; // 不允许杀死内核进程
    if (task->pid == 1)
        return EOF; // 不允许杀死 init 进程

    LOGK("kill task %s pid %d signal %d\n", task->name, pid, sig);
    task->signal |= SIGMASK(sig);
    if (task->state == TASK_WAITING || task->state == TASK_SLEEPING) {
        task_unblock(task, -EINTR);
    }
    return 0;
}

void task_signal() {
    task_t *task = running_task();
    
    u32 map = task->signal & ~task->blocked; // 可处理的信号位图

    if (!map)
        return; // 没有可处理的信号
    
    assert(task->uid); // 内核进程不应该处理信号
    int sig = 1;

    for (; sig <= MAXSIG; sig++) {
        if (map & SIGMASK(sig)) {
            // 置空当前信号位
            task->signal &= ~SIGMASK(sig); // 清除正在处理的信号位
            break;
        }
    }

    sigaction_t *action = &task->actions[sig - 1];

    if (action->handler == SIG_IGN)
        return; // 忽略信号
    if (action->handler == SIG_DFL && sig == SIGCHLD)
        return; // 默认处理子进程状态变化信号 (SIGCHLD) 是忽略
    if (action->handler == SIG_DFL)
        task_exit(SIGMASK(sig)); // 默认处理是终止进程，退出状态码为信号位

    // 构造信号处理栈帧
    intr_frame_t *iframe = (intr_frame_t *)((u32)task + PAGE_SIZE - sizeof(intr_frame_t));

    signal_frame_t *frame = (signal_frame_t *)(iframe->esp - sizeof(signal_frame_t));

    frame->eip = iframe->eip;
    frame->eflags = iframe->eflags;
    frame->eax = iframe->eax;
    frame->ecx = iframe->ecx;
    frame->edx = iframe->edx;

    frame->blocked = EOF;   // 屏蔽所有信号

    // check flag nomask
    if (action->flags & SIG_NOMASK)
        frame->blocked = task->blocked; // 不修改阻塞信号位图

    // signal
    frame->sig = sig;
    
    frame->restorer = (u32)action->restorer;

    LOGK("old esp 0x%p\n", iframe->esp);
    iframe->esp = (u32)frame;
    LOGK("new esp 0x%p\n", iframe->esp);
    iframe->eip = (u32)action->handler;

    // 如果是一次性处理，调用后恢复默认处理函数
    if (action->flags & SIG_ONESHOT)
        action->handler = SIG_DFL;

    // 进程屏蔽码添加当前处理的信号位 mask
    task->blocked |= action->mask;
}