#include "idt.h"
#include "config.h"
#include "kernel.h"
#include "memory/memory.h"
#include "io/io.h"
#include "task/task.h"
#include "task/process.h"
#include "status.h"


struct idt_desc idt_descriptors[PEACHOS_TOTAL_INTERRUPTS];
struct idtr_desc idtr_descriptor;

// Note: this is created by idt.asm (extern)
extern void* interrupt_pointer_table[PEACHOS_TOTAL_INTERRUPTS];

// Array of function pointers
static INTERRUPT_CALLBACK_FUNCTION interrupt_callbacks[PEACHOS_TOTAL_INTERRUPTS];
static ISR80H_COMMAND isr80h_commands[PEACHOS_MAX_ISR80H_COMMANDS];

extern void idt_load(struct idtr_desc* ptr);
extern void int21h();
extern void no_interrupt();
extern void isr80h_wrapper();

void no_interrupt_handler()
{
    outb(0x20, 0x20);
}

void interrupt_handler(int interrupt, struct interrupt_frame* frame) {
    kernel_page(); // Switch to kernel page
    // Check: if interrupt handler function exists
    if (interrupt_callbacks[interrupt] != 0) {
        task_current_save_state(frame); // Save the current tasks registers 
        interrupt_callbacks[interrupt](frame);
    }
    task_page(); // Switch back to task page
    outb(0x20, 0x20); // PIC requires acknowledgment, so we give it to em
}

void idt_zero()
{
    print("Divide by zero error\n");
}

void idt_set(int interrupt_no, void* address)
{
    struct idt_desc* desc = &idt_descriptors[interrupt_no];
    desc->offset_1 = (uint32_t) address & 0x0000ffff;
    desc->selector = KERNEL_CODE_SELECTOR;
    desc->zero = 0x00;
    desc->type_attr = 0xEE;
    desc->offset_2 = (uint32_t) address >> 16;
}

// Note: for now if any execption is generated by the process we terminate it 
void idt_handle_execption() {
    process_terminate(task_current()->process);
    task_next();
}

// Note: on every clock we switch to the next task (multitasking)
int c = 0; // Just putting some delay before starting context switching
void idt_clock() {
    outb(0x20, 0x20); // We have to acknowledge it here because task_next() never returns
    if (c < 10) {
        c++;
    }
    else {
        task_next();
    }
}

// Note: initializes descriptor
void idt_init()
{
    memset(idt_descriptors, 0, sizeof(idt_descriptors));
    idtr_descriptor.limit = sizeof(idt_descriptors) -1;
    idtr_descriptor.base = (uint32_t) idt_descriptors;

    for (int i = 0; i < PEACHOS_TOTAL_INTERRUPTS; i++)
    {
        idt_set(i, interrupt_pointer_table[i]);
    }

    // Here: we set the ISR80H  interrupt
    idt_set(0x80, isr80h_wrapper);

    // Here: we map all exepctions
    for (int i = 0; i < 0x20; i++) {
        idt_register_interrupt_callback(i, idt_handle_execption);
    }

    // Here: we set the clock
    idt_register_interrupt_callback(0x20, idt_clock);

    // Load the interrupt descriptor table
    idt_load(&idtr_descriptor);
}

// Note: registers the function into the function pointer array
int idt_register_interrupt_callback(int interrupt, INTERRUPT_CALLBACK_FUNCTION interrupt_callback) {
    // Check: for bounds
    if (interrupt < 0 || interrupt >= PEACHOS_TOTAL_INTERRUPTS) {
        return -EINVARG;
    }

    interrupt_callbacks[interrupt] = interrupt_callback;

    return 0;
}

// Note: saves functions in the commands list
void isr80h_register_command(int command_id, ISR80H_COMMAND command) {
    // Check: if the number is out of bound
    if (command_id < 0 || command_id >= PEACHOS_MAX_ISR80H_COMMANDS) {
        panic("\nThe command is out of bound");
    }

    // Check: if the command is already taken
    if (isr80h_commands[command_id]) {
        panic("\nYou're attempting to overwrite an existing command");
    }

    // Here: we go to the index, and pass the pointer of our command function
    isr80h_commands[command_id] = command;
}   

// Note: takes a command index, and passes it to the commands lists appropriate function
void* isr80h_handle_command(int command, struct interrupt_frame* frame) {
    void* result = 0;
    // Check: if the number is out of bound
    if (command < 0 || command >= PEACHOS_MAX_ISR80H_COMMANDS) {
        return 0;
    }

    ISR80H_COMMAND command_func = isr80h_commands[command];
    // Check: if the command doesnt exist
    if (!command_func) {
        panic("\n Command doesnt exist");
        return 0;
    }

    result = command_func(frame);

    return result;
}

// Note: switches to kernel mode, and serves the interrupt
void* isr80h_handler(int command, struct interrupt_frame* frame) {
    void* res = 0;
    kernel_page(); // we switch to using kernel directory, and we set the kernel segment registers
    task_current_save_state(frame);
    res = isr80h_handle_command(command, frame);
    task_page(); // we switch to using task directory, and we set the user segment registers
    return res;
}