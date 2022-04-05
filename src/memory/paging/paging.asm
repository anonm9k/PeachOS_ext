[BITS 32]

section .asm

global paging_load_directory
global enable_paging

; Which directory we want to use at the moment
paging_load_directory:
    push ebp
    mov ebp, esp
    mov eax, [ebp+8]
    mov cr3, eax ; Address of the page directory loaded to CR3
    pop ebp
    ret

enable_paging:
    push ebp
    mov ebp, esp
    mov eax, cr0
    or eax, 0x80000000 ; CR0 loaded with PG and PE bits set
    mov cr0, eax 
    pop ebp
    ret
