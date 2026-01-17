#include <xjos/interrupt.h>
#include <xjos/global.h>
#include <xjos/debug.h>
#include <xjos/printk.h>
#include <xjos/stdlib.h>
#include <hardware/io.h>
#include <xjos/assert.h>


#define LOGK(fmt, args...) DEBUGK(fmt, ##args)


#define ENTRY_SIZE 0x30

#define PIC_M_CTRL 0x20
#define PIC_M_DATA 0x21
#define PIC_S_CTRL 0xa0
#define PIC_S_DATA 0xa1
#define PIC_EOI 0x20

gate_t idt[IDT_SIZE];
gdt_ptr_t idt_ptr;

// all interrupt handlers func
handler_t handler_table[IDT_SIZE];

extern handler_t handler_entry_table[ENTRY_SIZE];
extern void syscall_handler();
extern void page_fault();


static char *messages[] = {
    "#DE Divide Error\0",
    "#DB RESERVED\0",
    "--  NMI Interrupt\0",
    "#BP Breakpoint\0",
    "#OF Overflow\0",
    "#BR BOUND Range Exceeded\0",
    "#UD Invalid Opcode (Undefined Opcode)\0",
    "#NM Device Not Available (No Math Coprocessor)\0",
    "#DF Double Fault\0",
    "    Coprocessor Segment Overrun (reserved)\0",
    "#TS Invalid TSS\0",
    "#NP Segment Not Present\0",
    "#SS Stack-Segment Fault\0",
    "#GP General Protection\0",
    "#PF Page Fault\0",
    "--  (Intel reserved. Do not use.)\0",
    "#MF x87 FPU Floating-Point Error (Math Fault)\0",
    "#AC Alignment Check\0",
    "#MC Machine Check\0",
    "#XF SIMD Floating-Point Exception\0",
    "#VE Virtualization Exception\0",
    "#CP Control Protection Exception\0",
};


// send interrupt handler EOI
void send_eoi(int vector) {
    if (vector >= 0x20 && vector < 0x28)
        outb(PIC_M_CTRL, PIC_EOI);
    if (vector >= 0x28 && vector < 0x30) {
        outb(PIC_M_CTRL, PIC_EOI);      // master
        outb(PIC_S_CTRL, PIC_EOI);      // slave
    }
}


void set_interrupt_handler(u32 irq, handler_t handler) {
    assert(irq >= 0 && irq < 16);
    handler_table[IRQ_MASTER_NR + irq] = handler;
}


void set_interrupt_mask(u32 irq, bool enable) {
    assert(irq >= 0 && irq < 16);

    u16 port;

    if (irq < 8) {
        port = PIC_M_DATA;  //master
    } else {
        port = PIC_S_DATA;  // slave
        irq -= 8;
    }

    // exp. irp = 2, 1 << irq, 00000100
    if (enable) {
        outb(port, inb(port) & ~(1 << irq));
    } else {
        outb(port, inb(port) | (1 << irq));
    }
}


void default_handler(int vector) {
    send_eoi(vector);
    // schedule();
    DEBUGK("[%x] default interrupt called %d...\n", vector);
}


void exception_handler(int vector, u32 edi, u32 esi, u32 ebp, u32 esp,
    u32 ebx, u32 edx, u32 ecx, u32 eax,
    u32 gs, u32 fs, u32 es, u32 ds,
    u32 vector0, u32 error, u32 eip, u32 cs, u32 eflags) {
    char *message = NULL;

    // if vector < 22 excep
    if (vector < 22) {
        message = messages[vector];
    } else {
        // int 
        message = messages[15];
    }

    printk("\nEXCEPTION: %s\n", messages[vector]);
    printk("    VECTOR : 0x%02x\n", vector);
    printk("    ERROR  : 0x%02x\n", error);
    printk("    EFLAGS : 0x%08x\n", eflags);
    printk("    CS     : 0x%02x\n", cs);
    printk("    EIP    : 0x%08x\n", eip);
    printk("    ESP    : 0x%08x\n", esp);

    // stoppage
    hang();
}


// pic init ICW1 -> ICW2 -> ICW3 -> ICW4
static void pic_init() {
    outb(PIC_M_CTRL, 0b00010001);   // master init, set ICW1
    outb(PIC_M_DATA, 0x20);         // IR0 - IR7 (0x20 - 0x27)
    outb(PIC_M_DATA, 0b00000100);   // IR2 with slave
    outb(PIC_M_DATA, 0b00000001);   // 8086, Manual send eoi


    outb(PIC_S_CTRL, 0b00010001);    // slave init
    outb(PIC_S_DATA, 0x28);         // IR8 - IR15 (0x28 - 0x2f)
    outb(PIC_S_DATA, 2);            // slave connected to master, IR2
    outb(PIC_S_DATA, 0b00000001);   // 8086, Manual send eoi

    outb(PIC_M_DATA, 0b11111111);   // mask all interrupts
    outb(PIC_S_DATA, 0b11111111);   // mask all interrupts
}


static void idt_init() {
    for (size_t i = 0; i < ENTRY_SIZE; i++) {
        gate_t *gate = &idt[i];

        
        // register handler
        handler_t handler = handler_entry_table[i];


        gate->offset0 = (u32)handler & 0xffff;
        gate->offset1 = ((u32)handler >> 16) & 0xffff;
        gate->selector = 1 << 3;
        gate->reserved = 0;
        gate->type = 0b1110;    // interrupt gate
        gate->segment = 0;
        gate->DPL = 0;          // kernel DPL
        gate->present = 1;
    }

    // register exception handler, wait asm code invoke
    for (size_t i = 0; i < 0x20; i++) {
        handler_table[i] = exception_handler;
    }

    handler_table[0xe] = page_fault;

    for (size_t i = 0x20; i < ENTRY_SIZE; i++) {
        handler_table[i] = default_handler;
    }

    // init syscall
    gate_t *gate = &idt[0x80];
    gate->offset0 = (u32)syscall_handler & 0xffff;
    gate->offset1 = ((u32)syscall_handler >> 16) & 0xffff;
    gate->selector = 1 << 3;    //code
    gate->reserved = 0;
    gate->type = 0b1110;        // interrupt gate
    gate->segment = 0;
    gate->DPL = 3;              // user DPL
    gate->present = 1;



    idt_ptr.base = (u32)idt;
    idt_ptr.limit = sizeof(idt) - 1;
    asm volatile("lidt idt_ptr\n");
}


void interrupt_init() {
    pic_init();
    idt_init();
}


bool interrupt_disable() {
    asm volatile(
        "pushfl\n"
        "cli\n"                 // clear IF flag
        "popl %eax\n"
        "shrl $9, %eax\n"
        "andl $1, %eax\n"       // get IF flag
    );
}


bool get_interrupt_state() {
    asm volatile(
        "pushfl\n"
        "popl %eax\n"
        "shrl $9, %eax\n"
        "andl $1, %eax\n"
    );
}


/*
    Atomic operation
    bool state = get_interrupt_state();
    // do something
    set_interrupt_state(state);
*/
void set_interrupt_state(bool state) {
    if (state)
        asm volatile("sti");
    else
        asm volatile("cli");
}
