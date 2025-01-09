#include "utils.h"

int editor_cx_to_index(editorConfig *config){
    return (*config).cx - (*config).last_row_digits;
}

int max(int a, int b){
    return a > b ? a : b;
}

int min(int a, int b){
    return a > b ? b : a;
}

char *tabs_to_spaces(int tabs_count){
    char *spaces = malloc(sizeof(char) * tabs_count);
    if (spaces == NULL){
        return NULL;
    }

    for (int i = 0; i < tabs_count; i++){
        spaces[i] = SPACE;
    }

    return spaces;
}

int count_digits(int num) {
    if (num == 0){
        return 1;
    }
    int count = 0;
    if (num < 0){
        num = -num;
    }

    while (num != 0) {
        num /= 10;
        count++;
    }
    return count;
}

void die(const char *s){
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    perror(s);
    exit(1);
}

void debug(const char *key, const char *fmt, ...) {
    const char *filename = "log.txt";
    char buffer[1024];
    char log_message[2048]; 
    va_list ap;

    va_start(ap, fmt);
    int n = vsnprintf(buffer, sizeof(buffer), fmt, ap);
    va_end(ap);

    if (n < 0) {
        die("Formatting error");
    } else if (n >= (int)sizeof(buffer)) {
        die("Output truncated: buffer size insufficient");
    }

    int m = snprintf(log_message, sizeof(log_message), "%s: %s\n", key, buffer);
    if (m < 0 || m >= (int)sizeof(log_message)) {
        die("Output truncated: log_message buffer size insufficient");
    }

    int fd = open(filename, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd == -1) {
        die("Failed to open the file");
    }

    ssize_t total_written = 0;
    ssize_t to_write = strlen(log_message);

    while (total_written < to_write) {
        ssize_t bytes_written = write(fd, log_message + total_written, to_write - total_written);
        if (bytes_written == -1) {
            close(fd);
            die("Failed to write to the file");
        }
        total_written += bytes_written;
    }

    if (close(fd) == -1) {
        die("Failed to close the file");
    }
}
