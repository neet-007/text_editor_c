#ifndef KILO_ROW_H
#define KILO_ROW_H

#include "text_highlighting.h"

int editorRowCxToRx(editorConfig *config, erow *row, int cx);
int editorRowRxToCx(editorConfig *config, erow *row, int rx);
void editorUpdateRow(editorConfig *config, erow *row);
void editorInsertRow(editorConfig *config, int at, char *s, size_t len);
void editorFreeRow(erow *row);
void editorDelRow(editorConfig *config, int at);
void editorRowInsertChar(editorConfig *config, erow *row, int at, int c);
void editorRowAppendString(editorConfig *config, erow *row, char* s, size_t len);
void editorRowDelChar(editorConfig *config, erow *row, int at);

#endif
