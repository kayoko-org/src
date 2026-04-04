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
#include <stdbool.h>

/* tr operates on the range of a byte (0-255) */
unsigned char map[256];
bool delete_set[256];
bool squeeze_set[256];

void expand_set(const char *arg, bool *flags) {
    for (int i = 0; arg[i]; i++) {
        if (arg[i+1] == '-' && arg[i+2]) {
            for (int c = (unsigned char)arg[i]; c <= (unsigned char)arg[i+2]; c++)
                flags[c] = true;
            i += 2;
        } else {
            flags[(unsigned char)arg[i]] = true;
        }
    }
}

int main(int argc, char *argv[]) {
    int opt;
    bool dflag = false, sflag = false;
    
    while ((opt = getopt(argc, argv, "ds")) != -1) {
        switch (opt) {
            case 'd': dflag = true; break;
            case 's': sflag = true; break;
            default: return 1;
        }
    }
    argc -= optind;
    argv += optind;

    /* Initialize map to identity */
    for (int i = 0; i < 256; i++) map[i] = i;

    if (dflag && !sflag) {
        if (argc < 1) return 1;
        expand_set(argv[0], delete_set);
    } else if (!dflag && !sflag) {
        if (argc < 2) return 1;
        /* Simple mapping: this doesn't handle complex POSIX ranges 
           but covers the standard char-to-char translation */
        for (int i = 0; argv[0][i] && argv[1][i]; i++) {
            map[(unsigned char)argv[0][i]] = (unsigned char)argv[1][i];
        }
    } else if (sflag && !dflag) {
        if (argc < 1) return 1;
        expand_set(argv[0], squeeze_set);
    }

    int ch, prev = -1;
    while ((ch = getchar()) != EOF) {
        if (dflag && delete_set[ch]) continue;
        
        unsigned char out = map[ch];
        
        if (sflag && squeeze_set[out] && out == prev) continue;
        
        putchar(out);
        prev = out;
    }

    return 0;
}
