#include "peachos.h"
#include <stdbool.h>


// Note: This function will keep running(block) until a key is pressed
int peachos_getkeyblock() {
    int val = 0;
    do {
        val = peachos_getkey();
    }
    while(val == 0);
    return val;
}

// Note: reads a line from the terminal
void peachos_terminal_readline(char* out, int max, bool output_while_typing) {
    int i = 0;
    for (i = 0; i < max -1; i++) {
        char key = peachos_getkeyblock();

        // Check: carriage return
        if (key == 13) {
            break;
        }

        // Check: if it is OK to print the buffer
        if (output_while_typing) {
            peachos_putchar(key);
        }

        // Check: backspace
        if (key == 0x08 && i >= 1) {
            out[i-1] = 0x00;
            // -2 because we will +1 when we go to continue
            i -= 2;
            continue;
        }
        out[i] = key;
    }

    // Here: we add null terminator
    out[i] = 0x00;
}