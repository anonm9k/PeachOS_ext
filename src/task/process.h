#ifndef PROCESS_H
#define PROCESS_H
#include <stdint.h>
#include <stdbool.h>
#include "config.h"
#include "task.h"

#define PROCESS_FILETYPE_ELF    0
#define PROCESS_FILETYPE_BINARY 1

typedef unsigned char PROCESS_FILETYPE;

struct process {
    uint16_t id;

    char filename[PEACHOS_MAX_PATH];

    struct task* task;
    
    // fail-safe, mem-leak
    void* allocations[PEACHOS_MAX_PROGRAM_ALLOCATIONS]; 

    // ELF? bin?
    PROCESS_FILETYPE filetype; 

    union {
        void* ptr; // physical address to process memory, once the process is loaded
        struct elf_file* elf_file;
    };

    // physical address to process stack, once the process is loaded
    void* stack; 

    // Size of the data pointed by ptr
    uint32_t size;

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

void* process_malloc(struct process* process, size_t size);
void process_free(struct process* process, void* ptr);


#endif