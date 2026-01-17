[org 0x7c00]


mov ax, 3
int 0x10 ; al = 0x00, ah = 0x03

; init 
mov ax, 0
mov ds, ax
mov es, ax
mov ss, ax
mov sp, 0x7c00

; xchg bx, bx ; bochs debug

mov si, string
call print

; xchg bx, bx ; bochs debug

mov edi, 0x1000     ; cpu -> memory 0x1000
mov ecx, 2          ; staring sector       
mov bl, 4           ; sector size



call read_disk

; 0x1000 0xaa 0x1001 0x55
cmp word [0x1000], 0x55aa   ; small store
; if magic number is not correct, jump to error
jnz error

; jump to kernel
jmp 0:0x1002


jmp $


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



string:
    db "Booting XJOS...", 10, 13, 0  ; '\n' '\r'

error:
    mov si, .msg
    call print
    hlt
    .msg db "Booting failed.", 10, 13, 0


times 510 - ($ - $$) db 0
db 0x55, 0xaa