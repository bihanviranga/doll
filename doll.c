#include <unistd.h>
#include <termios.h>
#include <stdlib.h>

struct termios orig_termios;
// Store the original terminal settings so we can
// restore them on exit.

void disableRawMode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enableRawMode() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    // retrieve the current terminal settings

    atexit(disableRawMode);
    // Register disableRawMode to be called when exit() is called
    // or when program returns from main().

    struct termios raw = orig_termios;

    raw.c_lflag &= ~(ECHO);
    // Symbol ~ means bitwise complement
    // Symbol & means bitwise 'and'
    // When '&' with the complement that flag gets turned off
    // When '|' that flag gets turned on (right?)

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    // Set the new attributes
    // TCSAFLUSH and stuff are in the manpage for termios
}

int main() {
    enableRawMode();

    char c;
    while (read(STDIN_FILENO, &c, 1) == 1 && c != 'q');
    return 0;
}