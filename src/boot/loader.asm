[org 0x1000]


dw 0x55aa  ; assert magic number







mov si, string
call print


detect_memory:
    xor ebx, ebx

    mov ax, 0
    mov es, ax      ; ARDS buffer, return info es:di pointer
    mov edi, ards_buffer

    ; 16bits machine use 32bits, add 0x66
    mov edx, 0x534d4150  ; 'SMAP'  start accept

.next:
    ; mode
    mov eax, 0xe820
    mov ecx, 20       ; words

    int 0x15            ; BIOS interrupt

    jc error            ; if cf = 1

    add di, cx         ; move pointer to next entry
    inc dword [ards_count]  ; 32bits dword

    cmp ebx, 0          ; if ebx = 0, end of table
    jnz .next

    mov si, detecting
    call print

    jmp prepare_protected_mode  ; jump to protected mode


print:
    mov ah, 0x0e    ; mdoe
.next:
    mov al, [ds:si]    ; mov al [ds:si]
    cmp al, 0
    jz .done
    int 0x10        ; load ax
    inc si
    jmp .next

.done:
    ret

string:
    db "Loading XJOS...", 10, 13, 0

detecting:
    db "Detecting Memory Loading Successfully...", 10, 13, 0


error:
    mov si, .msg
    call print
    hlt
    .msg db  "Loading failed.", 10, 13, 0


prepare_protected_mode:
    
    cli; off interrupt

    ; turn on 0x92
    in al, 0x92
    or al, 0b10
    out 0x92, al

    lgdt [gdt_ptr]   ; load gdt

    ; startup protected mode
    mov eax, cr0
    or eax, 1
    mov cr0, eax

    ; dword 4 bytes (32bits)
    jmp dword code_selector:protected_mode
    ; code_selector: 0x08

[bits 32]
protected_mode:
    mov ax, data_selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov esp, 0x10000  ; stack pointer

    mov edi, 0x10000
    mov ecx, 10
    mov bl, 200
    
    call read_disk

    mov eax, 0x20250901 ; magic number
    mov ebx, ards_count

    ; jump to kernel
    jmp dword code_selector:0x10000

    ; hlt
    ud2

jmp $


read_disk:
    mov dx, 0x1f2     ; read sector acomount
    mov al, bl
    out dx, al        ; al/ax/eax

    ; 0 ~ 7 bits
    inc dx ; dx + 1 = 0x1f3
    mov al, cl
    out dx, al

    ; 8 ~ 15 bits
    inc dx
    shr ecx, 8
    mov al, cl
    out dx, al

    ; 16 ~ 23 bits
    inc dx
    shr ecx, 8
    mov al, cl
    out dx, al

    ; 24 ~ 27 bits
    inc dx
    shr ecx, 8  ; now 24 ~ 31 bits
    and cl, 0b1111  ; set 24 ~ 27
    mov al, 0b1110_0000 ; set 28 ~ 31
    or al, cl
    out dx, al  ; LBA mode

    inc dx
    mov al, 0x20    ; read command
    out dx, al


    xor ecx, ecx     ; clear ecx
    mov cl, bl       ; sector size

    .read:
        push cx
        call .waits     ; wait for disk ready
        call .reads     ; read sector
        pop cx
        loop .read      ; `cx` control cycling amount
    
    ret

    .waits:
        mov dx, 0x1f7    ; disk status
        .check:
            in al, dx
            ; delay
            jmp $+2
            jmp $+2
            jmp $+2
        
            and al, 0b1000_1000
            cmp al, 0b0000_1000 ; disk ready
            jnz .check
        ret

    .reads:
        mov dx, 0x1f0    ; data port
        mov cx, 256      ; read 256 words (512 bytes)
        .readw:
            in ax, dx
            jmp $+2
            jmp $+2
            jmp $+2
            mov [edi], ax
            add edi, 2    ; next word
            loop .readw
        ret


code_selector equ (1 << 3) ; 0x08
data_selector equ (2 << 3) ; 0x10

memory_base equ 0   ; memory base address
memory_limit equ ((1024 * 1024 * 1024 * 4) / (1024 * 4)) - 1 ; memory limit address (20bits)


; bochs gdtr 0x100F0(0x17) gdt_base((gdt_end - gdt_base) - 1)
gdt_ptr:
    dw (gdt_end - gdt_base) - 1   ; limit 0 ~ 15bits
    dd gdt_base   ; int base

gdt_base:
    dd 0, 0         ; null code selector 4 bytes, null data selector 4 bytes
gdt_code:
    dw memory_limit & 0xffff   ; segmet limit low 0 ~ 15bits
    dw memory_base & 0xffff    ; base address low 0 ~ 15bits
    db (memory_base >> 16) & 0xff   ; base address high 16 ~ 23bits
    
    ; exist - DLP - S - code - C - R - A 
    ; P | DPL(2bits) | S | TYPE | C/E | R/W | A |
    db 0b_1_00_1_1_0_1_0 

    ; 4k - 32bits - no_64 - available - limit 16 ~ 19bits
    db 0b1_1_0_0_0000 | (memory_limit >> 16) & 0xf
    
    ; base address high 24 ~ 31bits
    db (memory_base >> 24) & 0xff

gdt_data:
    dw memory_limit & 0xffff   ; segmet limit low 0 ~ 15bits
    dw memory_base & 0xffff    ; base address low 0 ~ 15bits
    db (memory_base >> 16) & 0xff   ; base address high 16 ~ 23bits
    
    ; exist - DLP - S - data - E(up) - W - A 
    ; P | DPL(2bits) | S | TYPE | C/E | R/W | A |
    db 0b_1_00_1_0_0_1_0 

    ; 4k - 32bits - no_64 - available - limit 16 ~ 19bits
    db 0b1_1_0_0_0000 | (memory_limit >> 16) & 0xf
    
    ; base address high 24 ~ 31bits
    db (memory_base >> 24) & 0xff

gdt_end:

ards_count:
    dd 0
ards_buffer:    ; get ARDS structure, ARDS is a structure to describe memory map 
