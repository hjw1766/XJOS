[bits 32]
; Interrupt handler

extern handler_table

section .text

; 0 intel event / 1 outside event
%macro INTERRUPT_HANDLER 2  ; 2args
interrupt_handler_%1:       ; x_0x..
    %if %2 == 0                     ; check second arg (0 or 1)
        push 0
    %endif
    push %1                 ; 1
    jmp interrupt_entry
%endmacro




interrupt_entry:
    ; mov eax, [esp]  ; [esp] = interrupt number

    ; call [handler_table + eax * 4]  ; eax * 4(bytes) = offset of handler in table

    ; add esp, 8

    ; iret
    push ds
    push es
    push fs
    push gs
    pusha

    ; interrupt number
    mov eax, [esp + 12 * 4]

    push eax    ; stack top is func first argment


    call [handler_table + eax * 4]

 global interrupt_exit
 interrupt_exit:   

    add esp, 4

    popa
    pop gs
    pop fs
    pop es
    pop ds


    add esp, 8
    iret



; --- CPU Exceptions ---
INTERRUPT_HANDLER 0x00, 0; #DE - Divide-by-zero Error
INTERRUPT_HANDLER 0x01, 0; #DB - Debug
INTERRUPT_HANDLER 0x02, 0; NMI - Non-maskable Interrupt
INTERRUPT_HANDLER 0x03, 0; #BP - Breakpoint
INTERRUPT_HANDLER 0x04, 0; #OF - Overflow
INTERRUPT_HANDLER 0x05, 0; #BR - Bound Range Exceeded
INTERRUPT_HANDLER 0x06, 0; #UD - Invalid Opcode
INTERRUPT_HANDLER 0x07, 0; #NM - Device Not Available (No Math Coprocessor)

INTERRUPT_HANDLER 0x08, 1; #DF - Double Fault
INTERRUPT_HANDLER 0x09, 0; Coprocessor Segment Overrun (Obsolete)
INTERRUPT_HANDLER 0x0a, 1; #TS - Invalid TSS
INTERRUPT_HANDLER 0x0b, 1; #NP - Segment Not Present
INTERRUPT_HANDLER 0x0c, 1; #SS - Stack-Segment Fault
INTERRUPT_HANDLER 0x0d, 1; #GP - General Protection Fault
INTERRUPT_HANDLER 0x0e, 1; #PF - Page Fault
INTERRUPT_HANDLER 0x0f, 0; (Intel reserved)

INTERRUPT_HANDLER 0x10, 0; #MF - x87 FPU Floating-Point Error (Math Fault)
INTERRUPT_HANDLER 0x11, 1; #AC - Alignment Check
INTERRUPT_HANDLER 0x12, 0; #MC - Machine Check
INTERRUPT_HANDLER 0x13, 0; #XF - SIMD Floating-Point Exception
INTERRUPT_HANDLER 0x14, 0; #VE - Virtualization Exception
INTERRUPT_HANDLER 0x15, 1; #CP - Control Protection Exception
; Vectors 0x16 to 0x1F are mostly reserved or for specific modern features

INTERRUPT_HANDLER 0x16, 0; #PF - Reserved
INTERRUPT_HANDLER 0x17, 0; #PF - Reserved
INTERRUPT_HANDLER 0x18, 0; #PF - Reserved
INTERRUPT_HANDLER 0x19, 0; #PF - Reserved
INTERRUPT_HANDLER 0x1a, 0; #PF - Reserved
INTERRUPT_HANDLER 0x1b, 0; #PF - Reserved
INTERRUPT_HANDLER 0x1c, 0; #PF - Reserved
INTERRUPT_HANDLER 0x1d, 0; #PF - Reserved
INTERRUPT_HANDLER 0x1e, 0; #PF - Reserved
INTERRUPT_HANDLER 0x1f, 0; #PF - Reserved

; --- Hardware Interrupts (IRQ Mapping for 8259A PIC) ---
INTERRUPT_HANDLER 0x20, 0; IRQ  0 - Programmable Interval Timer (PIT)
INTERRUPT_HANDLER 0x21, 0; IRQ  1 - Keyboard Controller
INTERRUPT_HANDLER 0x22, 0; IRQ  2 - Cascade (for second PIC)
INTERRUPT_HANDLER 0x23, 0; IRQ  3 - COM2 (Serial Port 2)
INTERRUPT_HANDLER 0x24, 0; IRQ  4 - COM1 (Serial Port 1)
INTERRUPT_HANDLER 0x25, 0; IRQ  5 - LPT2 (Parallel Port 2), or Sound Card
INTERRUPT_HANDLER 0x26, 0; IRQ  6 - Floppy Disk Controller
INTERRUPT_HANDLER 0x27, 0; IRQ  7 - LPT1 (Parallel Port 1)

; --- Secondary PIC ---
INTERRUPT_HANDLER 0x28, 0; IRQ  8 - Real-time Clock (RTC)
INTERRUPT_HANDLER 0x29, 0; IRQ  9 - (Available, often for network cards)
INTERRUPT_HANDLER 0x2a, 0; IRQ 10 - (Available, often for network/USB)
INTERRUPT_HANDLER 0x2b, 0; IRQ 11 - (Available, often for graphics/SATA)
INTERRUPT_HANDLER 0x2c, 0; IRQ 12 - PS/2 Mouse
INTERRUPT_HANDLER 0x2d, 0; IRQ 13 - x87 FPU Co-processor
INTERRUPT_HANDLER 0x2e, 0; IRQ 14 - Primary ATA Hard Disk
INTERRUPT_HANDLER 0x2f, 0; IRQ 15 - Secondary ATA Hard Disk




section .data
global handler_entry_table
handler_entry_table:
    dd interrupt_handler_0x00
    dd interrupt_handler_0x01
    dd interrupt_handler_0x02
    dd interrupt_handler_0x03
    dd interrupt_handler_0x04
    dd interrupt_handler_0x05
    dd interrupt_handler_0x06
    dd interrupt_handler_0x07
    dd interrupt_handler_0x08
    dd interrupt_handler_0x09
    dd interrupt_handler_0x0a
    dd interrupt_handler_0x0b
    dd interrupt_handler_0x0c
    dd interrupt_handler_0x0d
    dd interrupt_handler_0x0e
    dd interrupt_handler_0x0f
    dd interrupt_handler_0x10
    dd interrupt_handler_0x11
    dd interrupt_handler_0x12
    dd interrupt_handler_0x13
    dd interrupt_handler_0x14
    dd interrupt_handler_0x15
    dd interrupt_handler_0x16
    dd interrupt_handler_0x17
    dd interrupt_handler_0x18
    dd interrupt_handler_0x19
    dd interrupt_handler_0x1a
    dd interrupt_handler_0x1b
    dd interrupt_handler_0x1c
    dd interrupt_handler_0x1d
    dd interrupt_handler_0x1e
    dd interrupt_handler_0x1f
    dd interrupt_handler_0x20
    dd interrupt_handler_0x21
    dd interrupt_handler_0x22
    dd interrupt_handler_0x23
    dd interrupt_handler_0x24
    dd interrupt_handler_0x25
    dd interrupt_handler_0x26
    dd interrupt_handler_0x27
    dd interrupt_handler_0x28
    dd interrupt_handler_0x29
    dd interrupt_handler_0x2a
    dd interrupt_handler_0x2b
    dd interrupt_handler_0x2c
    dd interrupt_handler_0x2d
    dd interrupt_handler_0x2e
    dd interrupt_handler_0x2f

section .text

extern syscall_check
extern syscall_table
global syscall_handler
syscall_handler:
    ; xchg bx, bx

    push eax
    call syscall_check
    add esp, 4

    push 0x20250901         ; match stack

    push 0x80

    push ds
    push es
    push fs
    push gs
    pusha

    push 0x80
    ; xchg bx, bx

    push ebp    ; arg6
    push edi    ; arg5
    push esi    ; arg4
    push edx    ; arg3
    push ecx    ; arg2
    push ebx    ; arg1

    call [syscall_table + eax * 4]

    ; xchg bx, bx
    add esp, (6 * 4)     ; system invoke return

    mov dword [esp + 8 * 4], eax
    
    jmp interrupt_exit