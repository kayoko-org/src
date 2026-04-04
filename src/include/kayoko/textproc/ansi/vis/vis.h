/*
 * Kayoko - Source Code
 * 
 * Copyright (c) 2026 The Kayoko Project. All Rights Reserved
 * 
 * This file is licensed under the Common Development and Distribution License (CDDL).
 * 
 * See /usr/src/COPYING for details.
 */

#ifndef KAYOKO_TEXTPROC_ANSI_VIS_H
#define KAYOKO_TEXTPROC_ANSI_VIS_H

#include <stdio.h>
#include <ctype.h>

/* Function Prototype */
void kayoko_vis(FILE *fp);

#ifdef KAYOKO_TEXTPROC_ANSI_VIS_IMPLEMENTATION

void kayoko_vis(FILE *fp) {
    int ch;
    while ((ch = fgetc(fp)) != EOF) {
        /* Filter for printable characters and standard whitespace */
        if (isprint(ch)) {
            if (ch == '\\') {
                fputs("\\\\", stdout);
            } else {
                putchar(ch);
            }
        } else {
            /* The classic BSD/C-style escape logic */
            switch (ch) {
                case '\r': fputs("\r", stdout); break;
                case '\n': fputs("\n", stdout); break;
                case '\t': fputs("\t", stdout); break;
                case '\a': fputs("\\a", stdout); break;
                case '\b': fputs("\\b", stdout); break;
                case '\v': fputs("\\v", stdout); break;
                case '\f': fputs("\\f", stdout); break;
                case ' ':  putchar(' ');         break; // Keep spaces literal
                default:
                    /* Fallback to octal for non-printables */
                    fprintf(stdout, "\\%03o", (unsigned char)ch);
            }
        }
    }
}

#endif /* KAYOKO_TEXTPROC_ANSI_VIS_IMPLEMENTATION */
#endif /* KAYOKO_TEXTPROC_ANSI_VIS_H */
