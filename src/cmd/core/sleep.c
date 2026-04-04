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
#include <ctype.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "usage: sleep seconds\n");
        return 1;
    }

    // POSIX check: Ensure it's actually a decimal integer string
    for (char *p = argv[1]; *p; p++) {
        if (!isdigit((unsigned char)*p)) {
            fprintf(stderr, "usage: sleep seconds\n");
            return 1;
        }
    }

    unsigned int seconds = (unsigned int)strtoul(argv[1], NULL, 10);

    // POSIX Utility spec: If interrupted by a caught signal, 
    // it should just stop and exit 0. 
    // The loop is great for a library, but the utility usually just calls it once.
    sleep(seconds);

    return 0;
}
