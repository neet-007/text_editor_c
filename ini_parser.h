#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "hash_table.h"

typedef struct Ini{
    char *filename;
    HashTable *sections;
}Ini;

Ini *parse_ini(const char *filename);
int parse_key_val(FILE *file, int *key_len, int *val_len, char **key, char **val);
char *parse_section(FILE *file, int *len);
char *parse_comment(FILE *file, int *len);
