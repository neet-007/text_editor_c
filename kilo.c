/*** includes ***/

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

enum editorHighlight {
    HL_NORMAL = 0,
    HL_COMMENT,
    HL_MLCOMMENT,
    HL_KEYWORD1,
    HL_KEYWORD2,
    HL_STRING,
    HL_NUMBER,
    HL_MATCH,
    VHL_NORMAL,
    VHL_HIGHLIGHT,
};

#define HL_HIGHLIGHT_NUMBERS (1<<0)
#define HL_HIGHLIGHT_STRINGS (1<<1)

/*** data ***/

struct editorConfig E;

/*** filetypes ***/
char *C_HL_extensions[] = { ".c", ".h", ".cpp", NULL };
char *C_HL_keywords[] = {
  "switch", "if", "while", "for", "break", "continue", "return", "else",
  "struct", "union", "typedef", "static", "enum", "class", "case",
  "int|", "long|", "double|", "float|", "char|", "unsigned|", "signed|",
  "void|", NULL
};

struct editorSyntax HLDB[] = {
    {
        "c",
        C_HL_extensions,
        C_HL_keywords,
        "//", "/*", "*/",
        HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS
    },
};
#define HLDB_ENTRIES (sizeof(HLDB) / sizeof(HLDB[0]))

/*** prototypes ***/

struct abuf{
    char *b;
    int len;
};

void editorSetStatusMessage(const char *fmt, ...);
void editorRefreshScreen();
char *editorPrompt(char *prompt, void (*callback)(char *, int));
void abAppend(struct abuf *ab, const char *s, int len);
    
void die(const char *s){
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    perror(s);
    exit(1);
}

void enableMouse() {
    write(STDOUT_FILENO, "\033[?1003h", 8);
}

void disableMouse() {
    write(STDOUT_FILENO, "\033[?1003l", 8);
}

/*** debug ***/

void debug(const char *key, const char *fmt, ...) {
    const char *filename = "log.txt";
    char buffer[1024];
    char log_message[2048]; 
    va_list ap;

    va_start(ap, fmt);
    int n = vsnprintf(buffer, sizeof(buffer), fmt, ap);
    va_end(ap);

    if (n < 0) {
        die("Formatting error");
    } else if (n >= (int)sizeof(buffer)) {
        die("Output truncated: buffer size insufficient");
    }

    int m = snprintf(log_message, sizeof(log_message), "%s: %s\n", key, buffer);
    if (m < 0 || m >= (int)sizeof(log_message)) {
        die("Output truncated: log_message buffer size insufficient");
    }

    int fd = open(filename, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd == -1) {
        die("Failed to open the file");
    }

    ssize_t total_written = 0;
    ssize_t to_write = strlen(log_message);

    while (total_written < to_write) {
        ssize_t bytes_written = write(fd, log_message + total_written, to_write - total_written);
        if (bytes_written == -1) {
            close(fd);
            die("Failed to write to the file");
        }
        total_written += bytes_written;
    }

    if (close(fd) == -1) {
        die("Failed to close the file");
    }
}

char *tabs_to_spaces(int tabs_count){
    char *spaces = malloc(sizeof(char) * tabs_count);
    if (spaces == NULL){
        return NULL;
    }

    for (int i = 0; i < tabs_count; i++){
        spaces[i] = SPACE;
    }

    return spaces;
}

int max(int a, int b){
    return a > b ? a : b;
}

int count_digits(int num) {
    if (num == 0){
        return 1;
    }
    int count = 0;
    if (num < 0){
        num = -num;
    }

    while (num != 0) {
        num /= 10;
        count++;
    }
    return count;
}

int editor_cx_to_index(){
    return E.cx - E.last_row_digits;
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

int editorReadKey(){
    int nread;
    char c;

    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) {
            die("read");
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

/*** syntax highlighting ***/

int is_separator(int c){
    return isspace(c) || c == '\0' || strchr(",.()+-/*=~%<>[];", c) != NULL;
}

void editorUpdateHighlight() {
    int start_row = E.vhl_row <= E.cy ? E.vhl_row : E.cy;
    int end_row = E.vhl_row <= E.cy ? E.cy : E.vhl_row;

    int start_idx = (start_row == E.cy) ? editor_cx_to_index() : E.vhl_start;
    int end_idx = E.row[start_row].rsize;
    if (E.cy == E.vhl_row){
        if (editor_cx_to_index() > E.vhl_start){
            start_idx = E.vhl_start;
            end_idx = editor_cx_to_index();
        }else{
            start_idx = editor_cx_to_index();
            end_idx = E.vhl_start;
        }
        memset(&E.row[start_row].vhl[start_idx], VHL_HIGHLIGHT, end_idx - start_idx);
        return;
    }

    memset(&E.row[start_row].vhl[start_idx], VHL_HIGHLIGHT, end_idx - start_idx);

    for (int y = start_row + 1; y < end_row; y++) {
        memset(E.row[y].vhl, VHL_HIGHLIGHT, E.row[y].rsize);
    }

    end_idx = (end_row == E.cy) ? editor_cx_to_index(): E.vhl_start;
    memset(E.row[end_row].vhl, VHL_HIGHLIGHT, end_idx);
}

void editorResetHighlight(){
    int start_row = E.vhl_row <= E.cy ? E.vhl_row : E.cy;
    int end_row = E.vhl_row <= E.cy ? E.cy : E.vhl_row;

    int start_idx = (start_row == E.cy) ? editor_cx_to_index() : E.vhl_start;
    int end_idx = E.row[start_row].rsize;
    if (E.cy == E.vhl_row){
        if (editor_cx_to_index() > E.vhl_start){
            start_idx = E.vhl_start;
            end_idx = editor_cx_to_index();
        }else{
            start_idx = editor_cx_to_index();
            end_idx = E.vhl_start;
        }
        memset(&E.row[start_row].vhl[start_idx], VHL_NORMAL, end_idx - start_idx);
        return;
    }

    memset(&E.row[start_row].vhl[start_idx], VHL_NORMAL, end_idx - start_idx);

    for (int y = start_row + 1; y < end_row; y++) {
        memset(E.row[y].vhl, VHL_NORMAL, E.row[y].rsize);
    }

    end_idx = (end_row == E.cy) ? editor_cx_to_index(): E.vhl_start;
    memset(E.row[end_row].vhl, VHL_NORMAL, end_idx);
}


void editorUpdateSyntax(erow *row) {
    row->hl = realloc(row->hl, row->rsize);
    memset(row->hl, HL_NORMAL, row->rsize);

    if (E.syntax == NULL){
        return;
    }

    char **keywords = E.syntax->keywords;

    char *scs = E.syntax->singleline_comment_start;
    char *mcs = E.syntax->multiline_comment_start;
    char *mce = E.syntax->multiline_comment_end;
    
    int scs_len = (scs) ? strlen(scs) : 0;
    int mcs_len = (mcs) ? strlen(mcs) : 0;
    int mce_len = (mce) ? strlen(mce) : 0;

    int prev_sep = 1;
    int in_string = 0;
    int in_comment = (row->idx > 0 && E.row[row->idx - 1].hl_open_comment);
    int i = 0;

    while (i < row->rsize){
        int c = row->render[i];
        unsigned char prev_hl = (i > 0) ? row->hl[i - 1] : HL_NORMAL;

        if (scs_len && !in_string && !in_comment){
            if (!strncmp(&row->render[i], scs, scs_len)){
                memset(&row->hl[i], HL_COMMENT, row->rsize - i);
                break;
            }
        }

        if (mcs_len && mce_len && !in_string) {
            if (in_comment) {
                row->hl[i] = HL_MLCOMMENT;
                if (!strncmp(&row->render[i], mce, mce_len)) {
                    memset(&row->hl[i], HL_MLCOMMENT, mce_len);
                    i += mce_len;
                    in_comment = 0;
                    prev_sep = 1;
                    continue;
                } else {
                    i++;
                    continue;
                }
            } else if (!strncmp(&row->render[i], mcs, mcs_len)) {
                memset(&row->hl[i], HL_MLCOMMENT, mcs_len);
                i += mcs_len;
                in_comment = 1;
                continue;
            }
        }

        if (E.syntax->flags & HL_HIGHLIGHT_STRINGS){
            if (in_string){
                if (c == '\\' && i + 1 < row->rsize) {
                    row->hl[i + 1] = HL_STRING;
                    i += 2;
                    continue;
                }
                row->hl[i] = HL_STRING;
                if (c == in_string){
                    in_string = 0;
                }
                i++;
                prev_sep = 1;
                continue;
            }else{
                if (c == '"' || c == '\''){
                    in_string = c;
                    row->hl[i] = HL_STRING;
                    i++;
                    continue;
                }
            }
        }
        if (E.syntax->flags & HL_HIGHLIGHT_NUMBERS){
            if (((isdigit(c)) && (prev_sep || prev_hl == HL_NUMBER)) || ((c == '.') && prev_hl == HL_NUMBER)) {
                row->hl[i] = HL_NUMBER;
                prev_sep = 0;
                i++;
                continue;
            }
        }

        if (prev_sep) {
            int j;
            for (j = 0; keywords[j]; j++) {
                int klen = strlen(keywords[j]);
                int kw2 = keywords[j][klen - 1] == '|';
                if (kw2) {
                    klen--;
                }
                if (!strncmp(&row->render[i], keywords[j], klen) && is_separator(row->render[i + klen])) {
                    memset(&row->hl[i], kw2 ? HL_KEYWORD2 : HL_KEYWORD1, klen);
                    i += klen;
                    break;
                }
            }
            if (keywords[j] != NULL) {
                prev_sep = 0;
                continue;
            }
        }

        prev_sep = is_separator(c);
        i++;
    }

    int changed = (row->hl_open_comment != in_comment);
    row->hl_open_comment = in_comment;
    if (changed && row->idx + 1 < E.numrows){
        editorUpdateSyntax(&E.row[row->idx + 1]);
    }
}

int editorSyntaxToColor(int hl){
    switch (hl) {
        case HL_NUMBER:{
            return 31;
        }
        case HL_COMMENT:
        case HL_MLCOMMENT:{
            return 36;
        }
        case HL_KEYWORD1:{
            return 33;
        }
        case HL_KEYWORD2:{
            return 32;
        }
        case HL_STRING:{
            return 35;
        }
        case HL_MATCH:{
            return 34;
        }
        default:{
            return 37;
        }
    }
}

int editorHighlightToColor(int vhl, int *palette, int *index_in_palette){
    switch (vhl) {
        case VHL_NORMAL:{
            return 49;
        }
        case VHL_HIGHLIGHT:{
            *palette = 5;
            *index_in_palette = 240;
            return 48;
        }

        default:{
            die("invalid color");
            return -1;
        }
    }
}

void editorSelectSyntaxHighlight() {
    E.syntax = NULL;
    if (E.filename == NULL || !E.syntax_flag){
        return;
    }

    char *ext = strrchr(E.filename, '.');
    for (unsigned int j = 0; j < HLDB_ENTRIES; j++) {
        struct editorSyntax *s = &HLDB[j];
        unsigned int i = 0;
        while (s->filematch[i]) {
            int is_ext = (s->filematch[i][0] == '.');
            if ((is_ext && ext && !strcmp(ext, s->filematch[i])) ||
                (!is_ext && strstr(E.filename, s->filematch[i]))) {
                E.syntax = s;

                int filerow;
                for (filerow = 0; filerow < E.numrows; filerow++) {
                    editorUpdateSyntax(&E.row[filerow]);
                }
                return;
            }
            i++;
        }
    }
}

/*** row operations ***/

int editorRowCxToRx(erow *row, int cx) {
    int rx = 0;
    int j;
    for (j = 0; j < cx; j++) {
        if (row->chars[j] == '\t'){
            rx += (E.indent_amount - 1) - (rx % E.indent_amount);
        }
        rx++;
    }
    return rx;
}

int editorRowRxToCx(erow *row, int rx){
    int cur_rx = 0;
    int cx;

    for (cx = 0; cx < row->size; cx++){
        if (row->chars[cx] == '\t'){
            cur_rx += (E.indent_amount - 1) - (cur_rx % E.indent_amount);
        }
        cur_rx++;
    }

    if (cur_rx > rx){
        return cx + E.last_row_digits;
    }

    return cx + E.last_row_digits;
}

void editorUpdateRow(erow *row){
    int tabs = 0;
    int j;
    for (j = 0; j < row->size; j++){
        if (row->chars[j] == '\t'){
            tabs++;
        }
    }

    free(row->render);
    row->render = malloc(row->size + tabs*(E.indent_amount - 1) + 1);

    int idx = 0;
    for (j = 0; j < row->size; j++){
        if (row->chars[j] == '\t') {
            row->render[idx++] = ' ';
            while (idx % E.indent_amount != 0) row->render[idx++] = ' ';
        } else {
            row->render[idx++] = row->chars[j];
        }
    }

    row->render[idx] = '\0';
    row->rsize = idx;

    if (row->vhl != NULL){
        free(row->vhl);
    }
    row->vhl = (unsigned char *)malloc(sizeof(char) * row->rsize);
    if (row->vhl == NULL){
        die("update row");
    }
    memset(row->vhl, VHL_NORMAL, row->rsize);
    editorUpdateSyntax(row);
}

void editorInsertRow(int at, char *s, size_t len){
    if (at < 0 || at > E.numrows) {
        return;
    }
    E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));
    if (E.row == NULL){
        die("editore append row");
    }
    memmove(&E.row[at + 1], &E.row[at], sizeof(erow) * (E.numrows - at));

    for (int j = at + 1; j <= E.numrows; j++) {
        E.row[j].idx++;
    }

    E.row[at].idx = at;

    E.row[at].size = len;
    E.row[at].chars = malloc(len + 1);
    if (E.row[at].chars == NULL){
        die("editore append row");
    }

    memcpy(E.row[at].chars, s, len);
    E.row[at].chars[len] = '\0';

    E.row[at].rsize = 0;
    E.row[at].render = NULL;
    E.row[at].hl = NULL;
    E.row[at].vhl = NULL;
    E.row[at].hl_open_comment = 0;

    editorUpdateRow(&E.row[at]);

    E.numrows++;
    if (E.line_numbers){
        E.last_row_digits = count_digits(E.numrows) + 1;
    }else{
        E.last_row_digits = 0;
    }
    E.dirty++;
}

void editorFreeRow(erow *row){
    free(row->chars);
    free(row->render);
    free(row->hl);
    free(row->vhl);
}

void editorDelRow(int at){
    if (at < 0 || at >= E.numrows){
        return;
    }

    editorFreeRow(&E.row[at]);
    memmove(&E.row[at], &E.row[at + 1], sizeof(erow) * (E.numrows - at - 1));

    for (int j = at; j < E.numrows - 1; j++) {
        E.row[j].idx--;
    }

    E.numrows--;
    if (E.line_numbers){
        E.last_row_digits = count_digits(E.numrows) + 1;
    }else{
        E.last_row_digits = 0;
    }
    E.dirty++;
}

void editorRowInsertChar(erow *row, int at, int c){
    if (at < 0 || at > row->size){
        at = row->size;
    }
    row->chars = realloc(row->chars, row->size + 2);
    memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
    row->size++;
    row->chars[at] = c;

    editorUpdateRow(row);
    E.dirty++;
}

void editorRowAppendString(erow *row, char* s, size_t len){
    row->chars = realloc(row->chars, row->size + len + 1);
    memcpy(&row->chars[row->size], s, len);
    row->size += len;
    row->chars[row->size] = '\0';
    editorUpdateRow(row);
    E.dirty++;
}

void editorRowDelChar(erow *row, int at){
    if (at < 0 || at >= row->size){
        return;
    }
    memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
    row->size--;
    editorUpdateRow(row);
    E.dirty++;
}

/*** editor operations ***/

void editorInsertChar(int c){
    if (E.cy == E.numrows){
        editorInsertRow(E.numrows, "", 0);
    }
    if (E.indent == SPACE && c == '\t'){
        for (int i = 0; i < E.indent_amount; i++){
            editorRowInsertChar(&E.row[E.cy], editor_cx_to_index(), SPACE);
            E.cx++;
        }
    }else{
        editorRowInsertChar(&E.row[E.cy], editor_cx_to_index(), c);
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
            editorInsertRow(++E.cy, buf, indent_count);
        }else{
            editorInsertRow(E.cy, buf, indent_count);
        }
        E.cx = E.last_row_digits + indent_count;
        return;
    }

    if (dir > 0){
        editorInsertRow(++E.cy, "", 0);
    }else{
        editorInsertRow(E.cy, "", 0);
    }
    E.cx = E.last_row_digits;
}

void editorInsertNewline(){
    if (E.cx == E.last_row_digits){
        editorInsertRow(E.cy, "", 0);
        E.cy++;
        E.cx = E.last_row_digits;
    }else{
        erow *row = &E.row[E.cy];
        int indent_count = editorCountIndent(row);
        if (indent_count > 0){
            char *buf = malloc(sizeof(char) * (indent_count + row->size - editor_cx_to_index() + 1));

            int i;
            for (i = 0; i < indent_count; i++) {
                buf[i] = E.indent;
            }

            buf[i] = '\0';

            strcat(buf, &row->chars[editor_cx_to_index()]);
            editorInsertRow(E.cy + 1, buf, indent_count + row->size - editor_cx_to_index());
        }else {
            editorInsertRow(E.cy + 1, &row->chars[editor_cx_to_index()], row->size - editor_cx_to_index());
        }
        row = &E.row[E.cy];
        row->size = editor_cx_to_index();
        row->chars[row->size] = '\0';
        E.cy++;
        E.cx = indent_count + E.last_row_digits;
        editorUpdateRow(row);
    }
}

void editorDelChar(){
    if (E.cy == E.numrows || (E.cx == E.last_row_digits && E.cy == 0)){
        return;
    }

    erow *row = &E.row[E.cy];
    if (E.cx > E.last_row_digits){
        E.cx = max(E.cx - 1, E.last_row_digits);
        editorRowDelChar(row, editor_cx_to_index());
    }else{
        E.cx = E.row[E.cy - 1].size + E.last_row_digits;
        editorRowAppendString(&E.row[E.cy - 1], row->chars, row->size);
        editorDelRow(E.cy);
        E.cy--;
    }
}

char *editorBufToString(){
    int start_row = E.vhl_row <= E.cy ? E.vhl_row : E.cy;
    int end_row = E.vhl_row <= E.cy ? E.cy : E.vhl_row;

    char *buf = NULL;
    int buf_size = 0;

    int start_idx = (start_row == E.cy) ? editor_cx_to_index() : E.vhl_start;
    int end_idx = E.row[start_row].rsize;
    if (start_row == end_row) {
        if (editor_cx_to_index() > E.vhl_start){
            start_idx = E.vhl_start;
            end_idx = editor_cx_to_index();
        }else{
            start_idx = editor_cx_to_index();
            end_idx = E.vhl_start;
        }
        buf_size = (end_idx - start_idx) + 2;
        buf = malloc(buf_size);
        if (!buf) {
            perror("Malloc failed");
            exit(EXIT_FAILURE);
        }
        strncpy(buf, &E.row[start_row].chars[start_idx], end_idx - start_idx);
        buf[end_idx - start_idx] = '\n';
        buf[end_idx - start_idx + 1] = '\0';
        return buf;
    }

    buf_size = (end_idx - start_idx) + 2;
    buf = malloc(buf_size);
    if (!buf) {
        perror("Malloc failed");
        exit(EXIT_FAILURE);
    }
    strncpy(buf, &E.row[start_row].chars[start_idx], end_idx - start_idx);
    buf[end_idx - start_idx] = '\n';
    buf[end_idx - start_idx + 1] = '\0';

    for (int y = start_row + 1; y < end_row; y++) {
        buf_size += E.row[y].size + 1;
        char *temp = realloc(buf, buf_size + 1);
        if (!temp) {
            free(buf);
            perror("Realloc failed");
            exit(EXIT_FAILURE);
        }
        buf = temp;

        strncat(buf, E.row[y].chars, E.row[y].size);
        strcat(buf, "\n");
    }

    end_idx = (end_row == E.cy) ? editor_cx_to_index() : E.vhl_start;
    buf_size += end_idx + 1;
    char *temp = realloc(buf, buf_size + 1);
    if (!temp) {
        free(buf);
        perror("Realloc failed");
        exit(EXIT_FAILURE);
    }
    buf = temp;

    strncat(buf, E.row[end_row].chars, end_idx);
    strcat(buf, "\n");

    return buf;
}

void editorCopy() {
    char *buf = editorBufToString();
    editorResetHighlight();
    editorRefreshScreen();
    FILE *fp = popen("xclip -selection clipboard", "w");
    if (fp == NULL) {
        free(buf);
        perror("Failed to run xclip");
        return;
    }

    debug("copy", "%s", buf);
    fprintf(fp, "%s", buf);
    free(buf);
    pclose(fp);
}

void editorPaste(){
    FILE *fp = popen("xclip -selection clipboard -o", "r");
    if (fp == NULL) {
        die("Failed to run xclip");
    }

    char *buffer = NULL;
    size_t size = 0;

    ssize_t read = getdelim(&buffer, &size, '\0', fp);
    if (read != -1) {
        debug("paste", "%s", buffer);
    } else {
        perror("Failed to read clipboard data");
    }

    int start = 0;
    int end = 0;
    int row_index = E.cy;
    char *first_row_start = malloc(sizeof(char) * (editor_cx_to_index() + 1));
    if (first_row_start == NULL){
        free(buffer);
        die("first editor paste");
    }
    char *first_row_end = malloc(sizeof(char) * (E.row[row_index].size - editor_cx_to_index() + 1));
    if (first_row_end == NULL){
        free(buffer);
        free(first_row_start);
        die("secnod editor paste");
    }

    strncpy(first_row_start, E.row[row_index].chars, editor_cx_to_index());
    first_row_start[editor_cx_to_index()] = '\0';
    strcpy(first_row_end, &E.row[row_index].chars[editor_cx_to_index()]);

    while (end <= size) {
        if (end == size || buffer[end] == '\n' || buffer[end] == '\r') {
            int line_len = end - start;

            if (row_index == E.cy) {
                erow *row = &E.row[row_index];
                row->size = editor_cx_to_index() + line_len;
                row->chars = realloc(row->chars, row->size + 1);
                if (!row->chars) {
                    die("editor paste");
                }

                strncpy(&row->chars[editor_cx_to_index()], &buffer[start], line_len);
                row->chars[row->size] = '\0';
                editorUpdateRow(row);
            } else {
                editorInsertRow(row_index, &buffer[start], line_len);
            }

            start = end + 1;
            row_index++;
        }
        end++;
    }

    erow *last_row = &E.row[row_index - 1];
    last_row->size += strlen(first_row_end);
    last_row->chars = realloc(last_row->chars, last_row->size + 1);
    if (!last_row->chars) {
        die("editor paste");
    }

    strcat(last_row->chars, first_row_end);
    editorUpdateRow(last_row);

    free(first_row_start);
    free(first_row_end);
    free(buffer);
    pclose(fp);
    editorRefreshScreen();
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

    editorSelectSyntaxHighlight();

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
        editorInsertRow(E.numrows, line, linelen);
    }
    free(line);
    fclose(fp);
    E.dirty = 0;
}

void editorSave(){
    if (E.filename == NULL){
        E.filename = editorPrompt("Save as: %s", NULL);
        if (E.filename == NULL) {
            editorSetStatusMessage("Save aborted");
            return;
        }
        editorSelectSyntaxHighlight();
    }


    int len;
    char *buf = editorRowsToString(&len);

    int fd = open(E.filename, O_RDWR | O_CREAT, 0644);
    if (fd == -1){
        editorSetStatusMessage("Can't save! I/O error: %s", strerror(errno));
        free(buf);
        return;
    }
    if (ftruncate(fd, len) == -1){
        editorSetStatusMessage("Can't save! I/O error: %s", strerror(errno));
        close(fd);
        free(buf);
        return;
    }

    if (write(fd, buf, len) != len){
        editorSetStatusMessage("Can't save! I/O error: %s", strerror(errno));
        close(fd);
        free(buf);
        return;
    }
    editorSetStatusMessage("%d bytes written to disk", len);
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
            E.cx = editorRowRxToCx(row, match - row->render) ;
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
        for (int i = editor_cx_to_index() + 1; i < row.size; i++){
            if (row.chars[i] == c){
                E.cx = E.last_row_digits + i;
                return;
            }
        }
        return;
    }
    for (int i = editor_cx_to_index() - 1; i > 0; i--){
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

/*** output ***/

void editorScroll() {
    E.rx = 0;
    if (E.cy < E.numrows){
        E.rx = editorRowCxToRx(&E.row[E.cy], E.cx);
    }

    if (E.cy < E.rowoff) {
        E.rowoff = E.cy;
    }
    if (E.cy >= E.rowoff + E.screenrows) {
        E.rowoff = E.cy - E.screenrows + 1;
    }
    if (E.rx < E.coloff) {
        E.coloff = E.rx;
    }
    if (E.rx >= E.coloff + E.screencols) {
        E.coloff = E.rx - E.screencols + 1;
    }
}

void editorDrawRows(struct abuf *ab){
    int y;
    for (y = 0; y < E.screenrows; y++){
        int filerow = y + E.rowoff;
        if (filerow >= E.numrows){
            if (E.numrows == 0 && y == E.screenrows / 3){
                char welcome[80];
                int welcomelen = snprintf(welcome, sizeof(welcome),
                                          "Kilo editor -- version %s", KILO_VERSION);
                if (welcomelen > E.screencols) {
                    welcomelen = E.screencols;
                }
                int padding = (E.screencols - welcomelen) / 2;
                if (padding){
                    abAppend(ab, "~", 1);
                    padding--;
                }
                while (padding--) {
                    abAppend(ab, " ", 1);
                }
                abAppend(ab, welcome, welcomelen);
            }else{
                abAppend(ab, "~", 1);
            }
        }else{
            if (E.line_numbers){
                int curr = count_digits(E.row[filerow].idx + 1);
                char *index = malloc(sizeof(char) * curr + 1);
                if (index == NULL){
                    die("draw rows");
                }
                index[curr] = '\0';
                snprintf(index, curr + 1, "%d", E.row[filerow].idx + 1); 
                abAppend(ab, index, curr);
                free(index);
                char *ws = malloc((sizeof(char) * (E.last_row_digits - curr)) + 1);
                if (ws == NULL){
                    die("draw rows");
                }
                int t = 0;
                while(curr + t < E.last_row_digits){
                    ws[t++] = ' ';
                }
                ws[t] = '\0';
                abAppend(ab, ws, E.last_row_digits - curr + 1);
                free(ws);
            }

            int len = E.row[filerow].rsize - E.coloff;
            if (len < 0) {
                len = 0;
            }
            if (len > E.screencols){
                len = E.screencols;
            }
            char *c = &E.row[filerow].render[E.coloff];
            unsigned char *hl = &E.row[filerow].hl[E.coloff];
            unsigned char *vhl = &E.row[filerow].vhl[E.coloff];
            if (vhl == NULL){
                die("vhl null");
            }
            for (int g = 0; g < E.row[filerow].rsize; g++){
            }
            int current_color = -1;
            int current_highlite = -1;
            int j;
            for (j = 0; j < len; j++) {
                if (vhl[j] == VHL_NORMAL){
                    if (current_highlite!= -1){
                        current_highlite= -1;
                        abAppend(ab, "\x1b[49m", 5);
                    }
                }else{
                    int palette = -1;
                    int index_in_palette = -1;
                    int h_color = editorHighlightToColor(vhl[j], &palette, &index_in_palette);
                    if (current_highlite != h_color){
                        current_highlite = h_color;
                        char buf[64];
                        if (palette != -1 || index_in_palette != -1){
                            int clen = snprintf(buf, sizeof(buf), "\x1b[%d;%d;%dm", h_color, palette, index_in_palette);
                            abAppend(ab, buf, clen);
                        }else{
                            int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", h_color);
                            abAppend(ab, buf, clen);
                        }
                    }
                }
                if (iscntrl(c[j])) {
                    char sym = (c[j] <= 26) ? '@' + c[j] : '?';
                    abAppend(ab, "\x1b[7m", 4);
                    abAppend(ab, &sym, 1);
                    abAppend(ab, "\x1b[m", 3);
                    if (current_color != -1) {
                        char buf[16];
                        int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", current_color);
                        abAppend(ab, buf, clen);
                    }
                } else if (hl[j] == HL_NORMAL) {
                    if (current_color != -1){
                        current_color = -1;
                        abAppend(ab, "\x1b[39m", 5);
                    }
                    abAppend(ab, &c[j], 1);
                } else {
                    int color = editorSyntaxToColor(hl[j]);
                    if (current_color != color){
                        current_color = color;
                        char buf[16];
                        int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", color);
                        abAppend(ab, buf, clen);
                    }
                    abAppend(ab, &c[j], 1);
                }
            }
            abAppend(ab, "\x1b[39m", 5);
            abAppend(ab, "\x1b[49m", 5);
        }

        abAppend(ab, "\x1b[K", 3);
        abAppend(ab, "\r\n", 2);
    }
}

void editorDrawStatusBar(struct abuf *ab){
    abAppend(ab, "\x1b[7m", 4);
    char status[87], rstatus[80];
    char mode[7];
    switch (E.mode) {
        case NORMAL:{
            snprintf(mode, sizeof(mode), "%s", "Normal");
            break;
        }
        case INSERT:{
            snprintf(mode, sizeof(mode), "%s", "Insert");
            break;
        }
        case VISUAL:{
            snprintf(mode, sizeof(mode), "%s", "Visual");
            break;
        }
        default:{
            die("invalid mode");
        }
    }
    int len = snprintf(status, sizeof(status), "%.20s - %d lines %s %s", E.filename ? E.filename : "[No Name]", E.numrows, E.dirty ? "(modified)" : "",
                       mode);
    int rlen = snprintf(rstatus, sizeof(rstatus), "%s | %d/%d:%d", E.syntax ? E.syntax->filetype : "no ft", E.cy + 1, E.numrows, editor_cx_to_index() + 1);
    if (len > E.screencols) {
        len = E.screencols;
    }
    abAppend(ab, status, len);

    while(len++ < E.screencols){
        if (E.screencols - len == rlen) {
            abAppend(ab, rstatus, rlen);
            break;
        }else{
            abAppend(ab, " ", 1);
        }
    }

    abAppend(ab, "\x1b[m", 3);
    abAppend(ab, "\r\n", 2);
}

void editorDrawMessageBar(struct abuf *ab) {
    abAppend(ab, "\x1b[K", 3);
    int msglen = strlen(E.statusmsg);
    if (msglen > E.screencols) msglen = E.screencols;
    if (msglen && time(NULL) - E.statusmsg_time < 5)
        abAppend(ab, E.statusmsg, msglen);
}

void editorRefreshScreen(){
    editorScroll();

    struct abuf ab = ABUF_INIT;

    abAppend(&ab, "\x1b[?25l", 6);
    abAppend(&ab, "\x1b[H", 3);

    editorDrawRows(&ab);
    editorDrawStatusBar(&ab);
    editorDrawMessageBar(&ab);
    
    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) + 1, (E.rx - E.coloff) + 1);
    abAppend(&ab, buf, strlen(buf));

    abAppend(&ab, "\x1b[?25h", 6);

    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}

void editorSetStatusMessage(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);
    va_end(ap);
    E.statusmsg_time = time(NULL);
}

/*** input ***/

char *editorPrompt(char *prompt, void (*callback)(char *, int)) {
    size_t bufsize = 128;
    char *buf = malloc(bufsize);
    size_t buflen = 0;
    buf[0] = '\0';
    while (1) {
        editorSetStatusMessage(prompt, buf);
        editorRefreshScreen();
        int c = editorReadKey();
        if (c == DEL_KEY || c == CTRL_KEY('h') || c == BACKSPACE) {
            if (buflen != 0) buf[--buflen] = '\0';
        }else if (c == '\x1b'){
            editorSetStatusMessage("");
            if (callback){
                callback(buf, c);
            }
            free(buf);
            return NULL;
        }else if (c == '\r') {
            if (buflen != 0) {
                editorSetStatusMessage("");
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

void editorMoveCursor(int key){
    erow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];

    switch (key) {
        case ARROW_UP:{
            if (E.cy != 0){
                E.cy--;
            }
            break;
        }
        case ARROW_DOWN:{
            if (E.cy < E.numrows){
                E.cy++;
            }
            break;
        }
        case ARROW_LEFT:{
            if (E.cx != E.last_row_digits){
                E.cx = max(E.cx - 1, E.last_row_digits);
            }else if (E.cy > 0){
                E.cx = E.row[--E.cy].size + E.last_row_digits;
            }
            break;
        }
        case ARROW_RIGHT:{
            if (row && editor_cx_to_index() < row->size){
                E.cx = max(E.cx + 1, E.last_row_digits);
            } else if (row && editor_cx_to_index() == row->size) {
                E.cy++;
                E.cx = E.last_row_digits;
            }
            break;
        }
    }

    row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
    int rowlen = row ? row->size : 0;
    if (editor_cx_to_index() > rowlen){
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

void mode_function_normal(int c){
    switch (c) {
        case CTRL_KEY('q'):{
            if (E.dirty && E.quit_times_curr > 0) {
                editorSetStatusMessage("WARNING!!! File has unsaved changes. "
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
            editorMoveCursor(ARROW_DOWN);
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
            E.vhl_start = editor_cx_to_index();
            editorUpdateHighlight();
            break;
        }

        case 'j':{
            editorMoveCursor(ARROW_DOWN);
            break;
        }

        case 'k':{
            editorMoveCursor(ARROW_UP);
            break;
        }

        case 'h':{
            editorMoveCursor(ARROW_LEFT);
            break;
        }
        
        case 'l':{
            editorMoveCursor(ARROW_RIGHT);
            break;
        }

        case 'g':{
            c = editorReadKey();
            if (c != 'g'){
                break;
            }

            E.cy = E.rowoff;
            {
                int times = E.screenrows;
                while (times--){
                    editorMoveCursor(ARROW_UP);
                }
            }

            break;
        }

        case 'G':{
            c = editorReadKey();
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
                    editorMoveCursor(ARROW_DOWN);
                }
            }

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
            int dir = c == 'f' ? 1 : -1;
            c = editorReadKey();
            if (c == '\x1b'){
                break;
            }
            editorFindInRow(c, dir);
            break;
        }

        case 'x':{
            editorMoveCursor(ARROW_RIGHT);
            editorDelChar();
            break;
        }

        case BACKSPACE:
        case DEL_KEY:
        case CTRL_KEY('h'):{
            editorMoveCursor(ARROW_RIGHT);
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
                    editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
                }
            }
            break;
        }

        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:{
            editorMoveCursor(c);
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
            // TODO: copy
            editorCopy();
            break;
        }

        case 'p':{
            // TODO: paste
            editorPaste();
            break;
        }

        default:{
            break;
        }
    }

    E.quit_times_curr = E.quit_times;
}

void mode_function_insert(int c){
    switch (c) {
        case CTRL_KEY('q'):{
            if (E.dirty && E.quit_times_curr > 0) {
                editorSetStatusMessage("WARNING!!! File has unsaved changes. "
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
                editorMoveCursor(ARROW_RIGHT);
            }
            editorDelChar();
            break;
        }

        case CTRL_KEY('c'):{
            editorCopy();
            break;
        }

        case CTRL_KEY('v'):{
            editorPaste();
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
                    editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
            }
            break;
        }

        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:{
            editorMoveCursor(c);
            break;
        }

        default:{
            editorInsertChar(c);
        }
    }

    E.quit_times_curr = E.quit_times;
}

void mode_function_visual(int c){
    editorResetHighlight();
    switch (c) {
        case CTRL_KEY('l'):
        case '\x1b':{
            E.mode = NORMAL;
            return;
        }

        case 'j':{
            editorMoveCursor(ARROW_DOWN);
            break;
        }

        case 'k':{
            editorMoveCursor(ARROW_UP);
            break;
        }

        case 'h':{
            editorMoveCursor(ARROW_LEFT);
            break;
        }

        case 'l':{
            editorMoveCursor(ARROW_RIGHT);
            break;
        }

        case 'y':{
            editorCopy();
            break;
        }

        default:{
            break;
        }
    }

    editorUpdateHighlight();
}

void editorProccessKeyPress(){
    int c = editorReadKey();
    
    return E.mode_functions[E.mode](c);
}

/*** init ***/

void initEditor(){
    E.cy = E.numrows = E.rowoff = E.coloff = E.rx = E.dirty = E.last_row_digits = E.vhl_start = E.vhl_row = 0;
    E.cx = 2;
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
    if (getWindowSize(&E.screenrows, &E.screencols) == -1){
        die("getWindowSize");
    }
    E.screenrows -= 2;
    init_kilo_config(&E);
}

int main(int argc, char *argv[]){
    enableRawMode();
    //enableMouse();
    initEditor();
    if (argc > 1){
        editorOpen(argv[1]);
    }

    editorSetStatusMessage("HELP: Ctrl-S = save | Ctrl-Q = quit | Ctrl-F = find");

    while (1) {
        editorRefreshScreen();
        editorProccessKeyPress();
    }
    return 0;
}
