section .asm ; IDT will be loaded at the beginning of everything basically. from address 0x00 in memory

extern int21h_handler
extern no_interrupt_handler
extern isr80h_handler
extern interrupt_handler

global idt_load
global no_interrupt
global enable_interrupts
global disable_interrupts
global isr80h_wrapper
global interrupt_pointer_table

enable_interrupts:
    sti
    ret

disable_interrupts:
    cli
    ret


idt_load:
    push ebp
    mov ebp, esp
    mov ebx, [ebp+8]
    lidt [ebx]
    pop ebp    
    ret

no_interrupt:
    cli
    pushad
    call no_interrupt_handler
    popad
    sti
    iret

%macro interrupt 1
    global int%1
    int%1:
        ; INTERRUPT FRAME START
        ; ALREADY PUSHED TO US BY THE PROCESSOR UPON ENTRY TO THIS INTERRUPT
        ; uint32_t ip
        ; uint32_t cs;
        ; uint32_t flags
        ; uint32_t sp;
        ; uint32_t ss;
        ; Pushes the general purpose registers to the stack
        pushad
        ; Interrupt frame end
        push esp
        push dword %1
        call interrupt_handler
        add esp, 8
        popad
        iret
%endmacro

%assign i 0
%rep 512
    interrupt i
%assign i i+1
%endrep

isr80h_wrapper:
    ; INTERRUPT FRAME START
    ; ALREADY PUSHED TO US BY THE PROCESSOR UPON ENTRY TO THIS INTERRUPT
    ; These are the tasks  registers when it invoked the interrupt routine
    ; uint32_t ip
    ; uint32_t cs;
    ; uint32_t flags
    ; uint32_t sp;
    ; uint32_t ss;
    ; Pushes the general purpose registers to the stack
    pushad

    ; INTERRUPT FRAME END

    ; Push the stack pointer (of that task that was running) so that we are pointing to the interrupt frame
    push esp ; we can use this pointer in c

    ; EAX holds our command lets push it to the stack for isr80h_handler
    push eax
    call isr80h_handler ; this will return an interger value to eax
    mov dword[tmp_res], eax
    add esp, 8 ; this will set the esp to where it was before, so we can popad the correct values

    ; Restore general purpose registers for user land
    popad
    mov eax, [tmp_res]
    iretd  

section .data
    ; Inside here is stored the return result from isr80h_handler
    tmp_res: dd 0

    ; Some confusions?!?!?!?!?!?!?!?!?!?!!?
    %macro interrupt_array_entry 1
        dd int%1
    %endmacro

    ; This is the table of pointers to int
    interrupt_pointer_table:
        %assign i 0
        %rep 512
            interrupt_array_entry i
        %assign i i+1
        %endrep
