[bits 32]

section .text  ; code section

global inb ; export inb
inb:
    push ebp ; save frame pointer
    mov ebp, esp ; save stack pointer

    xor eax, eax ; clear eax
    mov edx, [ebp + 8] ; get port number
    in al, dx ; read 8 bits from port dx

    jmp $+2 ; delay
    jmp $+2 ; delay
    jmp $+2 ; delay

    leave ; restore stack pointer
    ret

global outb
outb:
    push ebp ; save frame pointer
    mov ebp, esp ; save stack pointer

    mov edx, [ebp + 8] ; get port number
    mov eax, [ebp + 12] ; get value
    out dx, al ; write 8 bits to port dx

    jmp $+2 ; delay
    jmp $+2 ; delay
    jmp $+2 ; delay

    leave ; restore stack pointer
    ret

global inw
inw:
    push ebp; 
    mov ebp, esp

    xor eax, eax 
    mov edx, [ebp + 8]
    in ax, dx

    jmp $+2
    jmp $+2 
    jmp $+2 

    leave
    ret

global outw
outw:
    push ebp; 
    mov ebp, esp

    mov edx, [ebp + 8]
    mov eax, [ebp + 12]
    out dx, ax

    jmp $+2 
    jmp $+2  
    jmp $+2 

    leave 
    ret

global inl
inl:
    push ebp; 
    mov ebp, esp

    xor eax, eax 
    mov edx, [ebp + 8]
    in eax, dx   ; 32bit

    jmp $+2
    jmp $+2 
    jmp $+2 

    leave
    ret

global outl
outl:
    push ebp; 
    mov ebp, esp

    mov edx, [ebp + 8]
    mov eax, [ebp + 12]
    out dx, eax

    jmp $+2 
    jmp $+2  
    jmp $+2 

    leave 
    ret