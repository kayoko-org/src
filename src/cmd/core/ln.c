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
#include <libgen.h>
#include <sys/stat.h>

int main(int argc, char *argv[]) {
    int opt;
    int sflag = 0, fflag = 0;
    char path[1024];

    while ((opt = getopt(argc, argv, "sf")) != -1) {
        switch (opt) {
            case 's': sflag = 1; break;
            case 'f': fflag = 1; break;
            default:
                fprintf(stderr, "usage: ln [-sf] source_file target_file\n");
                return 1;
        }
    }

    argc -= optind;
    argv += optind;

    if (argc < 1) {
        fprintf(stderr, "usage: ln [-sf] source_file target_file\n");
        return 1;
    }

    const char *source = argv[0];
    const char *target = (argc > 1) ? argv[1] : ".";

    struct stat st;
    /* If target is a directory, append source's basename to target path */
    if (stat(target, &st) == 0 && S_ISDIR(st.st_mode)) {
        char *src_copy = strdup(source);
        snprintf(path, sizeof(path), "%s/%s", target, basename(src_copy));
        target = path;
        free(src_copy);
    }

    if (fflag) {
        unlink(target);
    }

    int (*link_func)(const char *, const char *) = sflag ? symlink : link;

    if (link_func(source, target) != 0) {
        perror("ln");
        return 1;
    }

    return 0;
}
