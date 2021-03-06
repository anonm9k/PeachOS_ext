#ifndef KERNEL_H
#define KERNEL_H

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define PEACHOS_COLOR 240

#define PEACHOS_MAX_PATH 108

void kernel_main(); // kernel.asm
void kernel_registers(); // kernel.asm

void kernel_page();

void print(const char* str);
void panic(const char* msg);

void terminal_writechar(char c, char colour);

#define ERROR(value) (void*)(value)
#define ERROR_I(value) (int)(value)
#define ISERR(value) ((int)value < 0)

#endif