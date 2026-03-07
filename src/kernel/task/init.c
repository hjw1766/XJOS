#include <xjos/types.h>
#include <xjos/task.h>
#include <xjos/stdio.h>
#include <xjos/interrupt.h>
#include <fs/buffer.h>


extern int sys_execve(char *filename, char *argv[], char *envp[]);
extern void init_user_thread();

// 子系统
extern void serial_init();
extern void keyboard_init();
extern void time_init();
extern void tty_init();

extern void ramdisk_init();
extern void ide_init();
extern void floppy_init();
extern void sb16_init();

extern void buffer_init();
extern void file_init();
extern void inode_init();
extern void super_init(); 
extern void dcache_init();
extern void dev_init();

static volatile bool task_sync_done = false;

void init_thread() {
    // 1. 基础硬件外设初始化
    serial_init();   // 初始化串口 (用于内核打印和调试)
    keyboard_init(); // 初始化键盘
    time_init();     // 初始化时间系统
    tty_init();      // 初始化 TTY 设备 (依赖键盘)

    // 2. 块设备驱动初始化
    ramdisk_init();  // 初始化内存虚拟磁盘
    ide_init();      // 初始化 IDE 硬盘设备
    sb16_init();     // 初始化 Sound Blaster 16 声卡
    floppy_init();   // 初始化软盘驱动 (如果存在)

    // 3. 文件系统核心初始化 (必须在块设备就绪后进行)
    buffer_init();   // 初始化高速缓冲 (依赖底层的块设备读写)
    file_init();     // 初始化文件表
    inode_init();    // 初始化 inode 缓存
    super_init();    // 初始化并挂载超级块 (解析磁盘上的文件系统结构)
    dcache_init();     // 初始化目录项缓存 (依赖超级块和 inode)

    // 4. 上层设备文件抽象初始化

    dev_init();      // 初始化 /dev 下的设备文件节点

    task_sync_done = true; // 标记初始化完成
    // 5. 进入用户模式，运行 init 进程
#ifdef XJOS_DEBUG

    task_to_user_mode(init_user_thread);

#else

    // Enter user mode and run /bin/init (a tiny supervisor that respawns /bin/sh).
    task_prepare_user_mode();
    char *argv[] = {"init", NULL};
    char *envp[] = {"HOME=/", "PATH=/bin", NULL};
    sys_execve("/bin/init", argv, envp);
    panic("init: failed to exec /bin/init");
#endif
}  


void sync_thread() {
    set_interrupt_state(true);
    while (true) {
        bool intr = interrupt_disable();
        if (task_sync_done)
            bsync();
        task_sleep(5000); // every 5 seconds
        set_interrupt_state(intr);
    }
}