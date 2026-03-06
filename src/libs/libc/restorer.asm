[bits 32]

extern ssetmask
section .text
global restorer

restorer:
    add esp, 4 ;跳过sig
    call ssetmask ; ssetmask(blocked) 恢复被阻塞的信号
    add esp, 4 ;blocked

    pop eax
    pop ecx
    pop edx
    popf
    ret