#include <xjos/fpu.h>
#include <xjos/task.h>
#include <xjos/cpu.h>
#include <xjos/interrupt.h>
#include <xjos/arena.h>
#include <xjos/debug.h>
#include <xjos/assert.h>


#define LOGK(fmt, args...) DEBUGK(fmt, ##args)


task_t *last_fpu_task = NULL;

static u32 cr0_read() {
    u32 cr0;
    asm volatile("movl %%cr0, %%eax\n" : "=a"(cr0));
    return cr0;
}

static void cr0_write(u32 cr0) {
    asm volatile("movl %%eax, %%cr0\n" : : "a"(cr0));
}

static void fpu_set_ts() {
    asm volatile(
        "movl %%cr0, %%eax\n"
        "orl %0, %%eax\n"
        "movl %%eax, %%cr0\n"
        :
        : "i"(CR0_TS)
        : "eax", "memory");
}

static void fpu_clear_ts() {
    asm volatile("clts" ::: "memory");
}

bool fpu_check() {
    cpu_version_t ver;
    cpu_version(&ver);
    if (!ver.FPU)
        return false;

    u32 testword = 0x55AA;
    u32 ret;
    asm volatile(
        "movl %%cr0, %%edx\n" // 获取 cr0 寄存器
        "andl %%ecx, %%edx\n" // 清除 EM TS 保证 FPU 可用
        "movl %%edx, %%cr0\n" // 设置 cr0 寄存器

        "fninit\n"    // 初始化 FPU
        "fnstsw %1\n" // 保存状态字到 testword

        "movl %1, %%eax\n" // 将状态字保存到 eax
        : "=a"(ret)        // 将 eax 写入 ret;
        : "m"(testword), "c"(-1 - CR0_EM - CR0_TS));
    return ret == 0; // 如果状态被改为 0 则 FPU 可用
}

void fpu_enable(task_t *task) {
    fpu_clear_ts();

    // 如果当前任务已经是 FPU 任务了，就不需要重复启用了
    if (last_fpu_task == task) {
        LOGK("fpu already enabled for this task\n");
        return;
    }

    // 如果之前有任务使用了 FPU，先保存它的状态
    if (last_fpu_task && last_fpu_task->flags & TASK_FPU_ENABLED) {
        assert(last_fpu_task->fpu);
        asm volatile("fnsave (%%eax) \n" ::"a"(last_fpu_task->fpu));
        last_fpu_task->flags &= ~TASK_FPU_ENABLED;
    }

    last_fpu_task = task;

    // 任务第一次使用 FPU，需要初始化它的状态
    if (task->fpu) {
        asm volatile("frstor (%%eax) \n" ::"a"(task->fpu));
    } else {

        asm volatile(
            "fnclex \n"
            "fninit \n");

        LOGK("FPU create state for task 0x%p\n", task);
        task->fpu = (fpu_t *)kmalloc(sizeof(fpu_t));
        task->flags |= (TASK_FPU_ENABLED | TASK_FPU_USED);
    }
}

void fpu_disable(task_t *task) {
    (void)task;
    if (!last_fpu_task)
        return;
    fpu_set_ts();
}

void fpu_handler(int vector) {
    LOGK("fpu handler...\n");
    assert(vector == INTR_NM);
    task_t *task = running_task();
    assert(task->uid);

    fpu_enable(task);
}

void fpu_init() {
    LOGK("fpu init...\n");

    bool exist = fpu_check();
    last_fpu_task = NULL;
    assert(exist);

    if(exist) {
        // 设置 FPU 异常处理函数
        set_exception_handler(INTR_NM, fpu_handler);
        // 硬件 FPU 存在时保留 EM=0，仅使用 TS 做惰性切换
        cr0_write((cr0_read() & ~CR0_EM) | CR0_TS | CR0_NE);
    } else {
        LOGK("fpu not exists...\n");
    }
}