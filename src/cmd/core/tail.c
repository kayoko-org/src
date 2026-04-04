/*
 * Kayoko - Source Code
 * 
 * Copyright (c) 2026 The Kayoko Project. All Rights Reserved
 * 
 * This file is licensed under the Common Development and Distribution License (CDDL).
 * 
 * See /usr/src/COPYING for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Simple buffer-based tail for POSIX compliance */
void copy_from_line(FILE *fp, long start_line) {
    char *line = NULL;
    size_t len = 0;
    long cur = 1;
    while (getline(&line, &len, fp) != -1) {
        if (cur >= start_line) fputs(line, stdout);
        cur++;
    }
    free(line);
}

void last_n_lines(FILE *fp, long n) {
    if (n <= 0) return;
    char **buf = calloc(n, sizeof(char *));
    char *line = NULL;
    size_t len = 0;
    long count = 0;

    while (getline(&line, &len, fp) != -1) {
        free(buf[count % n]);
        buf[count % n] = strdup(line);
        count++;
    }

    long start = (count > n) ? (count % n) : 0;
    long total = (count > n) ? n : count;

    for (long i = 0; i < total; i++) {
        fputs(buf[(start + i) % n], stdout);
        free(buf[(start + i) % n]);
    }
    free(buf);
    free(line);
}

int main(int argc, char *argv[]) {
    long n = 10;
    int plus = 0;
    int c_mode = 0;
    int opt;

    /* Manual check for the '+' prefix which getopt often eats */
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && (argv[i][1] == 'n' || argv[i][1] == 'c')) {
            if (argv[i+1] && argv[i+1][0] == '+') plus = 1;
        }
    }

    while ((opt = getopt(argc, argv, "n:c:")) != -1) {
        switch (opt) {
            case 'n': n = atol(optarg); break;
            case 'c': n = atol(optarg); c_mode = 1; break;
        }
    }

    argc -= optind;
    argv += optind;
    FILE *fp = (argc == 0) ? stdin : fopen(argv[0], "r");
    
    if (!fp) { perror("tail"); return 1; }

    if (plus) copy_from_line(fp, n);
    else last_n_lines(fp, n);

    if (fp != stdin) fclose(fp);
    return 0;
}
