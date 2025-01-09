#include "screen.h"

void editorScroll(editorConfig *config) {
    (*config).rx = 0;
    if ((*config).cy < (*config).numrows){
        (*config).rx = editorRowCxToRx(config, &(*config).row[(*config).cy], (*config).cx);
    }

    if ((*config).cy < (*config).rowoff) {
        (*config).rowoff = (*config).cy;
    }
    if ((*config).cy >= (*config).rowoff + (*config).screenrows) {
        (*config).rowoff = (*config).cy - (*config).screenrows + 1;
    }
    if ((*config).rx < (*config).coloff) {
        (*config).coloff = (*config).rx;
    }
    if ((*config).rx >= (*config).coloff + (*config).screencols) {
        (*config).coloff = (*config).rx - (*config).screencols + 1;
    }
}

void editorDrawRows(editorConfig *config, struct abuf *ab){
    int y;
    for (y = 0; y < (*config).screenrows; y++){
        int filerow = y + (*config).rowoff;
        if (filerow >= (*config).numrows){
            if ((*config).numrows == 0 && y == (*config).screenrows / 3){
                char welcome[80];
                int welcomelen = snprintf(welcome, sizeof(welcome),
                                          "Kilo editor -- version %s", KILO_VERSION);
                if (welcomelen > (*config).screencols) {
                    welcomelen = (*config).screencols;
                }
                int padding = ((*config).screencols - welcomelen) / 2;
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
            if ((*config).line_numbers){
                int curr = count_digits((*config).row[filerow].idx + 1);
                char *index = malloc(sizeof(char) * curr + 1);
                if (index == NULL){
                    die("draw rows");
                }
                index[curr] = '\0';
                snprintf(index, curr + 1, "%d", (*config).row[filerow].idx + 1); 
                abAppend(ab, index, curr);
                free(index);
                char *ws = malloc((sizeof(char) * ((*config).last_row_digits - curr)) + 1);
                if (ws == NULL){
                    die("draw rows");
                }
                int t = 0;
                while(curr + t < (*config).last_row_digits){
                    ws[t++] = ' ';
                }
                ws[t] = '\0';
                abAppend(ab, ws, (*config).last_row_digits - curr + 1);
                free(ws);
            }

            int len = (*config).row[filerow].rsize - (*config).coloff;
            if (len < 0) {
                len = 0;
            }
            if (len > (*config).screencols){
                len = (*config).screencols;
            }
            char *c = &(*config).row[filerow].render[(*config).coloff];
            unsigned char *hl = &(*config).row[filerow].hl[(*config).coloff];
            unsigned char *vhl = &(*config).row[filerow].vhl[(*config).coloff];
            if (vhl == NULL){
                die("vhl null");
            }
            for (int g = 0; g < (*config).row[filerow].rsize; g++){
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

void editorDrawStatusBar(editorConfig *config, struct abuf *ab){
    abAppend(ab, "\x1b[7m", 4);
    char status[87], rstatus[80];
    char mode[7];
    switch ((*config).mode) {
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
    int len = snprintf(status, sizeof(status), "%.20s - %d lines %s %s", (*config).filename ? (*config).filename : "[No Name]", (*config).numrows, (*config).dirty ? "(modified)" : "",
                       mode);
    int rlen = snprintf(rstatus, sizeof(rstatus), "%s | %d/%d:%d", (*config).syntax ? (*config).syntax->filetype : "no ft", (*config).cy + 1, (*config).numrows, editor_cx_to_index(config) + 1);
    if (len > (*config).screencols) {
        len = (*config).screencols;
    }
    abAppend(ab, status, len);

    while(len++ < (*config).screencols){
        if ((*config).screencols - len == rlen) {
            abAppend(ab, rstatus, rlen);
            break;
        }else{
            abAppend(ab, " ", 1);
        }
    }

    abAppend(ab, "\x1b[m", 3);
    abAppend(ab, "\r\n", 2);
}

void editorDrawMessageBar(editorConfig *config, struct abuf *ab) {
    abAppend(ab, "\x1b[K", 3);
    int msglen = strlen((*config).statusmsg);
    if (msglen > (*config).screencols) msglen = (*config).screencols;
    if (msglen && time(NULL) - (*config).statusmsg_time < 5)
        abAppend(ab, (*config).statusmsg, msglen);
}

void editorRefreshScreen(editorConfig *config){
    editorScroll(config);

    struct abuf ab = ABUF_INIT;

    abAppend(&ab, "\x1b[?25l", 6);
    abAppend(&ab, "\x1b[H", 3);

    editorDrawRows(config, &ab);
    editorDrawStatusBar(config, &ab);
    editorDrawMessageBar(config, &ab);
    
    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", ((*config).cy - (*config).rowoff) + 1, ((*config).rx - (*config).coloff) + 1);
    abAppend(&ab, buf, strlen(buf));

    abAppend(&ab, "\x1b[?25h", 6);

    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}

void editorSetStatusMessage(editorConfig *config, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf((*config).statusmsg, sizeof((*config).statusmsg), fmt, ap);
    va_end(ap);
    (*config).statusmsg_time = time(NULL);
}
