#include <stdint.h>
#include "shell.h"
#include "status.h"
#include "task/task.h"
#include "task/process.h"
#include "memory/heap/kheap.h"
#include "memory/memory.h"
#include "video/video.h"
#include "string/string.h"


struct shell* shells[12] = {};
struct shell* current_shell = 0;
uint16_t* video_buf = (uint16_t*) 0xB8000;

// Note: Check for availibility
bool shell_slot_available(int id) {
    return (shells[id] == 0x00);
}

// Note: This will create a new shell and append it to shells list
int shell_new(int id, struct shell** shell) {
    int res = 0;

    if(!shell_slot_available(id)) {
        res = -EINVARG;
        goto out;
    }

    struct shell* _shell;
    _shell = kzalloc(sizeof(struct shell));

    // Step1: give it shell ID
    _shell->shell_id = id;

    // Step2: Append this shell to the shells list
    //shells[id] = _shell;

    // Step3: give it video buffer memory
    void* video_buf = (void*)kzalloc(4096);
    _shell->video_mem = (uint16_t*)video_buf;

    // Step4: Set up terminal
    struct terminal* terminal = kzalloc(sizeof(struct terminal));
    terminal->terminal_row = 0;
    terminal->terminal_col = 0;
    _shell->terminal = terminal;


    // Step5: Start its first ever process, shell.elf, and link it to its internel processes list
    struct process* shell_process;
    res = process_load_switch("0:/shell.elf", &shell_process);
    if (res < 0) {
        res = -EIO;
        goto out;
    }
    shell_process->shell = _shell; // Link the process with shell (process > shell)
    _shell->process[0] = shell_process; // link shell with process (shell > process)
    struct command_argument argument;
    strcpy(argument.argument, itoa(id));
    argument.next = 0x00; 
    process_inject_arguments(shell_process, &argument);

    // Here: we initialize the shell, puts color
    shell_initialize(_shell);

    *shell = _shell;
    shells[id] = _shell;

    // Step6: Set this shell as the current shell running
    shell_switch(id);

    out:
        return res;
}

// Note: Just returns current shell
struct shell* get_current_shell() {
    return current_shell;
}

// Note: switches to a shell in shells list given the index
int shell_switch(int shell_id) {
    if (shells[shell_id] == 0x00) {
        return -EINVARG;
    }
    current_shell = shells[shell_id];
    process_switch(current_shell->process[0]);
    memcpy((void*)0xB8000, current_shell->video_mem, 4096);
    return 0;
}

// Note: switches to next shell in round order
void shell_switch_to_next() {
    int current_shell_id = current_shell->shell_id;

    if (current_shell_id == 0 && shells[current_shell_id+1]) {
        shell_switch(current_shell_id+1);
    }
    else if (current_shell_id == 0 && !shells[current_shell_id+1]) {
        shell_switch(current_shell_id);
    }

    else if (current_shell_id != 0 && shells[current_shell_id+1]) {
        shell_switch(current_shell_id+1);
    }
    else if (current_shell_id != 0 && !shells[current_shell_id+1]) {
        shell_switch(0);
    }
}

// Note: initializes the shell, puts color
void shell_initialize(struct shell* shell) {
    for (int y = 0; y < VGA_HEIGHT; y++)
    {
        for (int x = 0; x < VGA_WIDTH; x++)
        {
            shell->video_mem[(y * VGA_WIDTH) + x] = terminal_make_char(' ', 142);
        }
    }   
     
}