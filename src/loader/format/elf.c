#include "elf.h"

// Note: returns the entry address as a void pointer
void* elf_get_entry_ptr(struct elf_header* elf_header) {
    return (void*) elf_header->e_entry;
}

// Note: returns the entry address
uint32_t elf_get_entry(struct elf_header* elf_header) {
    return elf_header->e_entry;
}

