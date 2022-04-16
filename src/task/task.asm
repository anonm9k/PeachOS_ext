[BITS 32]

section .asm

global restore_general_purpose_registers
global task_return
global user_registers


; void task_return(struct registers* regs);
; this function will send us to user land
task_return:
    mov ebp, esp
    ; PUSH THE DATA SEGMENT (SS WILL BE FINE) ; Check GDT 
    ; PUSH THE STACK ADDRESS
    ; PUSH THE FLAGS
    ; PUSH THE CODE SEGMENT
    ; PUSH IP

    ; Let's access the structure passed to us
    mov ebx, [ebp+4] ; This is the registers structure
    ; push the data/stack selector
    push dword [ebx+44]
    ; Push the stack pointer
    push dword [ebx+40]

    ; Push the flags
    pushf
    pop eax ; flags?
    or eax, 0x200 ; to enable interrupts
    push eax

    ; Push the code segment
    push dword [ebx+32]

    ; Push the IP to execute
    push dword [ebx+28] ; actually a virtual address

    ; Setup some segment registers
    mov ax, [ebx+44]
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push dword [ebp+4] ; this is he whole register structure we push to the stack, so we can call restore
    call restore_general_purpose_registers
    add esp, 4

    ; Let's leave kernel land and execute in user land!
    iretd

; void restore_general_purpose_registers(struct registers* regs);
restore_general_purpose_registers:
    push ebp
    mov ebp, esp
    mov ebx, [ebp+8]
    mov edi, [ebx]
    mov esi, [ebx+4]
    mov ebp, [ebx+8]
    mov edx, [ebx+16]
    mov ecx, [ebx+20]
    mov eax, [ebx+24]
    mov ebx, [ebx+12]
    add esp, 4
    ret

; void user_registers()
user_registers:
    ; Set up some segment register
    mov ax, 0x23 ; just changes the data registers value to 0x23 which is the user data segment (see kernel.c)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    ret