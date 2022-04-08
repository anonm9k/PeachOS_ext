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

const char elf_signature[] = {0x7f, 'E', 'L', 'F'};

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

// Note: Gets actual physical address of where the executable part of the program is loaded
void* elf_phdr_phys_address(struct elf_file* file, struct elf32_phdr* phdr)
{
    return elf_memory(file)+phdr->p_offset;
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
    return (elf_valid_signature(header) && elf_valid_class(header) && elf_valid_encoding(header) && elf_has_program_header(header)) ? PEACHOS_ALL_OK : -EINFORMAT;
} 

// Note: Resolves program addresses from program header type of PT_LAOD
int elf_process_phdr_pt_load(struct elf_file* elf_file, struct elf32_phdr* phdr) {

    // Check: if virtual_base_address is set or not, and look for the lowest virtual base address
    // Here: we get the correct virtual_base_address, and also set physical_base_address
    if (elf_file->virtual_base_address >= (void*) phdr->p_vaddr || elf_file->virtual_base_address == 0x00) {
        elf_file->virtual_base_address = (void*) phdr->p_paddr;
        elf_file->physical_base_address = elf_memory(elf_file) + phdr->p_offset;
    }

    // Here: we get the correct virtual_end_address, and also set physical_end_address
    unsigned int end_virtual_address = phdr->p_vaddr + phdr->p_filesz;
    if (elf_file->virtual_end_address <= (void*)(end_virtual_address) || elf_file->virtual_end_address == 0x00)
    {
        elf_file->virtual_end_address = (void*) end_virtual_address;
        elf_file->physical_end_address = elf_memory(elf_file)+phdr->p_offset+phdr->p_filesz;
    }

    return 0;
}

// Note: We only allow program headers of type PT_LOAD
int elf_process_pheader(struct elf_file* elf_file, struct elf32_phdr* phdr) {
    int res = 0;

    switch(phdr->p_type) {
        case PT_LOAD:
            res = elf_process_phdr_pt_load(elf_file, phdr);
        break;
    }

    return res;
}

// Note: Goes through program header to find program addresses
int elf_process_pheaders(struct elf_file* elf_file) {
    int res = 0;
    struct elf_header* header = elf_header(elf_file);

    // Here: we go through all the program headers and find the right header
    for (int i = 0; i < header->e_phnum; i++) {
        // Here: we get program header by index
        struct elf32_phdr* phdr = elf_program_header(header, i);
        // Here: we inspect the program header (goal is to accept a single pheader with the lowest addresses)
        res = elf_process_pheader(elf_file, phdr);
        if (res < 0) {
            break;
        }
    }

    out:
        return res;
}

// Note: Get the actual program from the elf file
int elf_process_loaded(struct elf_file* elf_file) {
    int res = 0;
    struct elf_header* header = elf_header(elf_file);

    // Here: we check if we support this elf by checking  bheader information
    res = elf_validate_loaded(header);
    if (res < 0) {
        goto out;
    }

    // Here: we resolve the program pointers from the program header
    res = elf_process_pheaders(elf_file);
    if (res < 0) {
        goto out;
    }

    out:
        return res;
}

// Note: Responsible for loading the elf program into memory, and resolving elf_file structure
int elf_load(const char* filename, struct elf_file** file_out) {

    struct elf_file* elf_file = kzalloc(sizeof(struct elf_file));

    int fd = 0;
    int res = fopen(filename, "r");
    // Check: if opening the file is successful
    if (res < 0) {
        goto out;
    }

    fd = res;
    struct file_stat stat;
    // Check: if fstat is successful
    res = fstat(fd, &stat);
    if (res < 0) {
        goto out;
    }

    // Here: we resolve void* elf_memory
    elf_file->elf_memory = kzalloc(stat.filesize);

    // Here: we read the whole file
    res = fread(elf_file->elf_memory, stat.filesize, 1, fd);
    // Check: fread
    if (res < 0) {
        goto out;
    }

    // Here: Here we load the actual executable program
    res = elf_process_loaded(elf_file);
    if (res < 0) {
        goto out;
    }

    // Here: we give it to em
    *file_out = elf_file;

    out:
        fclose(fd);
        return res;
}

void elf_close(struct elf_file* file) {
    if (!file)
        return;

    kfree(file->elf_memory);
    kfree(file);
}