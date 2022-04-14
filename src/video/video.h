#ifndef VIDEO_H
#define VIDEO_H

#define VGA_WIDTH 80
#define VGA_HEIGHT 25

void terminal_backspace();
void terminal_writechar(char c, char colour);
void terminal_putchar(int x, int y, char c, char colour);
void print(const char* str);
void terminal_initialize();

#endif