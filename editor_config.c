#include "editor_config.h"
#include "hash_table.h"

char *expand_path(const char *path) {
    if (path[0] == '~') {
        const char *home = getenv("HOME");
        if (!home) {
            fprintf(stderr, "Error: HOME environment variable not set.\n");
            return NULL;
        }
        
        char *expanded_path = malloc(strlen(home) + strlen(path));
        if (!expanded_path) {
            perror("malloc");
            return NULL;
        }
        
        sprintf(expanded_path, "%s%s", home, path + 1);
        return expanded_path;
    } else {
        return strdup(path);
    }
}

HashTable *init_config(){
    HashTable *config = create_table(CAPACITY);
    if (config == NULL){
        return NULL;
    }

    int indent_amount = 8;
    char *indent = "tabs";
    bool line_numbers= "true";
    bool relative_line_numbers= "false";
    bool syntax = "true";
    int quit_times = 3;
    ht_insert(config, "indent_amount", &indent_amount, sizeof(int), TYPE_INT);
    ht_insert(config, "indent", indent, (sizeof(char) * strlen(indent)) + 1, TYPE_STR);
    ht_insert(config, "line_numbers", &line_numbers, sizeof(_Bool), TYPE_BOOL);
    ht_insert(config, "relative_line_numbers", &relative_line_numbers, sizeof(_Bool), TYPE_BOOL);
    ht_insert(config, "syntax", &syntax, sizeof(_Bool), TYPE_BOOL);
    ht_insert(config, "quit_times", &quit_times, sizeof(int), TYPE_INT);

    return config;
}

Ini *load_config(){
    char *expanded_config_path = expand_path("~/.kilorc.ini");
    if (expanded_config_path == NULL) {
        return NULL;
    }

    printf("Expanded Path: %s\n", expanded_config_path);
    Ini *config = parse_ini(expanded_config_path);
    printf("filename %s\n", config->filename);
    free(expanded_config_path);
    
    return config;
}

int init_kilo_config(editorConfig* kilo_config){
    char *included[6] = {"indent_amount", "indent", "line_numbers", "syntax", "quit_times", "relative_line_numbers"};

    HashTable *editor_config = init_config();
    if (editor_config == NULL){
        return 0;
    }
    Ini *config = load_config();
    if (config == NULL){
        free_table(editor_config);
        return 0;
    }


    Ht_item *editor_section_item = ht_search(config->sections, "editor");
    if (editor_section_item == NULL || editor_section_item->value_type != TYPE_HASH_TABLE){
        free_table(editor_config);
        free(config->filename);
        free_table(config->sections);
        free(config);
        return 0;
    }
    
    HashTable *editor_section = (HashTable *)editor_section_item->value;
    for (int i = 0; i < 6; i++){
        char *curr = included[i];

        Ht_item *editor_item = ht_search(editor_config, curr);
        if (editor_item== NULL){
            printf("editor config failed included in hashmap\n");
            free_table(editor_config);
            free(config->filename);
            free_table(config->sections);
            free(config);
            return 0;
        }

        Ht_item *curr_item = ht_search(editor_section, curr);
        if (curr_item == NULL){
            continue;
        }

        switch (editor_item->value_type) {
            case TYPE_BOOL:{
                if (strcmp((char *)curr_item->value, "true") == 0){
                    bool true_ = true;
                    ht_insert(editor_config, curr, &true_, sizeof(_Bool), TYPE_BOOL);
                }else if (strcmp((char *)curr_item->value, "false") == 0){
                    bool false_ = false;
                    ht_insert(editor_config, curr, &false_, sizeof(_Bool), TYPE_BOOL);
                }else{
                    printf("invalid value for bool\n");
                    continue;
                }
                break;
            }
            case TYPE_INT:{
                char *endptr;
                int base = 10;

                long num = strtol(curr_item->value, &endptr, base);

                if (*endptr != '\0') {
                    printf("Invalid character found: %c\n", *endptr);
                    continue;
                } 
                
                ht_insert(editor_config, curr, &num, sizeof(long), TYPE_INT);
                break;
            } 
            case TYPE_STR:{
                if (i == 1){
                    if (strcmp((char *)curr_item->value, "space") == 0){
                        ht_insert(editor_config, curr, curr_item->value, curr_item->value_size, TYPE_STR);
                        break;
                    }
                    if (strcmp((char *)curr_item->value, "tab") == 0){
                        ht_insert(editor_config, curr, curr_item->value, curr_item->value_size, TYPE_STR);
                        break;
                    }

                    printf("invalid value for %s\n", included[i]);
                    continue;
                }
                ht_insert(editor_config, curr, curr_item->value, curr_item->value_size, TYPE_STR);
                break;
            }
            case TYPE_HASH_TABLE:{
                break;
            }
            default:{
                break;
            }
        }
    }

    for (int i = 0; i < 6; i++){
        Ht_item* editor_item = ht_search(editor_config, included[i]);
        switch (i) {
            case 0:{
                kilo_config->indent_amount = (*(int *)editor_item->value);
                break;
            }
            case 1:{
                if (strcmp((char *)editor_item->value, "space") == 0){
                    kilo_config->indent = SPACE;
                }
                if (strcmp((char *)editor_item->value, "tab") == 0){
                    kilo_config->indent = TAB;
                }
                break;
            }
            case 2:{
                kilo_config->line_numbers = (*(bool *)editor_item->value);
                break;
            }
            case 3:{
                kilo_config->syntax_flag = (*(bool *)editor_item->value);
                break;
            }
            case 4:{
                kilo_config->quit_times = (*(int *)editor_item->value);
                kilo_config->quit_times_curr = (*(int *)editor_item->value);
                break;
            }
            case 5:{
                kilo_config->relative_line_numbers = (*(bool *)editor_item->value);
                if (kilo_config->relative_line_numbers){
                    kilo_config->line_numbers = true;
                }
            }
            default:{
                break;
            }
        }
    }
    if (kilo_config->line_numbers){
        kilo_config->cx = 2;
        kilo_config->last_cx = kilo_config->cx;
        kilo_config->last_row_digits = 2;
    }
    free_table(editor_config);
    free(config->filename);
    free_table(config->sections);
    free(config);

    return 1;
}
