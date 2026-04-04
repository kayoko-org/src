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
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>

typedef struct {
    long lines;
    long words;
    long bytes;
} counts_t;

static void print_counts(counts_t *c, int l, int w, int b, const char *name) {
    if (l) printf(" %7ld", c->lines);
    if (w) printf(" %7ld", c->words);
    if (b) printf(" %7ld", c->bytes);
    if (name) printf(" %s", name);
    printf("\n");
}

static void count_file(int fd, counts_t *res) {
    char buf[BUFSIZ];
    ssize_t n;
    int in_word = 0;
    res->lines = res->words = res->bytes = 0;

    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        res->bytes += n;
        for (ssize_t i = 0; i < n; i++) {
            if (buf[i] == '\n') res->lines++;
            if (isspace((unsigned char)buf[i])) {
                in_word = 0;
            } else if (!in_word) {
                in_word = 1;
                res->words++;
            }
        }
    }
}

int main(int argc, char *argv[]) {
    int do_l = 0, do_w = 0, do_c = 0, opt;
    counts_t total = {0, 0, 0}, current;
    
    while ((opt = getopt(argc, argv, "lwc")) != -1) {
        switch (opt) {
            case 'l': do_l = 1; break;
            case 'w': do_w = 1; break;
            case 'c': do_c = 1; break;
            default: return 2;
        }
    }
    /* POSIX: if no flags, default to all three */
    if (!do_l && !do_w && !do_c) do_l = do_w = do_c = 1;

    if (optind >= argc) {
        count_file(0, &current);
        print_counts(&current, do_l, do_w, do_c, NULL);
    } else {
        for (int i = optind; i < argc; i++) {
            int fd = (argv[i][0] == '-' && !argv[i][1]) ? 0 : open(argv[i], O_RDONLY);
            if (fd < 0) {
                perror(argv[i]);
                continue;
            }
            count_file(fd, &current);
            print_counts(&current, do_l, do_w, do_c, argv[i]);
            total.lines += current.lines;
            total.words += current.words;
            total.bytes += current.bytes;
            if (fd > 0) close(fd);
        }
        if (argc - optind > 1) {
            print_counts(&total, do_l, do_w, do_c, "total");
        }
    }
    return 0;
}
