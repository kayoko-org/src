/*
 * Kayoko - Source Code
 * 
 * Copyright (c) 2026 The Kayoko Project. All Rights Reserved
 * 
 * This file is licensed under the Common Development and Distribution License (CDDL).
 * 
 * See /usr/src/COPYING for details.
 */

#ifndef KAYOKO_TEXTPROC_ANSI_UNVIS_H
#define KAYOKO_TEXTPROC_ANSI_UNVIS_H

#include <stdio.h>
#include <stdlib.h>

/* Function Prototype */
void kayoko_unvis(FILE *fp);

#ifdef KAYOKO_TEXTPROC_ANSI_UNVIS_IMPLEMENTATION

/**
 * kayoko_unvis
 * Reads from fp, decodes Kayoko/BSD style escape sequences, 
 * and prints raw bytes to stdout.
 */
void kayoko_unvis(FILE *fp) {
    int ch;
    while ((ch = fgetc(fp)) != EOF) {
        if (ch == '\\') {
            int next = fgetc(fp);
            if (next == EOF) {
                putchar('\\'); // Trailing backslash
                break;
            }

            switch (next) {
                case 'n':  putchar('\n'); break;
                case 'r':  putchar('\r'); break;
                case 't':  putchar('\t'); break;
                case 'a':  putchar('\a'); break;
                case 'b':  putchar('\b'); break;
                case 'v':  putchar('\v'); break;
                case 'f':  putchar('\f'); break;
                case '\\': putchar('\\'); break;
                
                /* Handle Octal: \000 through \377 */
                case '0': case '1': case '2': case '3':
                case '4': case '5': case '6': case '7': {
                    char octal[4];
                    octal[0] = (char)next;
                    octal[1] = (char)fgetc(fp);
                    octal[2] = (char)fgetc(fp);
                    octal[3] = '\0';
                    
                    /* Convert octal string to raw byte */
                    putchar((unsigned char)strtol(octal, NULL, 8));
                    break;
                }

                default:
                    /* If it's an unrecognized escape, just print both */
                    putchar('\\');
                    putchar(next);
                    break;
            }
        } else {
            /* Regular printable character */
            putchar(ch);
        }
    }
}

#endif /* KAYOKO_TEXTPROC_ANSI_UNVIS_IMPLEMENTATION */
#endif /* KAYOKO_TEXTPROC_ANSI_UNVIS_H */
