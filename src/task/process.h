#ifndef PROCESS_H
#define PROCESS_H
#include <stdint.h>
#include "config.h"
#include "task.h"

struct process {
    uint16_t id;
    char filename[PEACHOS_MAX_PATH];
    struct task* task;
    void* allocations[PEACHOS_MAX_PROGRAM_ALLOCATIONS]; // fail-safe, mem-leak
    void* ptr; // physical address to process memory, once the process is loaded
    void* stack; // physical address to process stack, once the process is loaded
    uint32_t size; // Size of the data pointed by ptr
    struct keyboard_buffer {
        char buffer[PEACHOS_KEYBOARD_BUFFER_SIZE];
        int tail;
        int head;
    } keyboard;
};

int process_load_switch(const char* filename, struct process** process);
int process_load_for_slot(const char* filename, struct process** process, int process_slot);
int process_load(const char* filename, struct process** process);
struct process* process_current();
int process_switch(struct process* process);


#endif