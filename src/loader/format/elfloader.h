#ifndef ELFLOADER_H
#define ELFLOADER_H

#include <stdint.h>
#include <stddef.h>
#include "elf.h"
#include "config.h"

struct elf_file {
    char filename[PEACHOS_MAX_PATH];
    int in_memory_size;
    void* elf_memory; // Physical address of where this elf file is loaded in memory
    void* virtual_base_address; // Virtual address of the first loadable section in memory
    void* virtual_end_address; // Ending virtual address 
    void* physical_base_address; // Physical address of the first loadable section in memory
    void* physical_end_address; // Ending physical address
};

#endif