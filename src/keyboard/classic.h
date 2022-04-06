#ifndef CLASSIC_KEYBOARD_H
#define CLASSIC_KEYBOARD_H

#define PS2_PORT 0x64 // This is the command port register
#define KEYBOARD_INPUT_PORT 0x60 // This is the data port
#define PS2_COMMAND_ENABLE_FIRST_PORT 0xAE

#define CLASSIC_KEYBOARD_KEY_RELEASED 0x80 // This is bitmask
#define ISR_KEYBOARD_INTERRUPT 0x21


struct keyboard* classic_init();
void classic_keyboard_interrupt_handler();

#endif