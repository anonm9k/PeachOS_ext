#ifndef ISR80H_PROCESS_H
#define ISR80H_PROCESS_H

#include "task/task.h"
#include "task/process.h"


struct process_arguments;
struct command_argument;


void* isr80h_command6_process_load_start(struct interrupt_frame* frame);
void* isr80h_command7_invoke_system_command(struct interrupt_frame* frame);
void* isr80h_command8_get_program_arguments(struct interrupt_frame* frame);
void* isr80h_command9_exit(struct interrupt_frame* frame);

#endif