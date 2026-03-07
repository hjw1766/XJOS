#ifndef XJOS_INTERRUPT_H
#define XJOS_INTERRUPT_H


#include <xjos/types.h>


#define IDT_SIZE 256

#define INTR_DE 0   // 除零错误
#define INTR_DB 1   // 调试
#define INTR_NMI 2  // 不可屏蔽中断
#define INTR_BP 3   // 断点
#define INTR_OF 4   // 溢出
#define INTR_BR 5   // 越界
#define INTR_UD 6   // 指令无效
#define INTR_NM 7   // 协处理器不可用
#define INTR_DF 8   // 双重错误
#define INTR_OVER 9 // 协处理器段超限
#define INTR_TS 10  // 无效任务状态段
#define INTR_NP 11  // 段无效
#define INTR_SS 12  // 栈段错误
#define INTR_GP 13  // 一般性保护异常
#define INTR_PF 14  // 缺页错误
#define INTR_RE1 15 // 保留
#define INTR_MF 16  // 浮点异常
#define INTR_AC 17  // 对齐检测
#define INTR_MC 18  // 机器检测
#define INTR_XM 19  // SIMD 浮点异常
#define INTR_VE 20  // 虚拟化异常
#define INTR_CP 21  // 控制保护异常


enum {
    // Hardware IRQ numbers
    IRQ_CLOCK       = 0,    // Programmable Interval Timer (PIT)
    IRQ_KEYBOARD    = 1,    // Keyboard controller
    IRQ_CASCADE     = 2,    // Cascade line for the slave PIC
    IRQ_SERIAL_2    = 3,    // Serial port 2 (COM2)
    IRQ_SERIAL_1    = 4,    // Serial port 1 (COM1)
    IRQ_PARALLEL_2  = 5,    // Parallel port 2 (LPT2)
    IRQ_SB16        = 5,    // Sound Blaster 16 (uses IRQ5)
    IRQ_FLOPPY      = 6,    // Floppy disk controller
    IRQ_PARALLEL_1  = 7,    // Parallel port 1 (LPT1)
    IRQ_RTC         = 8,    // Real-Time Clock
    IRQ_REDIRECT    = 9,    // Redirected IRQ2, often available for peripherals
    IRQ_MOUSE       = 12,   // PS/2 Mouse controller
    IRQ_MATH        = 13,   // Math co-processor (FPU)
    IRQ_HARDDISK    = 14,   // Primary ATA hard disk controller
    IRQ_HARDDISK2   = 15,   // Secondary ATA hard disk controller

    // Base interrupt vector numbers for the PICs
    IRQ_MASTER_NR   = 0x20, // Base vector for the master PIC (IRQs 0-7)
    IRQ_SLAVE_NR    = 0x28, // Base vector for the slave PIC (IRQs 8-15)
};




typedef struct {
    u16 offset0;        // segment offset bits 0..15
    u16 selector;       // segment selector
    u8 reserved;        // reserved
    u8 type : 4;        // task / interrupt / trap
    u8 segment : 1;     // segment = 0  system segment,  = 1  code or data segment
    u8 DPL: 2;          // descriptor privilige level
    u8 present : 1;     // present bit
    u16 offset1;        // segment offset bits 16..31
}_packed gate_t;



typedef void* handler_t;

void send_eoi(int vector);

// Set the handler for the given IRQ number.
void set_interrupt_handler(u32 irq, handler_t handler);
void set_interrupt_mask(u32 irq, bool enable);
void set_exception_handler(u32 intr, handler_t handler);

bool interrupt_disable();                   // clear IF flag
bool get_interrupt_state();                 // get IF flag
void set_interrupt_state(bool state);       // set IF flag

#endif /* XJOS_INTERRUPT_H */
