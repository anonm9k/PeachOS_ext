#include "task.h"
#include "status.h"
#include "kernel.h"
#include "memory/heap/kheap.h"
#include "memory/memory.h"
#include "memory/paging/paging.h"
#include "idt/idt.h"
#include "process.h"
#include "string/string.h"
#include "loader/formats/elfloader.h"


// The current task that is running
struct task* current_task = 0;

// Task linked list 
struct task* task_tail = 0;
struct task* task_head = 0;

int task_init(struct task* task, struct process* process);

// Note: just gets the current task running
struct task* task_current() {
    return current_task;
}

// Creates and initializes a task structure from a process
// Note: resolves task: next, prev
struct task* task_new(struct process* process) {

    int res = 0;
    struct task* task = kzalloc(sizeof(struct task));
    // Check: memory
    if (!task) {
        res = -ENOMEM;
        goto out;
    };

    // Note: we initialize the task structure
    res = task_init(task, process);

    // Check: if able to initialize a new task
    if (res != PEACHOS_ALL_OK) {
        goto out;
    }

    // Check: is this the first task in the double linked list
    if (task_head == 0) {
        task_head = task;
        task_tail = task;
        current_task = task;
        goto out;
    }

    // Here: we implement the linked list
    task_tail->next = task;
    task->prev = task_tail;
    task_tail = task;

out:
    if (ISERR(res)) {
        task_free(task);
        return ERROR(res);
    }

    return task;
}

struct task* task_get_next() {

    if (!current_task->next) {
        return task_head;
    }

    return current_task->next;
}

// Note: removes the task from linked list
static void task_list_remove(struct task* task) {
    if (task->prev) {
        task->prev->next = task->next;
    }

    if (task == task_head) {
        task_head = task->next;
    }

    if (task == task_tail) {
        task_tail = task->prev;
    }

    if (task == current_task) {
        current_task = task_get_next();
    }
}

// Removes the task from the system completely
int task_free(struct task* task) {
    paging_free_4gb(task->page_directory);
    task_list_remove(task);
    kfree(task);

    return 0;
}

// Note: this will switch the directory to the tasks directory
int task_switch(struct task* task) {
    current_task = task;
    paging_switch(task->page_directory);
    return 0;
}

// Note: copy the int frame registers to our task registers
void task_save_state(struct task* task, struct interrupt_frame* frame) {
    task->registers.ip = frame->ip;
    task->registers.cs = frame->cs;
    task->registers.flags = frame->flags;
    task->registers.esp = frame->esp;
    task->registers.ss = frame->ss;
    task->registers.eax = frame->eax;
    task->registers.ebp = frame->ebp;
    task->registers.ebx = frame->ebx;
    task->registers.ecx = frame->ecx;
    task->registers.edi = frame->edi;
    task->registers.edx = frame->edx;
    task->registers.esi = frame->esi;
}

int copy_string_from_task(struct task* task, void* virtual, void* phys, int max) {
    // Check: should not exceed a page
    if (max >= PAGING_PAGE_SIZE) {
        return -EINVARG;
    }

    int res = 0;
    char* tmp = kzalloc(max);
    // Check: if enough memory
    if (!tmp) {
        res = -ENOMEM;
        goto out;
    }

    uint32_t* task_directory = task->page_directory->directory_entry;
    // Here: we save it so we can undo it
    uint32_t old_entry = paging_get(task_directory, tmp);

    /* Here: we map it so that the virtual address will map to the same physical address (e.g. virt: 0x1234 will map
    to physical address 0x1234) */
    paging_map(task->page_directory, tmp, tmp, PAGING_IS_WRITEABLE | PAGING_IS_PRESENT | PAGING_ACCESS_FROM_ALL);
    // Here: we switch to task
    paging_switch(task->page_directory);

    strncpy(tmp, virtual, max);
 
    kernel_page();

    // Here: we change it back to what it was
    res = paging_set(task_directory, tmp, old_entry);

    if (res < 0) {
        res = -EIO;
        goto out_free;
    }

    strncpy(phys, tmp, max);
    
    out_free:
        kfree(tmp);
    out:
        return res;
}

/* Note: this will save the registers of the task that invoked the interrupt, and it will save it to the tasks 
structures registers structure*/
void task_current_save_state(struct interrupt_frame* frame) {

    // Check: if there is no task then who tf gave us this interrupt
    if (!current_task) {
        panic("\nNo current task is running");
    }

    struct task* task = current_task;
    task_save_state(task, frame);
}

/* Note: when this function is called(from the kernel), this function changes the kernel directory to the current tasks
directory*/ 
int task_page() {
    // We go to the user data segment
    user_registers(); 
    task_switch(current_task);
    return 0;
}

// Note: will switch to the directory of the given task
int task_page_task(struct task* task) {
    user_registers();
    paging_switch(task->page_directory);
    return 0;
}

// Note: yes
void task_run_first_ever_task() {
    if (!current_task) {
        panic("task_run_first_ever_task: no current tasks exists");
    }

    // Here: we switch the directiry from kernels directory to this tasks directory
    task_switch(task_head); // because it is the first task in the list

    // Here: we change all register to the tasks registers, which also includes all the flags
    task_return(&task_head->registers); 
}

// Note: initialise a task structure
// Note: resolves the page directory, register: {ip, ss, cs, esp}
int task_init(struct task* task, struct process* process) {
    memset(task, 0, sizeof(struct task));
    // Map entire 4GB address space to it
    // Note: we didnt set PAGING_IS_WRITEABLE so its read only address space
    task->page_directory = paging_new_4gb(PAGING_IS_PRESENT | PAGING_ACCESS_FROM_ALL);

    // Check: if we were able to create a page directory
    if (!task->page_directory) {
        return -EIO;
    }

    // Here: we initilize the virtual addresses (all virtual! this is nuts!)
    task->registers.ip = PEACHOS_PROGRAM_VIRTUAL_ADDRESS;
    if (process->filetype == PROCESS_FILETYPE_ELF) {
        task->registers.ip = elf_header(process->elf_file)->e_entry;
    }
    task->registers.ss = USER_DATA_SEGMENT;
    task->registers.cs = USER_CODE_SEGMENT;
    task->registers.esp = PEACHOS_PROGRAM_VIRTUAL_STACK_ADDRESS_START;

    // Here: we resolve the process
    task->process = process;
    return 0;
}

void* task_get_stack_item(struct task* task, int index) {
    void* result = 0;

    uint32_t* sp_ptr = (uint32_t*) task->registers.esp;

    task_page_task(task);

    result = (void*) sp_ptr[index];

    kernel_page();
    return result;
}
