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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

/*
 * Xai pwd: SUSv4 compliant.
 * -L: Logical path (includes symlinks from $PWD)
 * -P: Physical path (resolves all symlinks)
 */

static char *get_logical_path(void) {
    char *pwd_env = getenv("PWD");
    struct stat st_pwd, st_dot;

    if (!pwd_env || pwd_env[0] != '/') {
        return NULL;
    }

    /* * To be valid, $PWD must point to the same device/inode as "."
     * and must not contain '.' or '..' components that aren't resolved.
     */
    if (stat(pwd_env, &st_pwd) == 0 && stat(".", &st_dot) == 0) {
        if (st_pwd.st_dev == st_dot.st_dev && st_pwd.st_ino == st_dot.st_ino) {
            return pwd_env;
        }
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    int opt;
    int mode = 'L'; // Default is Logical per POSIX
    char *path;
    char phys_path[4096]; // Using a fixed buffer for simplicity

    while ((opt = getopt(argc, argv, "LP")) != -1) {
        switch (opt) {
            case 'L':
                mode = 'L';
                break;
            case 'P':
                mode = 'P';
                break;
            default:
                fprintf(stderr, "usage: %s [-L | -P]\n", argv[0]);
                return 1;
        }
    }

    if (mode == 'L') {
        path = get_logical_path();
        if (path) {
            printf("%s\n", path);
            return 0;
        }
        /* If $PWD was invalid or not set, fall back to Physical behavior */
    }

    /* Physical mode or Logical fallback */
    if (getcwd(phys_path, sizeof(phys_path)) != NULL) {
        printf("%s\n", phys_path);
        return 0;
    } else {
        perror("pwd");
        return 1;
    }
}
