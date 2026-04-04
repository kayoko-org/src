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
#include <errno.h>

void usage(void) {
    fprintf(stderr, "usage: rmdir [-p] directory ...\n");
    exit(1);
}

int main(int argc, char *argv[]) {
    int pflag = 0;
    int ch, error = 0;

    while ((ch = getopt(argc, argv, "p")) != -1) {
        switch (ch) {
        case 'p':
            pflag = 1;
            break;
        default:
            usage();
        }
    }

    argc -= optind;
    argv += optind;

    if (argc < 1) usage();

    for (; *argv; ++argv) {
        char *path = *argv;
        
        if (rmdir(path) != 0) {
            perror(path);
            error = 1;
            continue;
        }

        if (pflag) {
            char *p;
            while ((p = strrchr(path, '/')) != NULL) {
                *p = '\0';
                if (*path == '\0') break; // Reached root
                if (rmdir(path) != 0) {
                    // POSIX says pflag should not report errors for parents 
                    // if they aren't empty, just stop.
                    break;
                }
            }
        }
    }

    return error;
}
