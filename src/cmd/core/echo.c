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
#include <string.h>

int main(int argc, char *argv[]) {
    int i;
    for (i = 1; i < argc; i++) {
        char *p = argv[i];
        while (*p) {
            if (*p == '\\') {
                p++;
                switch (*p) {
                    case 'a':  putchar('\a'); break;
                    case 'b':  putchar('\b'); break;
                    case 'c':  return 0;      /* \c: Terminate output immediately */
                    case 'f':  putchar('\f'); break;
                    case 'n':  putchar('\n'); break;
                    case 'r':  putchar('\r'); break;
                    case 't':  putchar('\t'); break;
                    case 'v':  putchar('\v'); break;
                    case '\\': putchar('\\'); break;
                    case '0': {               /* \0num: Octal constant */
                        int j, n = 0;
                        for (j = 0; j < 3 && p[1] >= '0' && p[1] <= '7'; j++) {
                            n = n * 8 + (*++p - '0');
                        }
                        putchar(n);
                        break;
                    }
                    default:
                        putchar('\\');
                        if (*p) putchar(*p);
                        else p--; /* Back up if backslash was at end of string */
                        break;
                }
            } else {
                putchar(*p);
            }
            if (*p) p++;
        }
        if (i < argc - 1) putchar(' ');
    }
    putchar('\n');
    return 0;
}
