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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>

int main(int argc, char *argv[]) {
    struct passwd *pw;
    uid_t uid;

    /* POSIX: whoami does not take arguments */
    if (argc > 1) {
        fprintf(stderr, "usage: whoami\n");
        return 1;
    }

    uid = geteuid();
    pw = getpwuid(uid);

    if (pw && pw->pw_name) {
        puts(pw->pw_name);
        return 0;
    }

    /* If the UID isn't in the passwd file, we can't determine the name */
    fprintf(stderr, "getpwuid %lu: no such user\n", (unsigned long)uid);
    return 1;
}
