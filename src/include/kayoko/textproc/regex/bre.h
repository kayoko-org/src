/*
 * Kayoko - Source Code
 * 
 * Copyright (c) 2026 The Kayoko Project. All Rights Reserved
 * 
 * This file is licensed under the Common Development and Distribution License (CDDL).
 * 
 * See /usr/src/COPYING for details.
 */

#ifndef KAYOKO_REGEX_BRE_H
#define KAYOKO_REGEX_BRE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <regex.h>
#include <kayoko/textproc/ed/buffer.h>
#include <kayoko/textproc/ed.h>

static inline void expand_rhs(char *buf, const char *rhs, const char *src, regmatch_t *pm) {
    for (const char *r = rhs; *r; r++) {
        if (*r == '&') {
            strncat(buf, src + pm[0].rm_so, pm[0].rm_eo - pm[0].rm_so);
        } else if (*r == '\\') {
            r++;
            if (isdigit(*r)) {
                int n = *r - '0';
                if (n < 10 && pm[n].rm_so != -1) {
                    strncat(buf, src + pm[n].rm_so, pm[n].rm_eo - pm[n].rm_so);
                }
            } else {
                strncat(buf, r, 1);
            }
        } else {
            strncat(buf, r, 1);
        }
    }
}

static inline void do_sub(Ed *e, int a, int b, char *p) {
    if (!p || *p == '\0') return;

    char delim = *p++;
    char *re_end = strchr(p, delim);
    if (!re_end) return;

    char *pat = strndup(p, re_end - p);
    regex_t re;
    if (strlen(pat) == 0) {
        if (!e->last_re || regcomp(&re, e->last_re, 0) != 0) { free(pat); return; }
    } else {
        if (regcomp(&re, pat, 0) != 0) { free(pat); return; }
        if (e->last_re) free(e->last_re);
        e->last_re = strdup(pat);
    }
    free(pat);
    p = re_end + 1;

    char *rhs_end = strchr(p, delim);
    if (rhs_end) {
        if (e->rhs) free(e->rhs);
        e->rhs = strndup(p, rhs_end - p);
        p = rhs_end + 1;
    }

    int global = 0, nth = 1;
    char *temp_p = p;
    while (*temp_p && !isspace(*temp_p)) {
        if (*temp_p == 'g') global = 1;
        else if (isdigit(*temp_p)) nth = (int)strtol(temp_p, &temp_p, 10);
        else temp_p++;
    }

    for (int i = a; i <= b; i++) {
        if (i < 1 || i > (int)e->count) continue;
        char *src = e->lines[i-1].rs->data;
        regmatch_t pm[10];
        char buf[8192] = {0};
        int off = 0, match_idx = 0, found = 0;

        while (regexec(&re, src + off, 10, pm, 0) == 0) {
            match_idx++;
            found = 1;

            if (match_idx == nth || (global && match_idx > nth)) {
                strncat(buf, src + off, pm[0].rm_so);
                if (e->rhs) expand_rhs(buf, e->rhs, src + off, pm);
            } else {
                /* Not our target match: copy everything including the match */
                strncat(buf, src + off, pm[0].rm_eo);
            }

            off += pm[0].rm_eo;
            if (pm[0].rm_eo == 0) {
                if (src[off] == '\0') break;
                strncat(buf, src + off, 1);
                off++;
            }
            if (!global && match_idx == nth) break;
        }

        if (found) {
            strcat(buf, src + off);
            update_line_text(&e->lines[i-1], buf);
            e->dirty = 1;
        }
    }
    regfree(&re);
}

#endif
