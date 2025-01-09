#ifndef KILO_AB_BUFFER_H
#define KILO_AB_BUFFER_H

#define ABUF_INIT {NULL, 0}

#include <stdlib.h>
#include <string.h>

struct abuf {
  char *b;
  int len;
};

void abAppend(struct abuf *ab, const char *s, int len);
void abFree(struct abuf *ab);

#endif
