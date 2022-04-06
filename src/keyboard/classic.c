#include <stdint.h>
#include <stddef.h>
#include "keyboard.h"
#include "io/io.h"
#include "classic.h"
#include "kernel.h"
#include "idt/idt.h"
#include "task/task.h"


int classic_keyboard_init();

// Note: here 0x00 are ignored
// Note: last few are numpad
static uint8_t keyboard_scan_set_one[] = {
    0x00, 0x1B, '1', '2', '3', '4', '5',
    '6', '7', '8', '9', '0', '-', '=',
    0x08, '\t', 'Q', 'W', 'E', 'R', 'T',
    'Y', 'U', 'I', 'O', 'P', '[', ']',
    0x0d, 0x00, 'A', 'S', 'D', 'F', 'G',
    'H', 'J', 'K', 'L', ';', '\'', '`', 
    0x00, '\\', 'Z', 'X', 'C', 'V', 'B',
    'N', 'M', ',', '.', '/', 0x00, '*',
    0x00, 0x20, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, '7', '8', '9', '-', '4', '5',
    '6', '+', '1', '2', '3', '0', '.'
};

// Here: keyboard structure
struct keyboard classic_keyboard = {
    .name = {"Classic"},
    .init = classic_keyboard_init
};

// Note: enables the PS/2 port
int classic_keyboard_init() {

    idt_register_interrupt_callback(ISR_KEYBOARD_INTERRUPT, classic_keyboard_interrupt_handler);

    // Note: 0x64 is command register (port), and 0xAe value tells the controller to enable the PS/2 port (check osdev.org)
    // Here: we enable the PS/2 port
    outb(PS2_PORT, PS2_COMMAND_ENABLE_FIRST_PORT);
    return 0;
}

// Note: converts scancode to 8-bit ASCII value
uint8_t classic_keyboard_scancode_to_char(uint8_t scancode) {
    // Here: we get the size of our scancodes array
    size_t size_of_keyboard_set_one = sizeof(keyboard_scan_set_one) / sizeof(uint8_t);
    // Check: if out of bound
    if (scancode > size_of_keyboard_set_one)
    {
        return 0;
    }

    char c = keyboard_scan_set_one[scancode];
    return c;
}

// Note: our generic keyboard interrupt handler (0x21)
void classic_keyboard_interrupt_handler() {
    kernel_page(); // Switch from task page to kernel page
    uint8_t scancode = 0;
    scancode = insb(KEYBOARD_INPUT_PORT); // Takes the scancode from keyboard
    insb(KEYBOARD_INPUT_PORT); // So we can ingore the rogue bytes sent after that

    if (scancode & CLASSIC_KEYBOARD_KEY_RELEASED) {
        return; // Note: for now we dont handle key released
    }

    uint8_t c = classic_keyboard_scancode_to_char(scancode);

    // Note: if converted character is valid ASCII
    if (c != 0) {
        keyboard_push(c); // We push it to the current processes keyboard buffer
    }

    task_page(); // Switch back page directory
}

struct keyboard* classic_init() {
    return &classic_keyboard;
};