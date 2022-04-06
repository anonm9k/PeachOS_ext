#include <stdbool.h>
#include "elfloader.h"
#include "fs/file.h"
#include "status.h"
#include "memory/memory.h"
#include "memory/heap/kheap.h"
#include "string/string.h"
#include "memory/paging/paging.h"
#include "kernel.h"
#include "config.h"

const char* elf_signature[] = {0x7f, 'E', 'L', 'F'};

// Note: checks the magic number
static bool elf_valid_signature(void* buffer) {
    return memcmp(buffer, (void*) elf_signature, sizeof(elf_signature)) == 0;
}

// Note: checks the class byte in the e_ident array for accepatable classes
static bool elf_valid_class(struct elf_header* header) {
    // Note: we only accept 32-bit code (in protected mode)
    return header->e_ident[EI_CLASS] == ELFCLASSNONE || header->e_ident[EI_CLASS] == ELFCLASS32;
}

// Note: checks the encoding byte in the e_ident array for accepatable encoding type
static bool elf_valid_encoding(struct elf_header* header)
{
    return header->e_ident[EI_DATA] == ELFDATANONE || header->e_ident[EI_DATA] == ELFDATA2LSB;
}

// Note: checks e_type and a valid e_entry address
static bool elf_is_executable(struct elf_header* header)
{   // Note: e_type should be executable and entry address should point to where executable programs start  
    // Note: we also check if the entry address is valid
    return header->e_type == ET_EXEC && header->e_entry >= PEACHOS_PROGRAM_VIRTUAL_ADDRESS;
}

// Note: checks e_phoff for number of headers  
static bool elf_has_program_header(struct elf_header* header)
{
    return header->e_phoff != 0;
}

// Note: returns elf_memory
void* elf_memory(struct elf_file* file)
{
    return file->elf_memory;
}

// Note: returns elf_header
struct elf_header* elf_header(struct elf_file* file)
{   // Note: because the header is at the beginning of the file and we return it as elf header structure
    return file->elf_memory;
}

// Note: returns the section header 
struct elf32_shdr* elf_sheader(struct elf_header* header)
{
    return (struct elf32_shdr*)((int)header+header->e_shoff);
}

// Note: returns program header (if exists)
struct elf32_phdr* elf_pheader(struct elf_header* header)
{
    if(header->e_phoff == 0)
    {
        return 0;
    }

    return (struct elf32_phdr*)((int)header + header->e_phoff);
}

// Note: returns program header based on index (because multiple program headers can exist)
struct elf32_phdr* elf_program_header(struct elf_header* header, int index)
{
    return &elf_pheader(header)[index];
}

// Note: returns section header based on index (because multiple section headers can exist)
struct elf32_shdr* elf_section(struct elf_header* header, int index)
{
    return &elf_sheader(header)[index];
}

// Note: finds string section and gets the address
char* elf_str_table(struct elf_header* header)
{
    return (char*) header + elf_section(header, header->e_shstrndx)->sh_offset;
}

// Note: returns virtual_base_address
void* elf_virtual_base(struct elf_file* file)
{
    return file->virtual_base_address;
}

// Note: returns virtual_end_address
void* elf_virtual_end(struct elf_file* file)
{
    return file->virtual_end_address;
}

// Note: returns physical_base_address
void* elf_phys_base(struct elf_file* file)
{
    return file->physical_base_address;
}

// Note: returns physical_end_address
void* elf_phys_end(struct elf_file* file)
{
    return file->physical_end_address;
}

// Note: checks magic number, class, encoding, if file header exists
int elf_validate_loaded(struct elf_header* header)
{
    return (elf_valid_signature(header) && elf_valid_class(header) && elf_valid_encoding(header) && elf_has_program_header(header)) ? PEACHOS_ALL_OK : -EINVARG;
} 