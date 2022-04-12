#include "keyboard.h"
#include "status.h"
#include "kernel.h"
#include "task/process.h"
#include "task/task.h"
#include "classic.h"


static struct keyboard* keyboard_list_head = 0;
static struct keyboard* keyboard_list_last = 0;

void keyboard_init() {
    keyboard_insert(classic_init());
}

// Note: inserts the keyboard into the linked list
int keyboard_insert(struct keyboard* keyboard) {
    int res = 0;
    // Check: if the driver has an init function
    if (keyboard->init == 0) {
        res = -EINVARG;
        goto out;
    }

    // Check: if keyboard list doesnt exist, we append this keyboard to the list
    if (keyboard_list_last) {
        keyboard_list_last->next = keyboard;
        keyboard_list_last = keyboard;
    }

    else {
        keyboard_list_head = keyboard;
        keyboard_list_last = keyboard;
    }

    res = keyboard->init();

    out:
        return res;
}

// Note: returns keyboard tail, if out of bound, gets looped
int keyboard_get_tail_index(struct process* process) {
    return process->keyboard.tail % sizeof(process->keyboard.buffer);
}

// Note: reduces tail pointer, places null into current pointer in buffer
void keyboard_backspace(struct process* process) {
    // Here: we reduce the tail pointer
    process->keyboard.tail -= 1;
    int real_index = keyboard_get_tail_index(process);
    // Here: we set null into the buffer current index
    process->keyboard.buffer[real_index] = 0x00;
}

// Note: modify keyboard capslock_state
void keyboard_set_capslock(struct keyboard* keyboard, KEYBOARD_CAPS_LOCK_STATE state) {
    keyboard->capslock_state = state;
}

// Note: just returns keyboard->capslock_state
KEYBOARD_CAPS_LOCK_STATE keyboard_get_capslock(struct keyboard* keyboard) {
    return keyboard->capslock_state;
}

// Note: pushes a character into current process keyboard char buffer
void keyboard_push(char c) {
    struct process* process = process_current();
    // Check: if we get the current process
    if (!process) {
        return;
    }
    // Check: if character is null
    if (c == 0x00) {
        return;
    }

    int real_index = keyboard_get_tail_index(process);
    process->keyboard.buffer[real_index] = c;
    process->keyboard.tail++;
}

// Note: gets character from the keyboard buffer of current tasks process
char keyboard_pop() {

    // Check: if task is running
    if (!task_current()) {
        return 0;
    }

    struct process* process = task_current()->process;
    int real_index = process->keyboard.head % sizeof(process->keyboard.buffer);
    char c = process->keyboard.buffer[real_index];

    // Check: if nothing to pop
    if (c == 0x00) {
        return 0;
    }

    process->keyboard.buffer[real_index] = 0;
    process->keyboard.head++;
    return c;
}