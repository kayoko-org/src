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
#include <unistd.h>
#include <string.h>

int main(int argc, char *argv[]) {
    char *tty_name;
    int silent = 0;
    int opt;

    /* POSIX: Only -s is defined for tty */
    while ((opt = getopt(argc, argv, "s")) != -1) {
        switch (opt) {
            case 's':
                silent = 1;
                break;
            default:
                return 2; /* POSIX: exit >1 on error */
        }
    }

    /* Check if stdin (fd 0) is a terminal */
    tty_name = ttyname(STDIN_FILENO);

    if (!silent) {
        if (tty_name) {
            printf("%s\n", tty_name);
        } else {
            printf("not a tty\n");
        }
    }

    /* POSIX exit status: 0 if tty, 1 if not */
    return (tty_name ? 0 : 1);
}
