extern void interrupt_init();
extern void clock_init();
extern void timer_init();
extern void memory_map_init();
extern void mapping_init();
extern void arena_init();
extern void file_init();
extern void task_init();
extern void syscall_init();
extern void tss_init();
extern void fpu_init();
extern void pci_init();
extern void pbuf_init();

#include <xjos/interrupt.h>


#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

void kernel_init() {
    // 1. CPU 结构与内存管理初始化 (最先执行，绝对不能乱)
    tss_init();          // 初始化任务状态段
    memory_map_init();   // 解析物理内存容量
    mapping_init();      // 建立内核页表映射 (Paging)
    arena_init();        // 初始化内核堆内存分配器 (kmalloc/kfree)

    // 2. 中断与核心时钟系统
    interrupt_init();    // 初始化中断描述符表 (IDT) 和 8259A 芯片
    clock_init();        // 初始化系统时钟 (PIT 8253/8254)
    timer_init();        // 初始化内核软件定时器链表
    
    fpu_init();          // 初始化 FPU 相关设置
    pci_init();          // 初始化 PCI 总线

    // 3. 系统调用接口
    syscall_init();      // 注册所有的 sys_* 系统调用

    // 4. 先初始化全局文件表，避免后续任务创建后再被清零
    file_init();

    // 5. 任务调度子系统初始化
    task_init();

    pbuf_init();        // 初始化网络缓冲区管理器

    // 6. 开启中断，将控制权正式移交给调度器
    // 此时时钟中断开始触发，调度器会选中 init_thread 开始执行设备和文件系统的初始化
    set_interrupt_state(true);
}
