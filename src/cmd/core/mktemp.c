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
#include <unistd.h>
#include <string.h>

int main(int argc, char *argv[]) {
    int dflag = 0;
    char *template;
    int opt;

    while ((opt = getopt(argc, argv, "d")) != -1) {
        if (opt == 'd') dflag = 1;
        else return 1;
    }

    if (optind < argc) {
        template = argv[optind];
    } else {
        template = strdup("/tmp/fileXXXXXX");
    }

    if (dflag) {
        if (!mkdtemp(template)) {
            perror("mktemp");
            return 1;
        }
    } else {
        int fd = mkstemp(template);
        if (fd < 0) {
            perror("mktemp");
            return 1;
        }
        close(fd);
    }

    puts(template);
    return 0;
}
