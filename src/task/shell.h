#ifndef SHELL_H
#define SHELL_H
#include <stdint.h>
#include <stdbool.h>

struct terminal {
    uint16_t terminal_row;
    uint16_t terminal_col;
};

struct shell {
    int shell_id;
    uint16_t* video_mem;
    struct terminal* terminal;
    struct process* process[2];
    struct process* current_process;
};

int shell_new(int id, struct shell** shell);
struct shell* get_current_shell();
bool shell_slot_available(int id);
void shell_switch_to_next();
int shell_switch(int shell_id);
void shell_initialize(struct shell* shell);

#endif