section .asm
global gdt_load

gdt_load:
    mov eax, [esp+4] ; GDT start address
    mov [gdt_descriptor + 2], eax
    mov ax, [esp+8] ; GDT size
    mov [gdt_descriptor], ax
    lgdt [gdt_descriptor]
    ret


section .data
gdt_descriptor:
    ; They are empty now, we get them from gdt_load call in gdt.c file
    dw 0x00 ; Size
    dd 0x00 ; GDT Start Address