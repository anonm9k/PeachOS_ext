[BITS 32]
section .asm

global _start

_start: 
    push message
    mov eax, 1 
    int 80h
    add esp, 4
    jmp $

section .data
message: db ' Hellow World', 0