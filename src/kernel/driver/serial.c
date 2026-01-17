#include <hardware/io.h>
#include <xjos/interrupt.h>
#include <xjos/fifo.h>
#include <xjos/task.h>
#include <xjos/mutex.h>
#include <xjos/assert.h>
#include <drivers/device.h>
#include <xjos/debug.h>
#include <xjos/stdarg.h>
#include <xjos/stdio.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

#define COM1_IOBASE 0x3F8 // 串口 1 基地址
#define COM2_IOBASE 0x2F8 // 串口 2 基地址

#define COM_DATA 0          // 数据寄存器
#define COM_INTR_ENABLE 1   // 中断允许
#define COM_BAUD_LSB 0      // 波特率低字节
#define COM_BAUD_MSB 1      // 波特率高字节
#define COM_INTR_IDENTIFY 2 // 中断识别
#define COM_LINE_CONTROL 3  // 线控制
#define COM_MODEM_CONTROL 4 // 调制解调器控制
#define COM_LINE_STATUS 5   // 线状态
#define COM_MODEM_STATUS 6  // 调制解调器状态

// 线状态
#define LSR_DR 0x1
#define LSR_OE 0x2
#define LSR_PE 0x4
#define LSR_FE 0x8
#define LSR_BI 0x10
#define LSR_THRE 0x20
#define LSR_TEMT 0x40
#define LSR_IE 0x80

#define BUF_LEN 64

typedef struct serial_t {
    u16 iobase;                 // I/O 端口基地址
    fifo_t rx_fifo;             // 接收 FIFO 缓冲区
    char rx_buf[BUF_LEN];       // read buffer
    mutex_t rlock;              // 读锁
    task_t *rx_waiter;          // read waiter task
    mutex_t wlock;              // 写锁
    task_t *tx_waiter;          // write waiter task
} serial_t;

static serial_t serials[2]; // COM1, COM2


void recv_data(serial_t *serial) {
    char ch = inb(serial->iobase);
    if (ch == '\r')
        ch = '\n';
    
    fifo_put(&serial->rx_fifo, ch);
    if (serial->rx_waiter != NULL ) {
        task_unblock(serial->rx_waiter);
        serial->rx_waiter = NULL;
    }
}

void serial_handler(int vector) {
    u32 irq = vector - 0x20;
    assert(irq == IRQ_SERIAL_1 || irq == IRQ_SERIAL_2);
    send_eoi(vector);

    serial_t *serial = &serials[irq - IRQ_SERIAL_1];
    u8 state = inb(serial->iobase + COM_LINE_STATUS);

    if (state & LSR_DR) { // 数据可用
        recv_data(serial);
    }

    // 可发数据且有等待的写任务
    if ((state & LSR_THRE) && serial->tx_waiter) {
        task_unblock(serial->tx_waiter);
        serial->tx_waiter = NULL;
    }
}

int serial_read(serial_t *serial, char *buf, u32 count) {
    mutex_lock(&serial->rlock);

    int nr = 0;
    while (nr < count) {
        while (fifo_empty(&serial->rx_fifo)) {
            assert(serial->rx_waiter == NULL);
            serial->rx_waiter = running_task();
            task_block(serial->rx_waiter, NULL, TASK_BLOCKED);
        }
        buf[nr++] = fifo_get(&serial->rx_fifo);
    }
    mutex_unlock(&serial->rlock);
    return nr;
}

int serial_write(serial_t *serial, char *buf, u32 count) {
    mutex_lock(&serial->wlock);

    int nr = 0;
    while (nr < count) {
        u8 state = inb(serial->iobase + COM_LINE_STATUS);
        if (state & LSR_THRE) {
            outb(serial->iobase, buf[nr++]);
            continue;
        }
        task_t *task = running_task();
        serial->tx_waiter = task;
        task_block(task, NULL, TASK_BLOCKED);
    }
    mutex_unlock(&serial->wlock);
    return nr;
}

void serial_init() {
    for (size_t i = 0; i < 2; i++) {
        serial_t *serial = &serials[i];
        fifo_init(&serial->rx_fifo, serial->rx_buf, BUF_LEN);
        serial->rx_waiter = NULL;
        mutex_init(&serial->rlock);
        serial->tx_waiter = NULL;
        mutex_init(&serial->wlock);

        u16 irq;
        if (!i) {
            irq = IRQ_SERIAL_1;
            serial->iobase = COM1_IOBASE;
        } else {
            irq = IRQ_SERIAL_2;
            serial->iobase = COM2_IOBASE;
        }

        // 激活 DLAB
        outb(serial->iobase + COM_LINE_CONTROL, 0x80);

        // 设置波特率为 2400 bps (115200 / 0x30)
        outb(serial->iobase + COM_BAUD_LSB, 0x30);
        outb(serial->iobase + COM_BAUD_MSB, 0x00);

        // reset DLAB, 设置数据位 8, 停止位 1, 无奇偶校验
        outb(serial->iobase + COM_LINE_CONTROL, 0x03);

        // 0x0d = 0b1101
        // 数据可用 + 中断/错误 + 状态变化 允许中断
        outb(serial->iobase + COM_INTR_ENABLE, 0x0d);

        // 启用本地环回测试
        outb(serial->iobase + COM_MODEM_CONTROL, 0b11011);
        
        outb(serial->iobase, 0xAE);

        if (inb(serial->iobase) != 0xAE) {
            LOGK("Serial COM%d not present\n", i + 1);
            continue;
        }

        // 正常工作，关闭环回，启用 IRQs
        outb(serial->iobase + COM_MODEM_CONTROL, 0b1011);

        // 注册中断回调函数,并打开中断
        set_interrupt_handler(irq, serial_handler);

        set_interrupt_mask(irq, true);

        char name[16];

        sprintf(name, "com%d", i + 1);

        device_install(
            DEV_CHAR, DEV_SERIAL, 
            serial, name, 0,
            NULL, serial_read, serial_write  
        );
        LOGK("Serial 0x%x init...\n", serial->iobase);
    }
}