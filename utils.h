#ifndef kilo_utils_h
#define kilo_utils_h

#include "editor_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>

int editor_cx_to_index(editorConfig *config);
int max(int a, int b);
int min(int a, int b);
char *tabs_to_spaces(int tabs_count);
int count_digits(int num);
void die(const char *s);
void debug(const char *key, const char *fmt, ...);

#endif
