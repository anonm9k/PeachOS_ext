#include "shell.h"
#include "status.h"
#include "task/task.h"
#include "task/process.h"
#include "memory/heap/kheap.h"


struct shell* shells[12];
struct shell* current_shell = 0;

// Note: Check for availibility
bool shell_slot_available(int id) {
    return (shells[id] == 0x00);
}

// Note: This will create a new shell and append it to shells list
int shell_new(int id, struct shell* shell) {
    int res = 0;

    if(!shell_slot_available(id)) {
        res = -EINVARG;
        goto out;
    }

    // Step1: give it shell ID
    shell->shell_id = id;

    // Step2: give it video buffer memory
    void* video_buf = (void*)kzalloc(4096);
    shell->video_mem = (uint16_t*)video_buf;

    // Step3: Set up terminal
    struct terminal* terminal = kzalloc(sizeof(struct terminal));
    terminal->terminal_row = 0;
    terminal->terminal_col = 0;
    shell->terminal = terminal;

    // Step4: Start its first ever process, shell.elf, and link it to its internel processes list
    struct process* shell_process;
    res = process_load_switch("0:/shell.elf", &shell_process);
    if (res < 0) {
        res = -EIO;
        goto out;
    }
    shell_process->shell = shell; // Link the process with shell (process > shell)
    shell->process[0] = shell_process; // link shell with process (shell > process)
    
    // Step5: Append this shell to the shells list
    shells[id] = shell;

    // Step6: Set this shell as the current shell running
    current_shell = shell;

    out:
        return res;
}

// Note: Just returns current shell
struct shell* get_current_shell() {
    return current_shell;
}

