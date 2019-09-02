#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>

struct termios orig_termios;
// Store the original terminal settings so we can
// restore them on exit.

void die(const char *s) {
    perror(s);
    exit(1);
}

void disableRawMode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1) die("tcsetattr");
}

void enableRawMode() {
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) die("tcgetattr");
    // retrieve the current terminal settings

    atexit(disableRawMode);
    // Register disableRawMode to be called when exit() is called
    // or when program returns from main().

    struct termios raw = orig_termios;
    raw.c_iflag &= ~(IXON | ICRNL | INPCK | ISTRIP | BRKINT);
    // This disables ctrl-s and ctrl-q
    // which are used to stop and continue data from
    // being transmitted to the terminal.
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    /* 
    Symbol ~ means bitwise complement
    Symbol & means bitwise 'and'
    When '&' with the complement that flag gets turned off
    When '|' that flag gets turned on (right?)
    ECHO means echoing to the terminal.
    ICANON means canonical mode - reading line-by-line.
    We want this to read byte-by-byte
    With ICANON, the program quits as soon as we press 'q'
    ISIG means signals like SIGINT (ctrl-c) or SIGTSTP (ctrl-z).
    We want those ignored.
     */
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
    // Set the new attributes
    // TCSAFLUSH and stuff are in the manpage for termios
}

int main() {
    enableRawMode();

    while (1) {
        char c = '\0';
        if (read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN) die ("read");
        if (iscntrl(c)) {
            printf("%d\r\n", c);
        }
        else {
            printf("%d ('%c')\r\n", c, c);
        }
        if(c == 'q') break;
    }
    return 0;
}