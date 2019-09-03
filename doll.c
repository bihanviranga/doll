/*** includes ***/

#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <string.h>

/*** defines ***/

#define CTRL_KEY(k) ((k) & 0x1f)
/* 
Set the upper 3 bits to 0 through bitwise 'and'.
It is similar to what the Ctrl key does in the terminal:
Take whatever key you press and strip the bits 5 and 6
(which are the highest 2 bits).
ASCII has 7 bits.    
*/

#define ABUF_INIT {NULL, 0}
// This acts as a constructor to abuf type

/*** data ***/

struct editorConfig {
    int screenrows;
    int screencols;
    struct termios orig_termios;
    // Store the original terminal settings so we can
    // restore them on exit.
};

struct editorConfig E;

/*** terminal ***/

void die(const char *s) {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);

    perror(s);
    exit(1);
}

void disableRawMode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1) die("tcsetattr");
}

void enableRawMode() {
    if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) die("tcgetattr");
    // retrieve the current terminal settings

    atexit(disableRawMode);
    // Register disableRawMode to be called when exit() is called
    // or when program returns from main().

    struct termios raw = E.orig_termios;
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

char editorReadKey() {
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) die("read");
    }
    return c;
}

int getCursorPosition(int *rows, int *cols) {
    char buf[32];
    unsigned int i = 0;
    
    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;

    // The cursor position is several bytes long.
    // So we save it in a buffer.
    while (i < sizeof(buf) -1) {
        if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
        if (buf[i] == 'R') break;
        i++;
    }
    buf[i] = '\0';

    if (buf[0] != '\x1b' || buf[1] != '[') return -1;
    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;

    return 0;
}

int getWindowSize(int *rows, int *cols) {
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
        return getCursorPosition(rows, cols);
    }
    else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
    // The TIOCGWINSZ request to ioctl gives the size of window.
}

/*** append buffer ***/

/* 
Instead of making several calls to write() everytime we
want to refresh, we are using a buffer now.
*/
struct abuf {
    char *b;
    int len;
};

void abAppend(struct abuf *ab, const char *s, int len) {
    char *new = realloc(ab->b, ab->len + len);

    if(new == NULL) return;
    memcpy(&new[ab->len], s, len);
    ab->b = new;
    ab->len += len;
}

void abFree(struct abuf *ab) {
    free(ab->b);
}

/*** output ***/

void editorDrawRows(struct abuf *ab) {
    int y;
    for (y = 0; y < E.screenrows; y++) {
        abAppend(ab, "~", 1);

        // There was a \r\n after the last tilde,
        // which caused the screen to scroll,
        // revealing a new line without a tilde.
        // This condition check avoids that.
        if (y < E.screenrows - 1) {
            abAppend(ab, "\r\n", 2);
        }
    }
}

void editorRefreshScreen() {
    struct abuf ab = ABUF_INIT;

    abAppend(&ab, "\x1b[2J", 4);
    // Clears the screen
    abAppend(&ab, "\x1b[H", 3);
    // Moves the cursor to top-left
    
    // These are an escape sequences.
    // \x1b is 27 which is the escape character.
    // See the README section on Escape Sequences for further reading.

    editorDrawRows(&ab);

    abAppend(&ab, "\x1b[H", 3);

    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}

/*** input ***/

void editorProcessKeypress() {
    char c = editorReadKey();

    switch (c) {
        case CTRL_KEY('q'):
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
            break;
    }
}

/*** init ***/

void initEditor() {
    if (getWindowSize(&E.screenrows, &E.screencols) == -1) die ("getWindowSize");
}

int main() {
    enableRawMode();
    initEditor();

    while (1) {
        editorRefreshScreen();
        editorProcessKeypress();
    }
    
    return 0;
}
