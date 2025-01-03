#include <time.h>
#include <termios.h>
#include "ini_parser.h"

typedef struct editorSyntax {
    char *filetype;
    char **filematch;
    char **keywords;
    char *singleline_comment_start;
    char *multiline_comment_start;
    char *multiline_comment_end;
    int flags;
} editorSyntax;

typedef struct erow{
    int idx;
    int size;
    int rsize;
    char *chars;
    char *render;
    unsigned char *hl;
    int hl_open_comment;
}erow;

typedef enum Indent{
    TAB=9,
    SPACE=32,
}Indent;

typedef struct editorConfig{
    int os_type;
    int cx, cy;
    int rowoff, coloff;
    int rx;
    int screenrows;
    int screencols;
    int numrows;
    int quit_times;
    int quit_times_curr;
    int indent_amount;
    Indent indent;
    bool line_numbers;
    bool syntax_flag;
    erow *row;
    int dirty;
    char *filename;
    char statusmsg[80];
    time_t statusmsg_time;
    struct editorSyntax *syntax;
    struct termios orig_termios;
} editorConfig;

int init_kilo_config(editorConfig* kilo_config);
