#include "text_highlighting.h"
#include "utils.h"

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

int is_separator(int c){
    return isspace(c) || c == '\0' || strchr(",.()+-/*=~%<>[];", c) != NULL;
}

void editorUpdateHighlight(editorConfig *config) {
    int start_row = (*config).vhl_row <= (*config).cy ? (*config).vhl_row : (*config).cy;
    int end_row = (*config).vhl_row <= (*config).cy ? (*config).cy : (*config).vhl_row;

    int start_idx = (start_row == (*config).cy) ? editor_cx_to_index(config) : (*config).vhl_start;
    int end_idx = (*config).row[start_row].rsize;
    if ((*config).cy == (*config).vhl_row){
        if (editor_cx_to_index(config) > (*config).vhl_start){
            start_idx = (*config).vhl_start;
            end_idx = editor_cx_to_index(config);
        }else{
            start_idx = editor_cx_to_index(config);
            end_idx = (*config).vhl_start;
        }
        memset(&(*config).row[start_row].vhl[start_idx], VHL_HIGHLIGHT, end_idx - start_idx);
        return;
    }

    memset(&(*config).row[start_row].vhl[start_idx], VHL_HIGHLIGHT, end_idx - start_idx);
    for (int y = start_row + 1; y < end_row; y++) {
        memset((*config).row[y].vhl, VHL_HIGHLIGHT, (*config).row[y].rsize);
    }

    end_idx = (end_row == (*config).cy) ? editor_cx_to_index(config): (*config).vhl_start;
    debug("bef last emeset", "end idx %d cx %d vhl start %d", end_idx, editor_cx_to_index(config), (*config).vhl_start);
    memset((*config).row[end_row].vhl, VHL_HIGHLIGHT, end_idx);
}

void editorResetHighlight(editorConfig *config){
    int start_row = (*config).vhl_row <= (*config).cy ? (*config).vhl_row : (*config).cy;
    int end_row = (*config).vhl_row <= (*config).cy ? (*config).cy : (*config).vhl_row;

    int start_idx = (start_row == (*config).cy) ? editor_cx_to_index(config) : (*config).vhl_start;
    int end_idx = (*config).row[start_row].rsize;
    if ((*config).cy == (*config).vhl_row){
        if (editor_cx_to_index(config) > (*config).vhl_start){
            start_idx = (*config).vhl_start;
            end_idx = editor_cx_to_index(config);
        }else{
            start_idx = editor_cx_to_index(config);
            end_idx = (*config).vhl_start;
        }
        memset(&(*config).row[start_row].vhl[start_idx], VHL_NORMAL, end_idx - start_idx);
        return;
    }

    memset(&(*config).row[start_row].vhl[start_idx], VHL_NORMAL, end_idx - start_idx);

    for (int y = start_row + 1; y < end_row; y++) {
        memset((*config).row[y].vhl, VHL_NORMAL, (*config).row[y].rsize);
    }

    end_idx = (end_row == (*config).cy) ? editor_cx_to_index(config): (*config).vhl_start;
    memset((*config).row[end_row].vhl, VHL_NORMAL, end_idx);
}


void editorUpdateSyntax(editorConfig *config, erow *row) {
    row->hl = realloc(row->hl, row->rsize);
    memset(row->hl, HL_NORMAL, row->rsize);

    if ((*config).syntax == NULL){
        return;
    }

    char **keywords = (*config).syntax->keywords;

    char *scs = (*config).syntax->singleline_comment_start;
    char *mcs = (*config).syntax->multiline_comment_start;
    char *mce = (*config).syntax->multiline_comment_end;
    
    int scs_len = (scs) ? strlen(scs) : 0;
    int mcs_len = (mcs) ? strlen(mcs) : 0;
    int mce_len = (mce) ? strlen(mce) : 0;

    int prev_sep = 1;
    int in_string = 0;
    int in_comment = (row->idx > 0 && (*config).row[row->idx - 1].hl_open_comment);
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

        if ((*config).syntax->flags & HL_HIGHLIGHT_STRINGS){
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
        if ((*config).syntax->flags & HL_HIGHLIGHT_NUMBERS){
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
    if (changed && row->idx + 1 < (*config).numrows){
        editorUpdateSyntax(config, &(*config).row[row->idx + 1]);
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

void editorSelectSyntaxHighlight(editorConfig *config) {
    (*config).syntax = NULL;
    if ((*config).filename == NULL || !(*config).syntax_flag){
        return;
    }

    char *ext = strrchr((*config).filename, '.');
    for (unsigned int j = 0; j < HLDB_ENTRIES; j++) {
        struct editorSyntax *s = &HLDB[j];
        unsigned int i = 0;
        while (s->filematch[i]) {
            int is_ext = (s->filematch[i][0] == '.');
            if ((is_ext && ext && !strcmp(ext, s->filematch[i])) ||
                (!is_ext && strstr((*config).filename, s->filematch[i]))) {
                (*config).syntax = s;

                int filerow;
                for (filerow = 0; filerow < (*config).numrows; filerow++) {
                    editorUpdateSyntax(config, &(*config).row[filerow]);
                }
                return;
            }
            i++;
        }
    }
}

