#ifndef KILO_TEXT_HIGHLIGHTING_H
#define KILO_TEXT_HIGHLIGHTING_H
#include "utils.h"

#define HL_HIGHLIGHT_NUMBERS (1<<0)
#define HL_HIGHLIGHT_STRINGS (1<<1)

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

int is_separator(int c);

void editorUpdateHighlight(editorConfig *config) ;

void editorResetHighlight(editorConfig *config);

void editorUpdateSyntax(editorConfig *config, erow *row);

int editorSyntaxToColor(int hl);

int editorHighlightToColor(int vhl, int *palette, int *index_in_palette);

void editorSelectSyntaxHighlight(editorConfig *config);

#endif
