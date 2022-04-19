#ifndef VIDEO_H
#define VIDEO_H

#include <stdint.h>

#define VGA_WIDTH 80
#define VGA_HEIGHT 25

uint16_t terminal_make_char(char c, char colour);
void terminal_backspace();
void terminal_writechar(char c, char colour);
void terminal_putchar(int x, int y, char c, char colour);
void print(const char* str);
void terminal_initialize();

#endif