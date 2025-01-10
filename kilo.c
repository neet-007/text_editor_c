/*** includes ***/

#include "utils.h"
#include <stdbool.h>
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <asm-generic/ioctls.h>
#include <ctype.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h> 
#include <sys/utsname.h>
#include "editor_config.h"
#include "editor_commands.h"
#include "row.h"
#include "screen.h"

/*** defines ***/
#define KILO_VERSION "0.0.1"
#define KILO_QUIT_TIMES 3

#define CTRL_KEY(k) ((k) & 0x1f)

enum OS_TYPE { 
    OS_UNKNOWN,
    OS_WINDOWS,
    OS_LINUX,
    OS_MACOS 
};

enum OS_TYPE detect_os() {
#ifdef _WIN32
    return OS_WINDOWS;
#elif __APPLE__
    return OS_MACOS;
#elif __linux__
    return OS_LINUX;
#else
    return OS_UNKNOWN;
#endif
}

enum editorKey {
    BACKSPACE = 127,
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    DEL_KEY,
    HOME_KEY,
    END_KEY,
    PAGE_UP,
    PAGE_DOWN,
};

/*** data ***/

struct editorConfig E;

/*** filetypes ***/

/*** prototypes ***/

char *editorPrompt(char *prompt, void (*callback)(char *, int));

void enableMouse() {
    write(STDOUT_FILENO, "\033[?1003h", 8);
}

void disableMouse() {
    write(STDOUT_FILENO, "\033[?1003l", 8);
}

/*** terminal ***/

void disableRawMode(){
    disableMouse();
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1){
        die("tcsetattr");
    }
}

void enableRawMode() {
    if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1){
        die("tcgetattr");
    }
    atexit(disableRawMode);

    struct termios raw = E.orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= ~(CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1){
        die("tcsetattr");
    }
}

int editorReadKey(int *count){
    int nread;
    char c;
    *count = 0;

    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) {
            die("read");
        }
    }

    while(E.mode != INSERT && '0' <= c && c <= '9'){
        (*count) = c  - '0' + (*count) * 10;
        while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
            if (nread == -1 && errno != EAGAIN) {
                die("read");
            }
        }
    }

    if (c == '\x1b'){
        char s[3];

        if (read(STDIN_FILENO, &s[0], 1) != 1){
            return '\x1b';
        }
        if (read(STDIN_FILENO, &s[1], 1) != 1){
            return '\x1b';
        }

        if (s[0] == '['){
            if (s[1] >= '0' && s[1] <= '9'){
                if (read(STDIN_FILENO, &s[2], 1) != 1){
                    return '\x1b';
                }

                if (s[2] == '~'){
                    switch (s[1]) {
                        case '1':{
                            return HOME_KEY;
                        }
                        case '3':{
                            return DEL_KEY;
                        }
                        case '4':{
                            return END_KEY;
                        }
                        case '5': {
                            return PAGE_UP;
                        }
                        case '6':{
                            return PAGE_DOWN;
                        }
                        case '7':{
                            return HOME_KEY;
                        }
                        case '8':{
                            return END_KEY;
                        }
                    }
                }
                return '\x1b';
            }

            switch (s[1]) {
                case 'A':{
                    return ARROW_UP;
                }
                case 'B':{
                    return ARROW_DOWN;
                }
                case 'C':{
                    return ARROW_RIGHT;
                }
                case 'D':{
                    return ARROW_LEFT;
                }
                case 'H':{
                    return HOME_KEY;
                }
                case 'F':{
                    return END_KEY;
                }
            }
        }

        if (s[0] == 'O') {
            switch (s[1]) {
                case 'H': return HOME_KEY;
                case 'F': return END_KEY;
            }
        }

        return '\x1b';
    }else if (c == '\033'){
        char s[5];
    
        if (read(STDIN_FILENO, &s[0], 1) != 1){
            return c;
        }
        if (read(STDIN_FILENO, &s[1], 1) != 1){
            return c;
        }

        if (s[0] == '['){
            if (s[1] == 'M'){
                int event = s[2] - 32;
                int x = s[3] - 32;   
                int y = s[4] - 32;  
            }
        }

        return c;
        
    } else{
        return c;
    }

}

int getCursorPosition(int *rows, int *cols) {
    char buf[32];
    unsigned int i = 0;
    
    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) {
        return -1;
    }
    while (i < sizeof(buf) - 1) {
        if (read(STDIN_FILENO, &buf[i], 1) != 1) {
            break;
        }
        if (buf[i] == 'R') {
            break;
        }
        i++;
    }
    buf[i] = '\0';

    if (buf[0] != '\x1b' || buf[1] != '['){
        return -1;
    }
    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2){
        return -1;
    }

    return 0;
}

int getWindowSize(int *rows, int *cols){
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0){
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12){
            return -1;
        }
        return getCursorPosition(rows, cols);
    }

    *cols = ws.ws_col;
    *rows = ws.ws_row;

    return 0;
}

/*** editor operations ***/

void editorInsertChar(int c){
    if (E.cy == E.numrows){
        editorInsertRow(&E, E.numrows, "", 0);
    }
    if (E.indent == SPACE && c == '\t'){
        for (int i = 0; i < E.indent_amount; i++){
            editorRowInsertChar(&E, &E.row[E.cy], editor_cx_to_index(&E), SPACE);
            E.cx++;
        }
    }else{
        editorRowInsertChar(&E, &E.row[E.cy], editor_cx_to_index(&E), c);
        E.cx++;
    }
}

int editorCountIndent(erow *row){
    int count = 0;
    for (int i = 0; i < row->size; i++){
        if (row->chars[i] != E.indent){
            return count;
        }
        count++;
    }

    return count;
}

void editorInsertNewlineCommand(int dir){
    erow *row = &E.row[E.cy];
    int indent_count = editorCountIndent(row);
    if (indent_count > 0){
        char *buf = malloc(sizeof(char) * indent_count + 1);

        int i;
        for (i = 0; i < indent_count; i++) {
            buf[i] = E.indent;
        }

        buf[i] = '\0';

        if (dir > 0){
            editorInsertRow(&E, ++E.cy, buf, indent_count);
        }else{
            editorInsertRow(&E, E.cy, buf, indent_count);
        }
        E.cx = E.last_row_digits + indent_count;
        return;
    }

    if (dir > 0){
        editorInsertRow(&E, ++E.cy, "", 0);
    }else{
        editorInsertRow(&E, E.cy, "", 0);
    }
    E.cx = E.last_row_digits;
}

void editorInsertNewline(){
    if (E.cx == E.last_row_digits){
        editorInsertRow(&E, E.cy, "", 0);
        E.cy++;
        E.cx = E.last_row_digits;
    }else{
        erow *row = &E.row[E.cy];
        int indent_count = editorCountIndent(row);
        if (indent_count > 0){
            char *buf = malloc(sizeof(char) * (indent_count + row->size - editor_cx_to_index(&E) + 1));

            int i;
            for (i = 0; i < indent_count; i++) {
                buf[i] = E.indent;
            }

            buf[i] = '\0';

            strcat(buf, &row->chars[editor_cx_to_index(&E)]);
            editorInsertRow(&E, E.cy + 1, buf, indent_count + row->size - editor_cx_to_index(&E));
        }else {
            editorInsertRow(&E, E.cy + 1, &row->chars[editor_cx_to_index(&E)], row->size - editor_cx_to_index(&E));
        }
        row = &E.row[E.cy];
        row->size = editor_cx_to_index(&E);
        row->chars[row->size] = '\0';
        E.cy++;
        E.cx = indent_count + E.last_row_digits;
        editorUpdateRow(&E, row);
    }
}

void editorDeleteCommand(int amount){
    if (E.cy == E.numrows || (E.cx == E.last_row_digits && E.cy == 0)){
        return;
    }

    erow *row = &E.row[E.cy];
    if (E.cx > E.last_row_digits){
        E.cx = max(E.cx - 1, E.last_row_digits);
        editorRowDelChar(&E, row, editor_cx_to_index(&E));
    }else{
        E.cx = E.row[E.cy - 1].size + E.last_row_digits;
        editorRowAppendString(&E, &E.row[E.cy - 1], row->chars, row->size);
        editorDelRow(&E, E.cy);
        E.cy--;
    }
}

void editorDelChar(){
    if (E.cy == E.numrows || (E.cx == E.last_row_digits && E.cy == 0)){
        return;
    }

    erow *row = &E.row[E.cy];
    if (E.cx > E.last_row_digits){
        E.cx = max(E.cx - 1, E.last_row_digits);
        editorRowDelChar(&E, row, editor_cx_to_index(&E));
    }else{
        E.cx = E.row[E.cy - 1].size + E.last_row_digits;
        editorRowAppendString(&E, &E.row[E.cy - 1], row->chars, row->size);
        editorDelRow(&E, E.cy);
        E.cy--;
    }
}

/*** file i/o ***/

char *editorRowsToString(int *buflen){
    int totlen = 0;
    int j;

    for (j = 0; j < E.numrows; j++){
        totlen += E.row[j].size + 1;
    }
    *buflen = totlen;

    char *buf = malloc(totlen);
    char *p = buf;

    for (j = 0; j < E.numrows; j++){
        memcpy(p, E.row[j].chars, E.row[j].size);
        p += E.row[j].size;
        *p = '\n';
        p++;
    }

    return buf;
}

void editorOpen(char *filename){
    free(E.filename);
    E.filename = strdup(filename);

    editorSelectSyntaxHighlight(&E);

    FILE *fp = fopen(filename, "r");
    if (!fp){
        E.dirty = 0;
        return;
        die("editor open");
    }
    char *line = NULL;
    size_t linecap;
    ssize_t linelen;
    while ((linelen = getline(&line, &linecap, fp)) != -1){
        while (linelen > 0 && (line[linelen - 1] == '\r' || line[linelen - 1] == '\n')){
            linelen --;
        }
        editorInsertRow(&E, E.numrows, line, linelen);
    }
    free(line);
    fclose(fp);
    E.last_row_digits = count_digits(E.numrows) + 1;
    E.screencols = E.screencolsBase - E.last_row_digits;
    E.cx = E.last_row_digits;
    E.dirty = 0;
}

void editorSave(){
    if (E.filename == NULL){
        E.filename = editorPrompt("Save as: %s", NULL);
        if (E.filename == NULL) {
            editorSetStatusMessage(&E, "Save aborted");
            return;
        }
        editorSelectSyntaxHighlight(&E);
    }


    int len;
    char *buf = editorRowsToString(&len);

    int fd = open(E.filename, O_RDWR | O_CREAT, 0644);
    if (fd == -1){
        editorSetStatusMessage(&E, "Can't save! I/O error: %s", strerror(errno));
        free(buf);
        return;
    }
    if (ftruncate(fd, len) == -1){
        editorSetStatusMessage(&E, "Can't save! I/O error: %s", strerror(errno));
        close(fd);
        free(buf);
        return;
    }

    if (write(fd, buf, len) != len){
        editorSetStatusMessage(&E, "Can't save! I/O error: %s", strerror(errno));
        close(fd);
        free(buf);
        return;
    }
    editorSetStatusMessage(&E, "%d bytes written to disk", len);
    close(fd);
    free(buf);
    E.dirty = 0;
}

/*** find ***/

void editorFindCallback(char *query, int key){
    static int last_match = -1;
    static int direction = 1;

    static int saved_hl_line;
    static char *saved_hl = NULL;

    if (saved_hl){
        memcpy(E.row[saved_hl_line].hl, saved_hl, E.row[saved_hl_line].rsize);
        free(saved_hl);
        saved_hl = NULL;
    }

    if (key == '\n' || key == '\x1b'){
        last_match = -1;
        direction = 1;
        return;
    }else if (key == ARROW_RIGHT || key == ARROW_DOWN){
        direction = 1;
    
    }else if (key == ARROW_LEFT || key == ARROW_UP){
        direction = -1;
    }else {
        last_match = -1;
        direction = 1;
    }

    if (last_match == -1){
        direction = 1;
    }
    int current = last_match;

    int i;
    for (i = 0; i < E.numrows; i++){
        current += direction;
        if (current == -1){
            current = E.numrows - 1;
        }
        if (current == E.numrows){
            current = 0;
        }

        erow *row = &E.row[current];
        char *match = strstr(row->render, query);
        if (match) {
            last_match = current;
            E.cy = current;
            E.cx = editorRowRxToCx(&E, row, match - row->render) ;
            E.rowoff = E.numrows;

            saved_hl_line = current;
            saved_hl = malloc(row->rsize);
            memcpy(saved_hl, row->hl, row->rsize);
            memset(&row->hl[match - row->render], HL_MATCH, strlen(query));
            break;
        }
    }
}

void editorFind(){
    int saved_cx = E.cx;
    int saved_cy = E.cy;
    int saved_coloff = E.coloff;
    int saved_rowoff = E.rowoff;

    char *query = editorPrompt("Search: %s (Use ESC/Arrows/Enter)", editorFindCallback);

    if (query){
        free(query);
    }else{
        E.cx = max(saved_cx, E.last_row_digits);
        E.cy = saved_cy;
        E.coloff = saved_coloff;
        E.rowoff = saved_rowoff;
    }
}

void editorFindInRow(int c, int dir){
    if (E.cy < 0 || E.cy > E.numrows){
        return;
    }
    erow row = E.row[E.cy];
    if (dir > 0){
        for (int i = editor_cx_to_index(&E) + 1; i < row.size; i++){
            if (row.chars[i] == c){
                E.cx = E.last_row_digits + i;
                return;
            }
        }
        return;
    }
    for (int i = editor_cx_to_index(&E) - 1; i > 0; i--){
        if (row.chars[i] == c){
            E.cx = E.last_row_digits + i;
            return;
        }
    }
}

/*** append buffer ***/

#define ABUF_INIT {NULL, 0}

void abAppend(struct abuf *ab, const char *s, int len){
    char *new = realloc(ab->b, ab->len + len);
    
    if (new == NULL){
        return;
    }

    memcpy(&new[ab->len], s, len);
    ab->b = new;
    ab->len += len;
}

void abFree(struct abuf *ab){
    free(ab->b);
}

/*** input ***/

char *editorPrompt(char *prompt, void (*callback)(char *, int)) {
    size_t bufsize = 128;
    char *buf = malloc(bufsize);
    size_t buflen = 0;
    buf[0] = '\0';
    while (1) {
        editorSetStatusMessage(&E, prompt, buf);
        editorRefreshScreen(&E);
        int count = 0;
        int c = editorReadKey(&count);
        if (c == DEL_KEY || c == CTRL_KEY('h') || c == BACKSPACE) {
            if (buflen != 0) buf[--buflen] = '\0';
        }else if (c == '\x1b'){
            editorSetStatusMessage(&E, "");
            if (callback){
                callback(buf, c);
            }
            free(buf);
            return NULL;
        }else if (c == '\r') {
            if (buflen != 0) {
                editorSetStatusMessage(&E, "");
                if (callback){
                    callback(buf, c);
                }
                return buf;
            }
        } else if (!iscntrl(c) && c < 128) {
            if (buflen == bufsize - 1) {
                bufsize *= 2;
                buf = realloc(buf, bufsize);
            }
            buf[buflen++] = c;
            buf[buflen] = '\0';
        }

        if (callback){
            callback(buf, c);
        }
    }
}

void editorMoveCursor(int key, int amount){
    erow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];

    switch (key) {
        case ARROW_UP:{
            if (E.cy != 0){
                E.cy = max(E.cy - amount, 0);
            }
            break;
        }
        case ARROW_DOWN:{
            if (E.cy < E.numrows){
                E.cy = min(E.cy + amount, E.numrows);
            }
            break;
        }
        case ARROW_LEFT:{
            if (E.cx != E.last_row_digits){
                E.cx = max(E.cx - amount, E.last_row_digits);
            }else if (E.cy > 0){
                E.cx = E.row[--E.cy].size + E.last_row_digits;
            }
            break;
        }
        case ARROW_RIGHT:{
            if (row && editor_cx_to_index(&E) < row->size){
                E.cx = max(E.cx + amount, E.last_row_digits);
            } else if (row && editor_cx_to_index(&E) == row->size) {
                E.cy++;
                E.cx = E.last_row_digits;
            }
            break;
        }
    }

    row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
    int rowlen = row ? row->size : 0;
    if (editor_cx_to_index(&E) > rowlen){
        E.cx = rowlen + E.last_row_digits;
    }
}

void editorMoveCursorCommand(int dir){
    if (E.cy < 0 || E.cy > E.numrows){
        return;
    }
    erow row = E.row[E.cy];
    if (dir < 0){
        for (int i = 0; i < row.size; i++){
            if (!isspace(row.chars[i])){
                E.cx = i + E.last_row_digits;
                return;
            }
        }
    }
    for (int i = row.size; i > 0; i--){
        if (!isspace(row.chars[i])){
            E.cx = i + E.last_row_digits;
            return;
        }
    }
}

void mode_function_normal(){
    int count = 0;
    int c = editorReadKey(&count);
    if (count == 0){
        count = 1;
    }
    
    switch (c) {
        case CTRL_KEY('q'):{
            if (E.dirty && E.quit_times_curr > 0) {
                editorSetStatusMessage(&E, "WARNING!!! File has unsaved changes. "
                                       "Press Ctrl-Q %d more times to quit.", E.quit_times_curr);
                E.quit_times_curr--;
                return;
            }
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
            break;
        }

        case '\r':{
            editorMoveCursor(ARROW_DOWN, 1);
            break;
        }

        case 'i':
        case 'I':{
            if (c == 'I'){
                editorMoveCursorCommand(-1);
            }
            E.mode = INSERT;
            break;
        }

        case 'a':
        case 'A':{
            E.cx ++;
            if (c == 'A'){
                editorMoveCursorCommand(1);
            }
            E.mode = INSERT;
            break;
        }

        case 'v':{
            E.mode = VISUAL;
            E.vhl_row = E.cy;
            E.vhl_start = editor_cx_to_index(&E);
            editorUpdateHighlight(&E);
            break;
        }

        case ARROW_DOWN:
        case 'j':{
            editorMoveCursorCommand_(&E, count, DOWN);
            break;
        }

        case ARROW_UP:
        case 'k':{
            editorMoveCursorCommand_(&E, count, UP);
            break;
        }

        case ARROW_LEFT:
        case 'h':{
            editorMoveCursorCommand_(&E, count, LEFT);
            break;
        }
        
        case ARROW_RIGHT:
        case 'l':{
            editorMoveCursorCommand_(&E, count, RIGTH);
            break;
        }

        case 'g':{
            int count = 0;
            c = editorReadKey(&count);
            if (c != 'g'){
                break;
            }

            editorMoveCursorCommand_(&E, count, TOP_FILE);
            break;
        }

        case 'G':{
            int count = 0;
            c = editorReadKey(&count);
            if (c != 'G'){
                break;
            }

            editorMoveCursorCommand_(&E, count, BOTTOM_FILE);
            break;
        }

        case 'o':
        case 'O':{
            if (c == 'o'){
                editorInsertNewlineCommand(1);
            }else{
                editorInsertNewlineCommand(-1);
            }
            E.mode = INSERT;
            break;
        }

        case 'f':
        case 'F':{
            EDITOR_MOTIONS dir = c == 'f' ? SEARCH_FORWARD : SEARCH_BACKWARD;
            int dummy = 0;
            c = editorReadKey(&dummy);
            if (c == '\x1b'){
                break;
            }

            editorSearchCommand_(&E, count, dir, c);
            break;
        }

        case 'x':{
            for (int i = 0; i < count; i++){
                editorMoveCursor(ARROW_RIGHT, 1);
                editorDelChar();
            }
            break;
        }

        case BACKSPACE:
        case DEL_KEY:
        case CTRL_KEY('h'):{
            editorMoveCursor(ARROW_RIGHT, 1);
            break;
        }

        case CTRL_KEY('s'):{
            editorSave();
            break;
        }

        case CTRL_KEY('l'):
        case '\x1b':{
            break;
        }

        case '/':
            editorFind();
            break;

        case HOME_KEY:
        case '0':{
            editorMoveCursorCommand_(&E, count, START_LINE);
            break;
        }

        case END_KEY:
        case '$':{
            editorMoveCursorCommand_(&E, count, END_LINE);
            break;
        }

        case PAGE_UP:
        case PAGE_DOWN:{
            if (c == PAGE_UP) {
                E.cy = E.rowoff;
            } else if (c == PAGE_DOWN) {
                E.cy = E.rowoff + E.screenrows - 1;
                if (E.cy > E.numrows){
                    E.cy = E.numrows;
                }
            }

            {
                int times = E.screenrows;
                while (times--){
                    editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN, 1);
                }
            }
            break;
        }

        case 'u':{
            // TODO: undo
            break;
        }

        case CTRL_KEY('r'):{
            // TODO: redo
            break;
        }

        case 'y':{
            int dummy = 0;
            int c = editorReadKey(&dummy);
            if (c == 'y'){
                editorYankCommand_(&E, count, YANK_LINE);
            }else{
                editorYankCommand_(&E, count, YANK);
            }
            break;
        }

        case 'p':{
            editorPasteCommand_(&E, count, PASTE);
            break;
        }

        default:{
            break;
        }
    }

    E.quit_times_curr = E.quit_times;
}

void mode_function_insert(){
    int count = 0;
    int c = editorReadKey(&count);

    switch (c) {
        case CTRL_KEY('q'):{
            if (E.dirty && E.quit_times_curr > 0) {
                editorSetStatusMessage(&E, "WARNING!!! File has unsaved changes. "
                                       "Press Ctrl-Q %d more times to quit.", E.quit_times_curr);
                E.quit_times_curr--;
                return;
            }
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
            break;
        }

        case '\r':{
            editorInsertNewline();
            break;
        }

        case BACKSPACE:
        case DEL_KEY:
        case CTRL_KEY('h'):{
            if (c == DEL_KEY){
                editorMoveCursor(ARROW_RIGHT, 1);
            }
            editorDelChar();
            break;
        }

        case CTRL_KEY('c'):{
            editorYankCommand_(&E, count, YANK);
            break;
        }

        case CTRL_KEY('v'):{
            editorPasteCommand_(&E, count, PASTE);
            break;
        }

        case CTRL_KEY('s'):{
            editorSave();
            break;
        }

        case CTRL_KEY('l'):
        case '\x1b':{
            E.mode = NORMAL;
            break;
        }

        case CTRL_KEY('f'):
            editorFind();
            break;

        case HOME_KEY:{
            E.cx = E.last_row_digits;
            break;
        }
        case END_KEY:{
            if (E.cy < E.numrows){
                E.cx = max(E.row[E.cy].size, E.last_row_digits);
            }
            break;
        }

        case PAGE_UP:
        case PAGE_DOWN:{
            if (c == PAGE_UP) {
                E.cy = E.rowoff;
            } else if (c == PAGE_DOWN) {
                E.cy = E.rowoff + E.screenrows - 1;
                if (E.cy > E.numrows){
                    E.cy = E.numrows;
                }
            }

            {
                int times = E.screenrows;
                while (times--)
                    editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN, 1);
            }
            break;
        }

        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:{
            editorMoveCursor(c, 1);
            break;
        }

        default:{
            editorInsertChar(c);
        }
    }

    E.quit_times_curr = E.quit_times;
}

void mode_function_visual(){
    int count = 0;
    int c = editorReadKey(&count);
    if (count == 0){
        count = 1;
    }   
    editorResetHighlight(&E);
    switch (c) {
        case CTRL_KEY('l'):
        case '\x1b':{
            E.mode = NORMAL;
            return;
        }

        case 'j':{
            editorMoveCursor(ARROW_DOWN, count);
            break;
        }

        case 'k':{
            editorMoveCursor(ARROW_UP, count);
            break;
        }

        case 'h':{
            editorMoveCursor(ARROW_LEFT, count);
            break;
        }

        case 'l':{
            editorMoveCursor(ARROW_RIGHT, count);
            break;
        }

        case 'v':{
            E.mode = NORMAL;
            editorResetHighlight(&E);
            editorRefreshScreen(&E);
            return;
        }

        case 'y':{
            editorYankCommand_(&E, count, YANK);
            E.mode = NORMAL;
            return;
        }

        case 'g':{
            int count = 0;
            c = editorReadKey(&count);
            if (c != 'g'){
                break;
            }

            E.cy = E.rowoff;
            {
                int times = E.screenrows;
                while (times--){
                    editorMoveCursor(ARROW_UP, 1);
                }
            }

            break;
        }

        case 'G':{
            int count = 0;
            c = editorReadKey(&count);
            if (c != 'G'){
                break;
            }

            E.cy = E.rowoff + E.screenrows - 1;
            if (E.cy > E.numrows){
                E.cy = E.numrows;
            }

            {
                int times = E.screenrows;
                while (times--){
                    editorMoveCursor(ARROW_DOWN, 1);
                }
            }

            break;
        }

        case 'f':
        case 'F':{
            int dir = c == 'f' ? 1 : -1;
            int count = 0;
            c = editorReadKey(&count);
            if (c == '\x1b'){
                break;
            }
            editorFindInRow(c, dir);
            break;
        }

        case BACKSPACE:
        case DEL_KEY:
        case CTRL_KEY('h'):{
            editorMoveCursor(ARROW_RIGHT, 1);
            break;
        }

        case CTRL_KEY('s'):{
            editorSave();
            break;
        }

        case '/':
            editorFind();
            break;

        case HOME_KEY:
        case '0':{
            E.cx = E.last_row_digits;
            break;
        }

        case END_KEY:
        case '$':{
            if (E.cy < E.numrows){
                E.cx = E.row[E.cy].size + E.last_row_digits;
            }
            break;
        }

        case PAGE_UP:
        case PAGE_DOWN:{
            if (c == PAGE_UP) {
                E.cy = E.rowoff;
            } else if (c == PAGE_DOWN) {
                E.cy = E.rowoff + E.screenrows - 1;
                if (E.cy > E.numrows){
                    E.cy = E.numrows;
                }
            }

            {
                int times = E.screenrows;
                while (times--){
                    editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN, 1);
                }
            }
            break;
        }

        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:{
            editorMoveCursor(c, count);
            break;
        }

        default:{
            break;
        }
    }

    editorUpdateHighlight(&E);
}

void editorProccessKeyPress(){
    return E.mode_functions[E.mode]();
}

/*** init ***/

void initEditor(){
    E.cy = E.numrows = E.rowoff = E.cx = E.coloff = E.rx = E.dirty = E.last_row_digits = E.vhl_start = E.vhl_row = 0;
    E.quit_times = E.quit_times_curr = 3;
    E.mode = NORMAL;
    E.row = NULL;
    E.filename = NULL;
    E.syntax = NULL;
    E.statusmsg[0] = '\0';
    E.statusmsg_time = 0;
    E.mode_functions[NORMAL] = mode_function_normal;
    E.mode_functions[INSERT] = mode_function_insert;
    E.mode_functions[VISUAL] = mode_function_visual;

    int os = detect_os();
    if (os == 0){
        die("unkown os");
    }
    E.os_type = os;
    if (getWindowSize(&E.screenrows, &E.screencolsBase) == -1){
        die("getWindowSize");
    }
    E.screenrows -= 2;
    init_kilo_config(&E);
    E.screencols = E.screencolsBase - E.last_row_digits;
}

int main(int argc, char *argv[]){
    enableRawMode();
    //enableMouse();
    initEditor();
    if (argc > 1){
        editorOpen(argv[1]);
    }

    editorSetStatusMessage(&E, "HELP: Ctrl-S = save | Ctrl-Q = quit | Ctrl-F = find");

    while (1) {
        editorRefreshScreen(&E);
        editorProccessKeyPress();
    }
    return 0;
}
