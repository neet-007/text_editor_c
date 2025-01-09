#ifndef KILO_SCREEN_H
#define KILO_SCREEN_H

#define KILO_VERSION "0.0.1"

#include "append_buffer.h"
#include "row.h"

void editorScroll(editorConfig *config);

void editorDrawRows(editorConfig *config, struct abuf *ab);

void editorDrawStatusBar(editorConfig *config, struct abuf *ab);

void editorDrawMessageBar(editorConfig *config, struct abuf *ab);

void editorRefreshScreen(editorConfig *config);

void editorSetStatusMessage(editorConfig *config, const char *fmt, ...);

#endif
