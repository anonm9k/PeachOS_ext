#include "process.h"
#include "task/task.h"
#include "config.h"
#include "status.h"
#include "task/process.h"
#include "string/string.h"


void* isr80h_command6_process_load_start(struct interrupt_frame* frame) {

    void* filename_user_ptr = task_get_stack_item(task_current(), 0);
    char filename[PEACHOS_MAX_PATH];
    // Here: we copy the string pointed by the poiter to our buffer
    int res = copy_string_from_task(task_current(), filename_user_ptr, filename, sizeof(filename));
    if (res < 0) {
        goto out;
    }

    char path[PEACHOS_MAX_PATH];
    strcpy(path, "0:/");
    strcpy(path+3, filename);

    struct process* process = 0;
    res = process_load_switch(path, &process);
    if (res < 0) {
        goto out;
    }

    // Here: we switch to task (page directory basically), and finally drop into user land
    task_switch(process->task);
    task_return(&process->task->registers);

    out:
        return 0;
}