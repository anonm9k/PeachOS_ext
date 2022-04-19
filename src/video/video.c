#include "video.h"
#include <stdint.h>
#include <stddef.h>
#include "string/string.h"
#include "task/process.h"
#include "task/task.h"
#include "task/shell.h"
#include "memory/memory.h"

uint16_t* video_mem = (uint16_t*) 0xB8000;
uint16_t terminal_row = 0;
uint16_t terminal_col = 0;

void terminal_init() {
    terminal_row = task_current()->process->shell->terminal->terminal_row;
    terminal_col = task_current()->process->shell->terminal->terminal_col;
}

void terminal_save(struct terminal* terminal) {
    terminal->terminal_row = terminal_row;
    terminal->terminal_col = terminal_col;
}

struct shell* get_current_task_shell() {
    return task_current()->process->shell;
}

uint16_t terminal_make_char(char c, char colour)
{
    // Boring: just deals with endianness
    return (colour << 8) | c;
}

void terminal_putchar(int x, int y, char c, char colour)
{
    get_current_task_shell()->video_mem[(y * VGA_WIDTH) + x] = terminal_make_char(c, colour);
    if (get_current_shell()->shell_id == get_current_task_shell()->shell_id) {
        memcpy((void*)video_mem, (void*)get_current_task_shell()->video_mem, 4096);
    }
}

bool backspace = false;
void terminal_writechar(char c, char colour)
{   
    if (!backspace) {
        terminal_init();
    }
    
    if (c == '\n')
    {
        terminal_row += 1;
        terminal_col = 0;
        terminal_save(get_current_task_shell()->terminal);
        return;
    }

    if (c == 0x08) { // 0x08 is ASCII code for backspace
        terminal_backspace();
        terminal_save(get_current_task_shell()->terminal);
        return;
    }

    terminal_putchar(terminal_col, terminal_row, c, colour);
    terminal_col += 1;
    if (terminal_col >= VGA_WIDTH)
    {
        terminal_col = 0;
        terminal_row += 1;
    }
    terminal_save(get_current_task_shell()->terminal);
}

void terminal_backspace() {
    backspace = true;
    if (terminal_row == 0 && terminal_col == 0)
    {
        return;
    }

    if (terminal_col == 0)
    {
        terminal_row -= 1;
        terminal_col = VGA_WIDTH;
    }

    terminal_col -=1;
    terminal_writechar(' ', 142);
    terminal_col -=1;
    backspace = false;
}

void print(const char* str)
{
    size_t len = strlen(str);
    for (int i = 0; i < len; i++)
    {
        terminal_writechar(str[i], 142);
    }
}

void terminal_initialize()
{
    //video_mem = (uint16_t*)(0xB8000);
    terminal_row = 0;
    terminal_col = 0;
    for (int y = 0; y < VGA_HEIGHT; y++)
    {
        for (int x = 0; x < VGA_WIDTH; x++)
        {
            terminal_putchar(x, y, ' ', 142);
        }
    }   
     
}