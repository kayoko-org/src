#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* POSIX 2024 requires handling arbitrary line lengths and large counts */
typedef struct {
    char *data;
} Line;

int compare_lines(const void *a, const void *b) {
    return strcmp(((Line *)a)->data, ((Line *)b)->data);
}

int main(int argc, char *argv[]) {
    size_t cap = 1024;
    size_t count = 0;
    Line *lines = malloc(cap * sizeof(Line));
    int unique = 0;
    int opt;

    /* Basic POSIX flag handling */
    while ((opt = getopt(argc, argv, "u")) != -1) {
        if (opt == 'u') unique = 1;
    }

    char *curr = NULL;
    size_t len = 0;
    
    /* getline() is the POSIX way to handle arbitrary line lengths */
    while (getline(&curr, &len, stdin) != -1) {
        if (count >= cap) {
            cap *= 2;
            lines = realloc(lines, cap * sizeof(Line));
        }
        lines[count++].data = strdup(curr);
    }
    free(curr);

    qsort(lines, count, sizeof(Line), compare_lines);

    for (size_t i = 0; i < count; i++) {
        if (unique && i > 0 && strcmp(lines[i].data, lines[i-1].data) == 0) {
            free(lines[i].data);
            continue;
        }
        fputs(lines[i].data, stdout);
        free(lines[i].data);
    }

    free(lines);
    return 0;
}
