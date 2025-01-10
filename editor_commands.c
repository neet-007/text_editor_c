#include "editor_commands.h"
#include "utils.h"
#include "screen.h"

void editorMoveCursorCommand_(editorConfig *config, int count, EDITOR_MOTIONS motion){
    erow *row = ((*config).cy >= (*config).numrows) ? NULL : &(*config).row[(*config).cy];

    switch (motion) {
        case UP:{
            if ((*config).cy != 0){
                (*config).cy = max((*config).cy - count, 0);
                if ((*config).cx > (*config).row[(*config).cy].rsize + (*config).last_row_digits){
                    (*config).cx = (*config).row[(*config).cy].rsize + (*config).last_row_digits;
                }
            }
            break;
        }

        case DOWN:{
            if ((*config).cy < (*config).numrows){
                (*config).cy = min((*config).cy + count, (*config).numrows);
                if ((*config).cx > (*config).row[(*config).cy].rsize + (*config).last_row_digits){
                    (*config).cx = (*config).row[(*config).cy].rsize + (*config).last_row_digits;
                }
            }
            break;
        }

        case LEFT:{
            if ((*config).cx > (*config).last_row_digits){
                (*config).cx = (*config).cx - count;
            }
            else if ((*config).cy > 0){
                (*config).cx = (*config).row[--(*config).cy].rsize + (*config).last_row_digits;
            }

            break;
        }

        case RIGTH:{
            if (row && editor_cx_to_index(config) < row->rsize){
                (*config).cx = (*config).cx + count;
            } else if (row && editor_cx_to_index(config) == row->rsize) {
                (*config).cy++;
                (*config).cx = (*config).last_row_digits;
            }
            break;
        }

        case START_LINE:{
            (*config).cx = (*config).last_row_digits;
            break;
        }

        case END_LINE:{
            if ((*config).cy < (*config).numrows){
                (*config).cx = (*config).row[(*config).cy].rsize + (*config).last_row_digits;
            }
            break;
        }

        case TOP_FILE:{
            (*config).cy = (*config).rowoff;
            {
                int times = (*config).screenrows;
                while (times--){
                    editorMoveCursorCommand_(config, UP, 1);
                }
            }

            break;
        }

        case BOTTOM_FILE:{
            (*config).cy = (*config).rowoff + (*config).screenrows - 1;
            if ((*config).cy > (*config).numrows){
                (*config).cy = (*config).numrows;
            }

            {
                int times = (*config).screenrows;
                while (times--){
                    editorMoveCursorCommand_(config, DOWN, 1);
                }
            }

            break;
        }

        default:{

        }
    }

    row = ((*config).cy >= (*config).numrows) ? NULL : &(*config).row[(*config).cy];
    int rowlen = row ? row->rsize : 0;
    if (editor_cx_to_index(config) > rowlen){
        (*config).cx = rowlen + (*config).last_row_digits;
    }
}

char *editorBufToString(editorConfig *config){
    int start_row = (*config).vhl_row <= (*config).cy ? (*config).vhl_row : (*config).cy;
    int end_row = (*config).vhl_row <= (*config).cy ? (*config).cy : (*config).vhl_row;

    char *buf = NULL;
    int buf_size = 0;

    int start_idx = (start_row == (*config).cy) ? editor_cx_to_index(config) : (*config).vhl_start;
    int end_idx = (*config).row[start_row].rsize;
    if (start_row == end_row) {
        if (editor_cx_to_index(config) > (*config).vhl_start){
            start_idx = (*config).vhl_start;
            end_idx = editor_cx_to_index(config);
        }else{
            start_idx = editor_cx_to_index(&(*config));
            end_idx = (*config).vhl_start;
        }
        buf_size = (end_idx - start_idx) + 2;
        buf = malloc(buf_size);
        if (!buf) {
            perror("Malloc failed");
            exit(EXIT_FAILURE);
        }
        strncpy(buf, &(*config).row[start_row].chars[start_idx], end_idx - start_idx);
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
    strncpy(buf, &(*config).row[start_row].chars[start_idx], end_idx - start_idx);
    buf[end_idx - start_idx] = '\n';
    buf[end_idx - start_idx + 1] = '\0';

    for (int y = start_row + 1; y < end_row; y++) {
        buf_size += (*config).row[y].size + 1;
        char *temp = realloc(buf, buf_size + 1);
        if (!temp) {
            free(buf);
            perror("Realloc failed");
            exit(EXIT_FAILURE);
        }
        buf = temp;

        strncat(buf, (*config).row[y].chars, (*config).row[y].size);
        strcat(buf, "\n");
    }

    end_idx = (end_row == (*config).cy) ? editor_cx_to_index(config) : (*config).vhl_start;
    buf_size += end_idx + 1;
    char *temp = realloc(buf, buf_size + 1);
    if (!temp) {
        free(buf);
        perror("Realloc failed");
        exit(EXIT_FAILURE);
    }
    buf = temp;

    strncat(buf, (*config).row[end_row].chars, end_idx);
    strcat(buf, "\n");

    return buf;
}

// TODO: this is not good change the segniture
void editorYankCommand_(editorConfig *config, int count, int motion){
    if (motion == YANK){
        char *buf = editorBufToString(config);
        editorResetHighlight(config);
        editorRefreshScreen(config);
        FILE *fp = popen("xclip -selection clipboard", "w");
        if (fp == NULL) {
            free(buf);
            perror("Failed to run xclip");
            return;
        }

        fprintf(fp, "%s", buf);
        free(buf);
        pclose(fp);
        return;
    }

    if (motion == YANK_LINE){
        if (config->cy < 0 || config->cy > config->numrows){
            return;
        }

        FILE *fp = popen("xclip -selection clipboard", "w");
        if (fp == NULL) {
            perror("Failed to run xclip");
            return;
        }

        fprintf(fp, "%s", (*config).row[(*config).cy].chars);
        pclose(fp);
        return;
    }

    if (motion == YANK_TO_END_LINE){

        return;
    }

}

void editorPasteCommand_(editorConfig *config, int count, int motion){
    FILE *fp = popen("xclip -selection clipboard -o", "r");
    if (fp == NULL) {
        die("Failed to run xclip");
    }

    char *buffer = NULL;
    size_t size = 0;

    ssize_t read = getdelim(&buffer, &size, '\0', fp);
    if (read != -1) {
        buffer[read] = '\0';
    } else {
        perror("Failed to read clipboard data");
    }

    int start = 0;
    int end = 0;
    int row_index = (*config).cy;
    char *first_row_start = malloc(sizeof(char) * (editor_cx_to_index(config) + 1));
    if (first_row_start == NULL){
        free(buffer);
        die("editor paste");
    }
    char *first_row_end = malloc(sizeof(char) * ((*config).row[row_index].size - editor_cx_to_index(config) + 1));
    if (first_row_end == NULL){
        free(buffer);
        free(first_row_start);
        die("editor paste");
    }

    strncpy(first_row_start, (*config).row[row_index].chars, editor_cx_to_index(config));
    first_row_start[editor_cx_to_index(config)] = '\0';
    strcpy(first_row_end, &(*config).row[row_index].chars[editor_cx_to_index(config)]);

    while (end <= read) {
        if (end == read || buffer[end] == '\n' || buffer[end] == '\r') {
            int line_len = end - start;

            if (row_index == (*config).cy) {
                erow *row = &(*config).row[row_index];
                row->size = editor_cx_to_index(config) + line_len;
                row->chars = realloc(row->chars, row->size + 1);
                if (!row->chars) {
                    die("editor paste");
                }

                strncpy(&row->chars[editor_cx_to_index(config)], &buffer[start], line_len);
                row->chars[row->size] = '\0';
                editorUpdateRow(config, row);
            } else {
                editorInsertRow(config, row_index, &buffer[start], line_len);
            }

            start = end + 1;
            row_index++;
        }
        end++;
    }

    erow *last_row = &(*config).row[row_index - 1];
    last_row->size += strlen(first_row_end);
    last_row->chars = realloc(last_row->chars, last_row->size + 1);
    if (!last_row->chars) {
        die("editor paste");
    }

    strcat(last_row->chars, first_row_end);
    editorUpdateRow(&(*config), last_row);

    free(first_row_start);
    free(first_row_end);
    free(buffer);
    pclose(fp);
    editorRefreshScreen(&(*config));
}

void editorSearchCommand_(editorConfig *config, int count, EDITOR_MOTIONS motion, int c){
    if ((*config).cy < 0 || (*config).cy > (*config).numrows){
        return;
    }
    erow row = (*config).row[(*config).cy];
    if (motion == SEARCH_FORWARD){
        int last = -1;
        for (int i = editor_cx_to_index(&(*config)) + 1; i < row.size; i++){
            if (row.chars[i] == c){
                if (--count > 0){
                    last = i;
                }else{
                    (*config).cx = (*config).last_row_digits + i;
                    return;
                }
            }
        }
        if (last != -1){
            (*config).cx = (*config).last_row_digits + last;
            return;
        }
        return;
    }
    int last = -1;
    for (int i = editor_cx_to_index(&(*config)) - 1; i >= 0; i--){
        if (row.chars[i] == c){
                if (--count > 0){
                    last = i;
                }else{
                    (*config).cx = (*config).last_row_digits + i;
                    return;
                }
        }
    }
    if (last != -1){
        (*config).cx = (*config).last_row_digits + last;
        return;
    }
}

void editorDeleteCommand_(editorConfig *config, int count, EDITOR_MOTIONS motion){

}

void editorChangeCaseCommand_(editorConfig *config, int count, EDITOR_MOTIONS motion){

}

void editorReplaceCommand_(editorConfig *config, int count, EDITOR_MOTIONS motion){

}
