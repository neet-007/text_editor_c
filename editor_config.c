#include "ini_parser.h"

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

int main(){
    char *expanded_config_path = expand_path("~/.kilorc.ini");
    if (expanded_config_path == NULL) {
        return 1;
    }

    printf("Expanded Path: %s\n", expanded_config_path);
    Ini *config = parse_ini(expanded_config_path);
    printf("filename %s\n", config->filename);
    print_table(config->sections, 0);
    free(expanded_config_path);
    free(config->filename);
    free_table(config->sections);
    free(config);
}
