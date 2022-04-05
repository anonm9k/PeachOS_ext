[BITS 32]

global _start
extern kernel_main
extern kernel_registers

CODE_SEG equ 0x08
DATA_SEG equ 0x10

_start:
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov ebp, 0x00200000
    mov esp, ebp

    ; Enable the A20 line
    in al, 0x92
    or al, 2
    out 0x92, al

    ; Remap the master PIC
    mov al, 00010001b
    out 0x20, al ; Tell master PIC

    mov al, 0x20 ; Interrupt 0x20 is where master ISR should start
    out 0x21, al

    mov al, 00000001b
    out 0x21, al
    ; End remap of the master PIC

    call kernel_main

    jmp $

kernel_registers:
    ; Set up some segment register
    mov ax, 0x10 ; just changes the data registers value to 0x10 which is the kernel data segment (see config.h)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    ret
    
times 512-($ - $$) db 0
