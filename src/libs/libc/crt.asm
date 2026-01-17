[bits 32]


section .text
global _start

extern __libc_start_main
extern _init
extern _fini
extern main

; LD 指定的入口点
_start:
    xor ebp, ebp          ; clear ebp
    pop esi ; 栈顶参数为 argc
    mov ecx, esp ; ecx 指向 argv[0]

    add esp, -16 ; 为环境变量预留空间
    push eax;   ??
    push esp;   用户程序最大栈地址
    push edx;   动态链接器
    push _init; 初始化函数
    push _fini; 结束函数
    push ecx;   argv
    push esi;   argc
    push main;  用户程序入口点

    call __libc_start_main

    ud2