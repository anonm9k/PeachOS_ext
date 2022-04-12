#include "paging.h"
#include "memory/heap/kheap.h"
#include "status.h"
void paging_load_directory(uint32_t* directory);

// Note: we keep track of which directory we are in. Because we will be jumping from dir to dir, when we task switch, also when we enter kernel mode and shit
static uint32_t* current_directory = 0;

// Note: creates and initializes a whole directory and puts it into the structure paging_4gb_chunk
struct paging_4gb_chunk* paging_new_4gb(uint8_t flags)
{
    // Layer 1: creating directory
    uint32_t* directory = kzalloc(sizeof(uint32_t) * PAGING_TOTAL_ENTRIES_PER_TABLE);

    int offset = 0;
    for (int i = 0; i < PAGING_TOTAL_ENTRIES_PER_TABLE; i++)
    {   // Layer 2: creating tables on each loop
        uint32_t* entry = kzalloc(sizeof(uint32_t) * PAGING_TOTAL_ENTRIES_PER_TABLE);
        // Here: we go through each items in the table
        for (int b = 0; b < PAGING_TOTAL_ENTRIES_PER_TABLE; b++)
        {   // Note: each has structure of a page table entry
            entry[b] = (offset + (b * PAGING_PAGE_SIZE)) | flags;
            /*
            Structure of Page Table entries:
            31                               12  11   9   8    7    6   5    4    3      2     1    0
            -------------------------------------------------------------------------------------------
            |   Bits 31-12 of address(20-bits)  |  AVL  | G | PAT | D | A | PCD | PWT | U/S | R/W | p |
            -------------------------------------------------------------------------------------------
            P:      Present                 D:      Dirty
            R/W:    Read/Write              G:      Global
            U/S:    User/ Supervisor        AVL:    Available
            PWT:    Write-Through           PAT:    Page Attribute Table
            PCD:    Cache Disable           
            A:      Accessed 
            */
        }
        offset += (PAGING_TOTAL_ENTRIES_PER_TABLE * PAGING_PAGE_SIZE); // offset increased by size of page table

        // Note: each has structure of a page directory entry
        directory[i] = (uint32_t)entry | flags | PAGING_IS_WRITEABLE;
        /* 
        Structure of Page Directory entries:
        31                               12  11   8   7     6    5    4     3     2     1    0
        ----------------------------------------------------------------------------------------
        |   Bits 31-12 of address(20-bits)  |  AVL  | PS | AVL | A | PCD | PWT | U/S | R/W | P |
        ----------------------------------------------------------------------------------------
        P:      Present                 D:      Dirty
        R/W:    Read/Write              PS:     Page Size
        U/S:    User/ Supervisor        G:      Global
        PWT:    Write-Through           AVL:    Available
        PCD:    Cache Disable           PAT:    Page Attribute Table
        A:      Accessed 
        */
    }

    struct paging_4gb_chunk* chunk_4gb = kzalloc(sizeof(struct paging_4gb_chunk));
    chunk_4gb->directory_entry = directory;
    return chunk_4gb;
}

void paging_switch(struct paging_4gb_chunk* directory)
{
    paging_load_directory(directory->directory_entry);
    current_directory = directory->directory_entry;
}

// Note: this will basically reverses what paging_new_4gb does
void paging_free_4gb(struct paging_4gb_chunk* chunk) {
    for (int i = 0; i < 1024; i++) {
        uint32_t entry = chunk->directory_entry[i];
        uint32_t* table = (uint32_t*)(entry & 0xfffff000);
        kfree(table);
    }

    kfree(chunk->directory_entry);
    kfree(chunk);
}

uint32_t* paging_4gb_chunk_get_directory(struct paging_4gb_chunk* chunk)
{
    return chunk->directory_entry;
}

bool paging_is_aligned(void* addr)
{
    return ((uint32_t)addr % PAGING_PAGE_SIZE) == 0;
} 

/* Note: takes the virtual address, finds out which page table it is in the page directory (so finds directory
entry), then goes to that page table and finds which page table entry (so finds page table entry index, and
then returns both indexes*/
int paging_get_indexes(void* virtual_address, uint32_t* directory_index_out, uint32_t* table_index_out)
{
    int res = 0;
    // Check: if the virtual address is aligned with page size
    if (!paging_is_aligned(virtual_address))
    {
        res = -EINVARG;
        goto out;
    }  

    *directory_index_out = ((uint32_t)virtual_address / (PAGING_TOTAL_ENTRIES_PER_TABLE * PAGING_PAGE_SIZE));
    *table_index_out = ((uint32_t) virtual_address % (PAGING_TOTAL_ENTRIES_PER_TABLE * PAGING_PAGE_SIZE) / PAGING_PAGE_SIZE);
out:
    return res;
}

// Note: aligns an address with the page size
void* paging_align_address(void* ptr)
{
    // Check: if it already aligned, if not then...
    if ((uint32_t)ptr % PAGING_PAGE_SIZE)
    {
        return (void*)((uint32_t)ptr + PAGING_PAGE_SIZE - ((uint32_t)ptr % PAGING_PAGE_SIZE));
    }

    return ptr;
}

// Note: works like floor function
void* paging_align_to_lower_page(void* addr)
{
    uint32_t _addr = (uint32_t) addr;
    _addr -= (_addr % PAGING_PAGE_SIZE);
    return (void*) _addr;
}

// Note: maps virtual address to physical address in a directory
int paging_map(struct paging_4gb_chunk* directory, void* virt, void* phys, int flags) {
    // Check: if virtual and physical addresses are page aligned
    if ((unsigned int)virt % PAGING_PAGE_SIZE || ((unsigned int)phys % PAGING_PAGE_SIZE)) {
        return -EINVARG;
    }
    return paging_set(directory->directory_entry, virt, (uint32_t)phys | flags);
}

// Note: for each page, we map the virt with phys, and then increment by page size
int paging_map_range(struct paging_4gb_chunk* directory, void* virt, void* phys, int count, int flags) {
    int res = 0;
    // Here: we set the addresses for each page
    for (int i = 0; i < count; i++) {
        res = paging_map(directory, virt, phys, flags);
        if (res < 0)
            break;
        virt += PAGING_PAGE_SIZE;
        phys += PAGING_PAGE_SIZE;
    }
    return res;
}

int paging_map_to(struct paging_4gb_chunk* directory, void* virt, void* phys, void* phys_end, int flags) {
    int res = 0;
    // Check: for address alignment with page size
    if ((uint32_t) virt % PAGING_PAGE_SIZE) {
        res = -EINVARG;
        goto out;
    }
    if ((uint32_t) phys % PAGING_PAGE_SIZE) {
        res = -EINVARG;
        goto out;
    }
    if ((uint32_t) phys_end % PAGING_PAGE_SIZE) {
        res = -EINVARG;
        goto out;
    }
    if ((uint32_t) phys_end < (uint32_t) phys) {
        res = -EINVARG;
        goto out;
    }
    // Here: we check how many pages we need for the program
    uint32_t total_bytes = phys_end - phys;
    int total_pages = total_bytes / PAGING_PAGE_SIZE;

    // Here: we do the actual mapping
    res = paging_map_range(directory, virt, phys, total_pages, flags);

    out:
        return res;
}

// Note: takes a virtual address, and maps it to a real address
int paging_set(uint32_t* directory, void* virt, uint32_t val)
{
    // Check: if the virtual address is aligned with page size
    if (!paging_is_aligned(virt))
    {
        return -EINVARG;
    }

    uint32_t directory_index = 0;
    uint32_t table_index = 0;
    
    // Here: we get the page directory and page table index
    int res = paging_get_indexes(virt, &directory_index, &table_index);
    if (res < 0)
    {
        return res;
    }

    uint32_t entry = directory[directory_index];
    uint32_t* table = (uint32_t*)(entry & 0xfffff000);
    // We go there and asign our value (physical address) there
    table[table_index] = val;

    return 0;
}

// Note: this will take a virtual address, and look in the directory, and find the page table, then return the page table entry
uint32_t paging_get(uint32_t* directory, void* virt) {

    uint32_t directory_index = 0;
    uint32_t table_index = 0;

    paging_get_indexes(virt, &directory_index, &table_index);
    uint32_t entry = directory[directory_index];
    uint32_t* table =  (uint32_t*) (entry & 0xfffff000);
    return table[table_index];
}

// Note: given a virtual address and directory, get the physical address
void* paging_get_physical_address(uint32_t* directory, void* virt)
{
    // Here: we check for alignment
    void* virt_addr_new = (void*) paging_align_to_lower_page(virt);
    void* difference = (void*)((uint32_t) virt - (uint32_t) virt_addr_new);

    return (void*)((paging_get(directory, virt_addr_new) & 0xfffff000) + difference);
}