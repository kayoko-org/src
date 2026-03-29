#ifndef KAYOKO_REGEX_BRE_H
#define KAYOKO_REGEX_BRE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include <kayoko/textproc/ed/buffer.h>
#include <kayoko/textproc/ed.h>
#include <kayoko/textproc/ed/commands.h>

/* * Standalone substitution logic for the ed editor.
 * Assumes Ed struct provides: lines[], last_re, rhs, curr, and dirty.
 */

static inline void do_sub(Ed *e, int a, int b, char *p) {
    if (!p || *p == '\0') {
        set_err(e, "invalid command");
        return;
    }

    char delim = *p++;
    char *re_end = strchr(p, delim);
    if (!re_end) { 
        set_err(e, "delimiter mismatch"); 
        return; 
    }

    // Extract pattern
    char *pat = strndup(p, re_end - p);
    regex_t re;
    
    if (strlen(pat) == 0) {
        if (!e->last_re || regcomp(&re, e->last_re, 0) != 0) {
            set_err(e, "no prev re");
            free(pat);
            return;
        }
    } else {
        if (regcomp(&re, pat, 0) != 0) {
            set_err(e, "bad re");
            free(pat);
            return;
        }
        if (e->last_re) free(e->last_re);
        e->last_re = strdup(pat);
    }
    free(pat);
    p = re_end + 1;

    // Extract replacement string (RHS)
    char *rhs_end = strchr(p, delim);
    if (rhs_end) {
        if (e->rhs) free(e->rhs);
        e->rhs = strndup(p, rhs_end - p);
        p = rhs_end + 1;
    }

    int global = (strchr(p, 'g') != NULL);

    // Process line range [a, b]
    for (int i = a; i <= b; i++) {
	if (i < 1 || i > (int)e->count) continue;
        char *src = e->lines[i-1].rs->data;
        regmatch_t pm;
        char buf[8192] = {0}; 
        int off = 0, found = 0;

        // Perform substitution
        while (regexec(&re, src + off, 1, &pm, 0) == 0) {
            found = 1;
            // Append prefix
            strncat(buf, src + off, pm.rm_so);
            // Append replacement
            if (e->rhs) strcat(buf, e->rhs);
            
            off += pm.rm_eo;
            if (!global) break;
        }

        if (found) {
            // Append remaining suffix
            strcat(buf, src + off);
            update_line_text(&e->lines[i-1], buf);
            e->curr = i;
            e->dirty = 1;
        }
    }
    
    regfree(&re);
}

#endif /* KAYOKO_REGEX_BRE_H */
