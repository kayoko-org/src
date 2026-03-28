#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct { char *data; } Line;

int rev = 1;
int num = 0;

int compare_lines(const void *a, const void *b) {
    const char *s1 = ((Line *)a)->data;
    const char *s2 = ((Line *)b)->data;
    
    if (num) {
        return rev * (atof(s1) - atof(s2) > 0 ? 1 : (atof(s1) - atof(s2) < 0 ? -1 : 0));
    }
    return rev * strcmp(s1, s2);
}

void load_file(FILE *fp, Line **lines, size_t *count, size_t *cap) {
    char *curr = NULL;
    size_t len = 0;
    while (getline(&curr, &len, fp) != -1) {
        if (*count >= *cap) {
            *cap *= 2;
            *lines = realloc(*lines, *cap * sizeof(Line));
        }
        (*lines)[(*count)++].data = strdup(curr);
    }
    free(curr);
}

int main(int argc, char *argv[]) {
    size_t cap = 1024, count = 0;
    Line *lines = malloc(cap * sizeof(Line));
    int unique = 0, check = 0, merge = 0, opt;
    char *outfile = NULL;

    while ((opt = getopt(argc, argv, "nmcu ro:")) != -1) {
        switch (opt) {
            case 'n': num = 1; break;
            case 'm': merge = 1; break; /* In memory, merge/sort are effectively similar */
            case 'c': check = 1; break;
            case 'u': unique = 1; break;
            case 'r': rev = -1; break;
            case 'o': outfile = optarg; break;
        }
    }

    /* Process input operands */
    if (optind == argc) {
        load_file(stdin, &lines, &count, &cap);
    } else {
        for (int i = optind; i < argc; i++) {
            FILE *fp = (strcmp(argv[i], "-") == 0) ? stdin : fopen(argv[i], "r");
            if (!fp) continue;
            load_file(fp, &lines, &count, &cap);
            if (fp != stdin) fclose(fp);
        }
    }

    if (check) {
        for (size_t i = 1; i < count; i++) {
            if (compare_lines(&lines[i-1], &lines[i]) > 0) return 1;
        }
        return 0;
    }

    qsort(lines, count, sizeof(Line), compare_lines);

    FILE *out = outfile ? fopen(outfile, "w") : stdout;
    for (size_t i = 0; i < count; i++) {
        if (unique && i > 0 && compare_lines(&lines[i-1], &lines[i]) == 0) {
            free(lines[i].data);
            continue;
        }
        fputs(lines[i].data, out);
        free(lines[i].data);
    }

    if (outfile) fclose(out);
    free(lines);
    return 0;
}
