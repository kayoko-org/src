/*
 * Kayoko - Source Code
 * 
 * Copyright (c) 2026 The Kayoko Project. All Rights Reserved
 * 
 * This file is licensed under the Common Development and Distribution License (CDDL).
 * 
 * See /usr/src/COPYING for details.
 */

#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <locale.h>
#include <errno.h>

typedef struct {
    char *data;
} Line;

int rev = 1;
int num = 0;

void usage(const char *prog) {
    fprintf(stderr, "usage: %s [-cnru] [-o output] [file ...]\n", prog);
    exit(2);
}

int compare_lines(const void *a, const void *b) {
    const char *s1 = ((const Line *)a)->data;
    const char *s2 = ((const Line *)b)->data;
    int res = 0;

    if (num) {
        char *end1, *end2;
        double d1 = strtod(s1, &end1);
        double d2 = strtod(s2, &end2);
        res = (d1 > d2) ? 1 : (d1 < d2 ? -1 : 0);
    } else {
        res = strcoll(s1, s2);
    }
    return rev * res;
}

void load_file(FILE *fp, Line **lines, size_t *count, size_t *cap) {
    char *curr = NULL;
    size_t len = 0;
    while (getline(&curr, &len, fp) != -1) {
        if (*count >= *cap) {
            size_t new_cap = (*cap == 0) ? 1024 : *cap * 2;
            Line *temp = realloc(*lines, new_cap * sizeof(Line));
            if (!temp) {
                perror("sort: memory exhausted");
                exit(1);
            }
            *lines = temp;
            *cap = new_cap;
        }
        (*lines)[*count].data = strdup(curr);
        if (!(*lines)[*count].data) {
            perror("sort: strdup");
            exit(1);
        }
        (*count)++;
    }
    free(curr);
}

int main(int argc, char *argv[]) {
    setlocale(LC_ALL, "");

    size_t cap = 0, count = 0;
    Line *lines = NULL;
    int unique = 0, check = 0, opt;
    char *outfile = NULL;

    /* Handle unknown flags strictly */
    while ((opt = getopt(argc, argv, "cnruo:")) != -1) {
        switch (opt) {
            case 'c': check = 1; break;
            case 'n': num = 1;   break;
            case 'r': rev = -1;  break;
            case 'u': unique = 1; break;
            case 'o': outfile = optarg; break;
            default:  usage(argv[0]);
        }
    }

    /* Input Processing */
    if (optind == argc) {
        load_file(stdin, &lines, &count, &cap);
    } else {
        for (int i = optind; i < argc; i++) {
            FILE *fp = (strcmp(argv[i], "-") == 0) ? stdin : fopen(argv[i], "r");
            if (!fp) {
                fprintf(stderr, "sort: %s: %s\n", argv[i], strerror(errno));
                exit(1);
            }
            load_file(fp, &lines, &count, &cap);
            if (fp != stdin) fclose(fp);
        }
    }

    if (check) {
        for (size_t i = 1; i < count; i++) {
            if (compare_lines(&lines[i-1], &lines[i]) > 0) {
                fprintf(stderr, "sort: disorder: %s", lines[i].data);
                return 1;
            }
        }
        return 0;
    }

    qsort(lines, count, sizeof(Line), compare_lines);

    FILE *out = stdout;
    if (outfile) {
        out = fopen(outfile, "w");
        if (!out) {
            perror(outfile);
            return 1;
        }
    }

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

