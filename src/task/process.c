#include <stdbool.h>
#include "kernel.h"
#include "process.h"
#include "config.h"
#include "status.h"
#include "memory/memory.h"
#include "task/task.h"
#include "memory/heap/kheap.h"
#include "fs/file.h"
#include "string/string.h"
#include "memory/paging/paging.h"
#include "loader/formats/elfloader.h"

// Current process that is running
struct process* current_process = 0;

static struct process* processes[PEACHOS_MAX_PROCESSES] = {};

// Note: just memset the structure
static void process_init(struct process* process) {
    memset(process, 0, sizeof(struct process));
}

// Note: returns current_process variable
struct process* process_current() {
    return current_process;
}

// Note: will return the process by index from processes list
struct process* process_get(int process_id) {
    if (process_id < 0 || process_id >= PEACHOS_MAX_PROCESSES) {
        return NULL;
    }
    return processes[process_id];
}

// Note: switches from current process to provided process
int process_switch(struct process* process) {
    // ToDo: in a better implementation we should sabe the current process state
    current_process = process;
    return 0;
}

// Note: go thrugh given process mem allocations array to find empty slot
static int process_find_free_allocation_index(struct process* process) {
    int res = -ENOMEM;
    for (int i = 0; i < PEACHOS_MAX_PROGRAM_ALLOCATIONS; i++) {
        if (process->allocations[i].ptr == 0) {
            res = i;
            break;
        }
    }

    return res;
}

// Note: allocates heap for given process, and resolves void* allocations of that process
void* process_malloc(struct process* process, size_t size) {
    void* ptr = kzalloc(size);
    if (!ptr) {
        return 0;
    }

    int index = process_find_free_allocation_index(process);
    if (index < 0) {
        goto out_err;
    }

    // MMapping the addresses so it is addressable from any ring level (PAGING_ACCESS_FROM_ALL)
    int res = paging_map_to(process->task->page_directory, ptr, ptr, paging_align_address(ptr+size), PAGING_IS_WRITEABLE | PAGING_ACCESS_FROM_ALL | PAGING_IS_PRESENT);
    if (res < 0) {
        goto out_err;
    }
    
    process->allocations[index].ptr = ptr;
    process->allocations[index].size = size;

    return ptr;

    out_err:
        if (ptr) {
            kfree(ptr);
        }
        return 0;
}   

// Note: Given a process, checks if the ptr exists in its allocations array
static bool process_is_process_pointer(struct process* process, void* ptr) {
    for (int i = 0; i < PEACHOS_MAX_PROGRAM_ALLOCATIONS; i++) {
        if (process->allocations[i].ptr == ptr) {
            return true;
        }
    }
    return false;
}

// Note: given an address, get the process_allocation structure from the array
static struct process_allocation* process_get_allocation_by_addr(struct process* process, void* addr) {
    for (int i = 0; i < PEACHOS_MAX_PROGRAM_ALLOCATIONS; i++) {
        if (process->allocations[i].ptr == addr)
            return &process->allocations[i];
    }

    return 0;
};

// Note: removes the pointer from process allocations array
void process_allocation_unjoin(struct process* process, void* ptr) {
    for (int i = 0; i < PEACHOS_MAX_PROGRAM_ALLOCATIONS; i++) {
        if (process->allocations[i].ptr == ptr) {
            process->allocations[i].ptr = 0x00;
            process->allocations[i].size = 0;
        }
    }
}

// Note: goes though all allocations and frees them
int process_terminate_allocations(struct process* process) {
    for (int i = 0; i < PEACHOS_MAX_PROGRAM_ALLOCATIONS; i++) {
        process_free(process, process->allocations[i].ptr);
    }
    return 0;
}

// Note: frees up binary of a process program
int process_free_binary_data(struct process* process) {
    kfree(process->ptr);
    return 0;
}

// Note: closes elf file of the process
int process_free_elf_data(struct process* process) {
    elf_close(process->elf_file);
    return 0;
}

// Note: fress up program data
int process_free_program_data(struct process* process) {
    int res = 0;
    switch(process->filetype) {
        case PROCESS_FILETYPE_BINARY:
            res = process_free_binary_data(process);
        break;

        case PROCESS_FILETYPE_ELF:
            res = process_free_elf_data(process);
        break;

        default:
            res = -EINVARG;
    }
    return res;
}

// Note: switches to a process in the processes
void process_switch_to_any() {
    for (int i = 0; i < PEACHOS_MAX_PROCESSES; i++) {
        if (processes[i]) {
            process_switch(processes[i]);
            return;
        }
    }
    panic("No processes to switch to!\n");
}

// Note: removes the process from processes list
static void process_unlink(struct process* process) {
    processes[process->id] = 0x00;

    if (current_process == process) {
        process_switch_to_any();
    }
}



// Note: terminates a process
int process_terminate(struct process* process) {
    int res = 0;
    res = process_terminate_allocations(process);
    if (res < 0) {
        goto out;
    }

    res = process_free_program_data(process);
    if (res < 0) {
        goto out;
    }

    //Here: we free the process stack
    kfree(process->stack);

    task_free(process->task);

    process_unlink(process);
    
    out:
        return res;
}

// Note: gets arguments from process arguments structure
void process_get_arguments(struct process* process, int* argc, char*** argv) {
    *argc = process->arguments.argc;
    *argv = process->arguments.argv;
}

// Note: goes through command_argument linked list and calculates
int process_count_command_arguments(struct command_argument* root_argument)
{
    struct command_argument* current = root_argument;
    int i = 0;
    while(current)
    {
        i++;
        current = current->next;
    }

    return i;
}

int process_inject_arguments(struct process* process, struct command_argument* root_argument) {
    int res = 0;
    struct command_argument* current = root_argument;
    int i = 0;
    int argc = process_count_command_arguments(root_argument);
    if (argc == 0)
    {
        res = -EIO;
        goto out;
    }

    char **argv = process_malloc(process, sizeof(const char*) * argc);
    if (!argv)
    {
        res = -ENOMEM;
        goto out;
    }


    while(current)
    {
        char* argument_str = process_malloc(process, sizeof(current->argument));
        if (!argument_str)
        {
            res = -ENOMEM;
            goto out;
        }

        strncpy(argument_str, current->argument, sizeof(current->argument));
        argv[i] = argument_str;
        current = current->next;
        i++;
    }

    process->arguments.argc = argc;
    process->arguments.argv = argv;
out:
    return res;
}

// Note: frees up the heap memory
void process_free(struct process* process, void* ptr) {
    // Here: unlink the pages from process for the given address
    struct process_allocation* allocation = process_get_allocation_by_addr(process, ptr);
    // Check: if pointer is valid
    if (!allocation) {
        return;
    }

    int res = paging_map_to(process->task->page_directory, allocation->ptr, allocation->ptr, paging_align_address(allocation->ptr+allocation->size), 0x00);
    if (res < 0) {
        return;
    }

    // Here: we remove it from allocations array
    process_allocation_unjoin(process, ptr);

    // Here: we kfree the heap memory
    kfree(ptr);
}

// Note: this function will load the binary into memory
// Note: this will resolve process: filesize, *ptr, filetype
static int process_load_binary(const char* filename, struct process* process) {
    void* program_data_ptr = 0x00;
    int res = 0;
    // Here: we open the file in read mode
    int fd = fopen(filename, "r");

    // Check: if the filename is valid
    if (!fd) {
        res = -EIO;
        goto out;
    }

    // Here: we try to get the program size
    struct file_stat stat; 
    res = fstat(fd, &stat);
    if (res != PEACHOS_ALL_OK) {
        goto out;
    }

    // Here: we are creating memory space for our program
    program_data_ptr = kzalloc(stat.filesize);

    // Check: if enough memory
    if (!program_data_ptr) {
        res = -ENOMEM;
        goto out;
    }

    // Here: we read the program from disk to our allocated program space
    // Check: if able to read
    if (fread(program_data_ptr, stat.filesize, 1, fd) != 1) {
        res = -EIO;
        goto out;
    }

    // Here: we resolve: filesize, *ptr, filetype
    process->size = stat.filesize;
    process->ptr = program_data_ptr;
    process->filetype = PROCESS_FILETYPE_BINARY;

    out:
        if (res < 0){
            if (program_data_ptr) {
                kfree(program_data_ptr);
            }
        }
        // Closing file
        fclose(fd);
        return res;
}

// Note: responsible for creating process from elf file
int process_load_elf(const char* filename, struct process* process) {
    int res = 0;
    struct elf_file* elf_file = 0;
    res = elf_load(filename, &elf_file);
    if (res < 0) {
        goto out;
    }

    process->filetype = PROCESS_FILETYPE_ELF;
    process->elf_file = elf_file;

    out:
        return res;
}

// Note: responsible for looking at the data type: ELF, Raw Binary etc and loading it
static int process_load_data(const char* filename, struct process* process) {
    int res = 0;
    // Check: if it is valid elf
    res = process_load_elf(filename, process);
    if (res == -EINFORMAT) {
        res = process_load_binary(filename, process);
    }
    return res;
}

// Note: maps the process to virtual address
int process_map_binary(struct process* process) {
    int res = 0;
    res = paging_map_to(process->task->page_directory, (void*) PEACHOS_PROGRAM_VIRTUAL_ADDRESS, process->ptr, paging_align_address(process->ptr + process->size), PAGING_IS_PRESENT | PAGING_IS_WRITEABLE | PAGING_ACCESS_FROM_ALL);
    return res;
}

// Note: maps the elf program to virtual address
// Note: for now it only maps a single elf segment
int process_map_elf(struct process* process) {
    int res = 0;

    struct elf_file* elf_file = process->elf_file;
    struct elf_header* header = elf_header(elf_file);
    struct elf32_phdr* phdrs = elf_pheader(header);

    for (int i = 0; i < header->e_phnum; i++) {
        struct elf32_phdr* phdr = &phdrs[i];
        void* phdr_phys_address = elf_phdr_phys_address(elf_file, phdr);
        int flags = PAGING_IS_PRESENT | PAGING_ACCESS_FROM_ALL;

        if (phdr->p_flags & PF_W) {
            flags |= PAGING_IS_WRITEABLE;
        }
        res = paging_map_to(process->task->page_directory, paging_align_to_lower_page((void*) phdr->p_vaddr), paging_align_to_lower_page(phdr_phys_address), paging_align_address(phdr_phys_address + phdr->p_memsz), flags);

        if (ISERR(res)) {
            break;
        }
    }

    return res;
}

// Note: responsible for looking at the data type: ELF, Raw Binary etc and memory map it
// Note: this function expects an initialized process
int process_map_memory(struct process* process) {
    int res = 0;

    switch(process->filetype) {
        case PROCESS_FILETYPE_BINARY:
            res = process_map_binary(process);
            break;
        
        case PROCESS_FILETYPE_ELF:
            res = process_map_elf(process);
            break;

        default:
            panic("\n Invalid filetype");
    }

    if (res < 0) {
        goto out;
    }
    
    // Note: here we map the stack
    // Attention! It is STACK_ADDRESS_END because stack grows downwards (I was wrong)
    paging_map_to(process->task->page_directory, (void*) PEACHOS_PROGRAM_VIRTUAL_STACK_ADDRESS_END, process->stack, paging_align_address(process->stack + PEACHOS_USER_PROGRAM_STACK_SIZE), PAGING_IS_PRESENT | PAGING_IS_WRITEABLE | PAGING_ACCESS_FROM_ALL);
    out:
        return res;
}

// Note: goes through the processes list
int process_get_free_slot() {
    for (int i = 0; i < PEACHOS_MAX_PROCESSES; i++) {
        if (processes[i] == 0)  
            return i;
    }

    return -EISTKN;
}

// Note: first checks for available slots, then returns PID
int process_load(const char* filename, struct process** process) {
    int res = 0;
    // Here: we get free slot from list
    int process_slot = process_get_free_slot();
    if (process_slot < 0) {
        res = -EISTKN;
        goto out;
    }

    // Here: we load it
    res = process_load_for_slot(filename, process, process_slot);

    out:
        return res;
}

// Note: loads a process, and switches to that process
int process_load_switch(const char* filename, struct process** process) {
    int res = process_load(filename, process);
    // Check: if successfully loaded a process
    if (res == 0) {
        process_switch(*process);
    }

    return res;
}

// Note: returns PID
// Note: this will resolve process: *stack, filename, id, task
int process_load_for_slot(const char* filename, struct process** process, int process_slot) {
    int res = 0;
    struct task* task = 0;
    struct process* _process;
    void* program_stack_ptr = 0;

    // Check: if the index is valid
    if (process_get(process_slot) != PEACHOS_ALL_OK) {
        res = -EISTKN;
        goto out;
    }

    _process = kzalloc(sizeof(struct process));
    // Check: if enough memory
    if (!_process) {
        res = -ENOMEM;
        goto out;
    }

    // Here: we initialize the process
    process_init(_process);

    res = process_load_data(filename, _process);

    // Check: if there is a problem loading data
    if (res < 0) {
        goto out;
    }

    // Here: we create stack for the program
    program_stack_ptr = kzalloc(PEACHOS_USER_PROGRAM_STACK_SIZE);
                                                                // Memory leak?
    // Check: if enough memory
    if (!program_stack_ptr) {
        res = -ENOMEM;
        goto out;
    }

    // Here: we resolve filename, *stack, id
    strncpy(_process->filename, filename, sizeof(_process->filename));
    _process->stack = program_stack_ptr;
    _process->id = process_slot;

    // Here: we create/initialize a task
    task = task_new(_process);

    // Check: if able to create new task
    if (ERROR_I(task) == 0) {
        res = ERROR_I(task);
        goto out;
    }

    // Here: we resolve process: task
    _process->task = task;

    // Here: we map the process in memory
    res = process_map_memory(_process);
    
    // Check: if mapped succesfully
    if (res < 0) {
        goto out;
    }

    // Here: we give the process (so: _process can be found in the processes list)
    *process = _process;

    // Here: we add the process to the list
    processes[process_slot] = _process;

    out:
        if (ISERR(res)) {
            if (_process && _process->task) {
                task_free(_process->task);
            }
            // ToDo: Free the process data
        }
        return res;
}