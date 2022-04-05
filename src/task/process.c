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

// Note: this function will load the binary into memory
// Note: this will resolve process: filesize, *ptr
static int process_load_binary(const char* filename, struct process* process) {
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
    void* program_data_ptr = kzalloc(stat.filesize);

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

    // Here: we resolve: filesize, *ptr
    process->size = stat.filesize;
    process->ptr = program_data_ptr;

    out:
        // Closing file
        fclose(fd);
        return res;
}

// Note: responsible for looking at the data type: ELF, Raw Binary etc and loading it
static int process_load_data(const char* filename, struct process* process) {
    int res = 0;
    res = process_load_binary(filename, process);
    return res;
}

// Note: maps the process to virtual address
int process_map_binary(struct process* process) {
    int res = 0;
    paging_map_to(process->task->page_directory, (void*) PEACHOS_PROGRAM_VIRTUAL_ADDRESS, process->ptr, paging_align_address(process->ptr + process->size), PAGING_IS_PRESENT | PAGING_IS_WRITEABLE | PAGING_ACCESS_FROM_ALL);
    return res;
}

// Note: responsible for looking at the data type: ELF, Raw Binary etc and memory map it
// Note: this function expects an initialized process
int process_map_memory(struct process* process) {
    int res = 0;
    res = process_map_binary(process);
    if (res < 0) {
        goto out;
    }
    
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