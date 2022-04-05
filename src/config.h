#ifndef CONFIG_H
#define CONFIG_H

#define KERNEL_CODE_SELECTOR 0x08
#define KERNEL_DATA_SELECTOR 0x10


#define PEACHOS_TOTAL_INTERRUPTS 512

// 100MB heap size
#define PEACHOS_HEAP_SIZE_BYTES 104857600
#define PEACHOS_HEAP_BLOCK_SIZE 4096
#define PEACHOS_HEAP_ADDRESS 0x01000000 
#define PEACHOS_HEAP_TABLE_ADDRESS 0x00007E00

#define PEACHOS_SECTOR_SIZE 512

#define PEACHOS_MAX_FILESYSTEMS 12
#define PEACHOS_MAX_FILE_DESCRIPTORS 512

#define PEACHOS_MAX_PATH 108

#define PEACHOS_TOTAL_GDT_SEGMENTS 6

#define PEACHOS_PROGRAM_VIRTUAL_ADDRESS 0x400000
#define PEACHOS_USER_PROGRAM_STACK_SIZE 1024 * 16 // 16KB stack size: must be aligned with page size (4096)
#define PEACHOS_PROGRAM_VIRTUAL_STACK_ADDRESS_START 0x3FF000

#define PEACHOS_MAX_PROGRAM_ALLOCATIONS 1024
#define PEACHOS_MAX_PROCESSES 12

// Here: we minus (-) it because stack grows downwards
#define PEACHOS_PROGRAM_VIRTUAL_STACK_ADDRESS_END PEACHOS_PROGRAM_VIRTUAL_STACK_ADDRESS_START - PEACHOS_USER_PROGRAM_STACK_SIZE

// Note: these numbers come from kernel.c gdt_real entries
#define USER_DATA_SEGMENT 0x23
#define USER_CODE_SEGMENT 0x1b

#define PEACHOS_MAX_ISR80H_COMMANDS 1024

#define PEACHOS_KEYBOARD_BUFFER_SIZE 1024

#endif