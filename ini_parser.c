#include "ini_parser.h"
#include <stdio.h>
#include <stdlib.h>

int valid_char(char c){
    return (isalnum(c) || c == '_' || c == '-' || c == ' ' || c == '\t');
}

char *strip_str(char *str) {
    char *p = str + strlen(str);
    while (p > str && isspace((unsigned char)(*(p - 1)))) {
        if (*(p - 1) == '\n' || *(p - 1) == '\r') {
            break; 
        }
        *--p = '\0';
    }

    while (*str != '\0' && isspace((unsigned char)(*str)) && *str != '\n' && *str != '\r') {
        str++;
    }

    return str;
}

char *parse_comment(FILE *file, int *len){
    char buf;   
    ssize_t buf_read;

    int size = 128;
    char *comment = malloc(sizeof(char) * size);
    if (comment == NULL){
        return NULL;
    }

    while ((buf = fgetc(file)) != EOF) {
        switch (buf) {
            case '\r':
            case '\n':{
                if (*len >= size){
                    size += 2;
                    comment = realloc(comment, sizeof(char) * size);
                    if (comment == NULL){
                        return NULL;
                    }
                }
                comment[(*len)++] = buf;
                comment[(*len)] = '\0';
                return comment;
            }

            default:{
                if (*len >= size){
                    size *= 2;
                    comment = realloc(comment, sizeof(char) * size);
                    if (comment == NULL){
                        return NULL;
                    }
                }
                comment[(*len)++] = buf;
            }
        }
    }

    return NULL;
}

char *parse_section(FILE *file, int *len){
    char buf;   
    ssize_t buf_read;

    int size = 128;
    char *section = malloc(sizeof(char) * size);
    if (section == NULL){
        return NULL;
    }
    while ((buf = fgetc(file)) != EOF) {
        switch (buf) {
            case '#':
            case ';':
            case '[':{
                free(section);
                return NULL;
            }

            case ']':{
                if (*len >= size){
                    size += 2;
                    section =realloc(section, size);
                    if (section == NULL){
                        return NULL;
                    }

                }
                section[(*len)++] = '\n';
                section[(*len)] = '\0';
                return strip_str(section);
            }

            default:{
                if (!valid_char(buf)){
                    free(section);
                    return NULL;
                }
                if (*len >= size){
                    size *= 2;
                    section = realloc(section, size);
                    if (section == NULL){
                        return NULL;
                    }
                }
                section[(*len)++] = buf;
            }
        }       
    }
    return NULL;
}

int parse_key_val(FILE *file, int *key_len, int *val_len, char **key, char **val) {
    char buf;
    int is_key = 1;
    int key_size = 128;
    int val_size = 128;

    int ret = fseek(file, -1, SEEK_CUR);
    if (ret != 0){
        return 0;
    }

    *key = malloc(sizeof(char) * key_size);
    if (*key == NULL) {
        return 0;
    }

    *val = malloc(sizeof(char) * val_size);
    if (*val == NULL) {
        free(*key);
        return 0;
    }

    char q = -1;
    while ((buf = fgetc(file)) != EOF) {
        switch (buf) {
            case '"':
            case '\'':{
                if (q == -1){
                    q = buf;
                }else if (q != buf){
                    free(*key);
                    free(*val);
                    return 0;
                }else{
                    q = -1;
                }

                break;
            }

            case ']': {
                free(*key);
                free(*val);
                return 0;
            }

            case '#':
            case ';':
            case '\n':
            case '\r': {
                if (q != -1){
                    free(*key);
                    free(*val);
                    return 0;
                }
                if (buf == '#' || buf == ';'){
                    int comment_len = 0;
                    char *comment = parse_comment(file, &comment_len);
                    if (comment == NULL){
                        free(*key);
                        free(*val);
                        return 0;
                    }
                    printf("found inlince comment %s\n", comment);
                    free(comment);
                }
                if (*key_len >= key_size){
                    *key = realloc(*key, ++key_size);
                    if (*key == NULL){
                        free(*val);
                        return 0;
                    }
                }
                (*key)[*key_len] = '\0';

                if (*val_len >= val_size){
                    val_size += 2;
                    *val= realloc(*val, val_size);
                    if (*val == NULL){
                        free(*key);
                        return 0;
                    }
                }
                (*val)[(*val_len)++] = '\n';
                (*val)[*val_len] = '\0';

                *key = strip_str(*key);
                *val= strip_str(*val);
                return 1;
            }

            case '=': {
                if (q != -1){
                    free(*key);
                    free(*val);
                    return 0;
                }
                is_key = 0;
                q = -1;
                break;
            }

            default: {
                if (is_key) {
                    if (*key_len >= key_size) {
                        *key = realloc(*key, ++key_size);
                        if (*key == NULL) {
                            free(*val);
                            return 0;
                        }
                    }
                    (*key)[(*key_len)++] = buf;
                } else {
                    if (*val_len >= val_size) {
                        *val = realloc(*val, ++val_size);
                        if (*val == NULL) {
                            free(*key);
                            return 0;
                        }
                    }
                    (*val)[(*val_len)++] = buf;
                }
                break;
            }
        }
    }

    free(*key);
    free(*val);
    return 0;
}

int parse_ini(const char *filename){
    FILE *file = fopen(filename, "r");
    if (!file){
        return 0;
    }
    
    char *current_section = NULL;
    char buf;   
    ssize_t buf_read;

    while ((buf = fgetc(file)) != EOF) {
        switch (buf) {
            case '\n':
            case '\r':{
                break;
            }

            case '#':
            case ';':{
                int len = 0;
                char *comment = parse_comment(file, &len);
                if (comment == NULL){
                    fclose(file);
                    return 0;
                }
                printf("comment:%s", comment);
                break;
            }

            case '[':{
                int len = 0;
                char *section = parse_section(file, &len);
                if (section == NULL){
                    fclose(file);
                    return 0;
                }
                current_section = section;
                printf("section:%s", section);
                break;
            }

            default:{
                if (current_section == NULL){
                    return 0;
                }
                char *key;
                char *val;
                int key_len = 0;
                int val_len = 0;
                int res = parse_key_val(file, &key_len, &val_len, &key, &val);
                if (!res){
                    return 0;
                }
                printf("key->%s: val ->%s", key, val);
                break;
            }
        }       
    }

    fclose(file);
    return 1;
}

int main(int argc, char*argv[]){
    if (argc < 2){
        printf("must provied path\n");
        return 1;
    }

    int res = parse_ini(argv[1]);
    if (!res){
        printf("failed\n");
    }
    return 0;
}
