#include "row.h"
#include "utils.h"

int editorRowCxToRx(editorConfig *config, erow *row, int cx) {
    int rx = config->last_row_digits;
    int j;
    for (j = 0; j < cx; j++) {
        if (row->chars[j] == '\t'){
            rx += ((*config).indent_amount - 1) - (rx % (*config).indent_amount);
        }
        rx++;
    }
    return rx;
}

int editorRowRxToCx(editorConfig *config, erow *row, int rx){
    int cur_rx = 0;
    int cx;

    for (cx = 0; cx < row->size; cx++){
        if (row->chars[cx] == '\t'){
            cur_rx += ((*config).indent_amount - 1) - (cur_rx % (*config).indent_amount);
        }
        cur_rx++;
    }

    if (cur_rx > rx){
        return cx + (*config).last_row_digits;
    }

    return cx + (*config).last_row_digits;
}

void editorUpdateRow(editorConfig *config, erow *row){
    int tabs = 0;
    int j;
    for (j = 0; j < row->size; j++){
        if (row->chars[j] == '\t'){
            tabs++;
        }
    }

    free(row->render);
    row->render = malloc(row->size + tabs*((*config).indent_amount - 1) + 1);

    int idx = 0;
    for (j = 0; j < row->size; j++){
        if (row->chars[j] == '\t') {
            row->render[idx++] = ' ';
            while (idx % (*config).indent_amount != 0) row->render[idx++] = ' ';
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
    editorUpdateSyntax(config, row);
}

void editorInsertRow(editorConfig *config, int at, char *s, size_t len){
    if (at < 0 || at > (*config).numrows) {
        return;
    }
    (*config).row = realloc((*config).row, sizeof(erow) * ((*config).numrows + 1));
    if ((*config).row == NULL){
        die("editore append row");
    }
    memmove(&(*config).row[at + 1], &(*config).row[at], sizeof(erow) * ((*config).numrows - at));

    for (int j = at + 1; j <= (*config).numrows; j++) {
        (*config).row[j].idx++;
    }

    (*config).row[at].idx = at;

    (*config).row[at].size = len;
    (*config).row[at].chars = malloc(len + 1);
    if ((*config).row[at].chars == NULL){
        die("editore append row");
    }

    memcpy((*config).row[at].chars, s, len);
    (*config).row[at].chars[len] = '\0';

    (*config).row[at].rsize = 0;
    (*config).row[at].render = NULL;
    (*config).row[at].hl = NULL;
    (*config).row[at].vhl = NULL;
    (*config).row[at].hl_open_comment = 0;

    editorUpdateRow(config, &(*config).row[at]);

    (*config).numrows++;
    if ((*config).line_numbers){
        (*config).last_row_digits = count_digits((*config).numrows) + 1;
        (*config).screencols = (*config).screencolsBase - (*config).last_row_digits;
    }else{
        (*config).last_row_digits = 0;
    }
    (*config).dirty++;
}

void editorFreeRow(erow *row){
    free(row->chars);
    free(row->render);
    free(row->hl);
    free(row->vhl);
}

void editorDelRow(editorConfig *config, int at){
    if (at < 0 || at >= (*config).numrows){
        return;
    }

    editorFreeRow(&(*config).row[at]);
    memmove(&(*config).row[at], &(*config).row[at + 1], sizeof(erow) * ((*config).numrows - at - 1));

    for (int j = at; j < (*config).numrows - 1; j++) {
        (*config).row[j].idx--;
    }

    (*config).numrows--;
    if ((*config).line_numbers){
        (*config).last_row_digits = count_digits((*config).numrows) + 1;
        (*config).screencols = (*config).screencolsBase - (*config).last_row_digits;
    }else{
        (*config).last_row_digits = 0;
    }
    (*config).dirty++;
}

void editorRowInsertChar(editorConfig *config, erow *row, int at, int c){
    if (at < 0 || at > row->size){
        at = row->size;
    }
    row->chars = realloc(row->chars, row->size + 2);
    memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
    row->size++;
    row->chars[at] = c;

    editorUpdateRow(config, row);
    (*config).dirty++;
}

void editorRowAppendString(editorConfig *config, erow *row, char* s, size_t len){
    row->chars = realloc(row->chars, row->size + len + 1);
    memcpy(&row->chars[row->size], s, len);
    row->size += len;
    row->chars[row->size] = '\0';
    editorUpdateRow(config, row);
    (*config).dirty++;
}

void editorRowDelChar(editorConfig *config, erow *row, int at){
    if (at < 0 || at >= row->size){
        return;
    }
    memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
    row->size--;
    editorUpdateRow(config, row);
    (*config).dirty++;
}
