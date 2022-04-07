#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define PAGING_CACHE_DISABLED  0b00010000
#define PAGING_WRITE_THROUGH   0b00001000
#define PAGING_ACCESS_FROM_ALL 0b00000100
#define PAGING_IS_WRITEABLE    0b00000010
#define PAGING_IS_PRESENT      0b00000001


#define PAGING_TOTAL_ENTRIES_PER_TABLE 1024
#define PAGING_PAGE_SIZE 4096

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


struct paging_4gb_chunk
{
    uint32_t* directory_entry;
};

struct paging_4gb_chunk* paging_new_4gb(uint8_t flags);
void paging_free_4gb(struct paging_4gb_chunk* chunk);
void paging_switch(struct paging_4gb_chunk* directory);
void enable_paging();

int paging_map_to(struct paging_4gb_chunk* directory, void* virt, void* phys, void* phys_end, int flags);
int paging_map_range(struct paging_4gb_chunk* directory, void* virt, void* phys, int count, int flags);
int paging_map(struct paging_4gb_chunk* directory, void* virt, void* phys, int flags) ;
int paging_set(uint32_t* directory, void* virt, uint32_t val);
uint32_t paging_get(uint32_t* directory, void* virt);

bool paging_is_aligned(void* addr);
void* paging_align_address(void* ptr);
void* paging_align_to_lower_page(void* addr);


uint32_t* paging_4gb_chunk_get_directory(struct paging_4gb_chunk* chunk);

#endif