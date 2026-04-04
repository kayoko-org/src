/*
 * Kayoko - Source Code
 * 
 * Copyright (c) 2026 The Kayoko Project. All Rights Reserved
 * 
 * This file is licensed under the Common Development and Distribution License (CDDL).
 * 
 * See /usr/src/COPYING for details.
 */

#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ftw.h>

/* Global state for simplicity in a small core utility */
static long total_blocks = 0;
static int opt_a = 0, opt_s = 0, opt_k = 0;

/* * Callback for nftw. 
 * Note: POSIX du measures in 512-byte units by default.
 */
static int display_usage(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    long blocks = (long)sb->st_blocks;

    /* Logic for -a (all files) vs default (directories only) */
    if (!opt_s) {
        if (typeflag == FTW_D || (opt_a && typeflag == FTW_F)) {
            long display_val = opt_k ? (blocks + 1) / 2 : blocks;
            printf("%ld\t%s\n", display_val, fpath);
        }
    }

    total_blocks += blocks;
    return 0;
}

int main(int argc, char *argv[]) {
    int opt;

    /* Reset opterr for standard error handling */
    while ((opt = getopt(argc, argv, "ask")) != -1) {
        switch (opt) {
            case 'a': opt_a = 1; break;
            case 's': opt_s = 1; break;
            case 'k': opt_k = 1; break;
            default: 
                fprintf(stderr, "Usage: du [-ask] [file...]\n");
                return 1;
        }
    }

    const char *path = (optind < argc) ? argv[optind] : ".";

    /* * FTW_PHYS: Do not follow symbolic links.
     * FTW_MOUNT: Stay within the same file system (often desired for du).
     */
    if (nftw(path, display_usage, 20, FTW_PHYS) == -1) {
        perror("du: nftw");
        return 1;
    }

    /* If -s (summary) was requested, print the grand total for the target path */
    if (opt_s) {
        long final_val = opt_k ? (total_blocks + 1) / 2 : total_blocks;
        printf("%ld\t%s\n", final_val, path);
    }

    return 0;
}
