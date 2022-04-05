section .asm

global tss_load

; TSS will go somewhere in GDT, and it will have its own offset in the gdt_real (check kernel.c)
; Here we pass that offset to tss_load
tss_load:
    push ebp
    mov ebp, esp
    mov ax, [ebp+8] ; TSS Segment offset
    ltr ax
    pop ebp
    ret 
