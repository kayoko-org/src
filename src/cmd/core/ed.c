#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <regex.h>
#include <errno.h>

typedef struct {
    char **lines;
    size_t count, cap;
    int curr, dirty, show_help;
    char *fname, *last_err;
} Ed;

static void set_err(Ed *e, char *msg) {
    e->last_err = msg;
    fprintf(stdout, "?\n");
    if (e->show_help) fprintf(stdout, "%s\n", msg);
}

static void add_line(Ed *e, int at, char *s) {
    if (e->count >= e->cap) {
        e->cap = e->cap ? e->cap * 2 : 128;
        e->lines = realloc(e->lines, e->cap * sizeof(char *));
    }
    for (int i = e->count; i > at; i--) e->lines[i] = e->lines[i-1];
    e->lines[at] = strndup(s, 1024);
    e->count++; e->curr = at + 1; e->dirty = 1;
}

/* POSIX Substitution: s/re/replacement/g */
static void do_sub(Ed *e, int a, int b, char *args) {
    if (a < 1 || b > e->count) { set_err(e, "Invalid address"); return; }
    char delim = *args++;
    char *re_str = args;
    char *repl_str = strchr(re_str, delim);
    if (!repl_str) { set_err(e, "Missing delimiter"); return; }
    *repl_str++ = '\0';
    char *opts = strchr(repl_str, delim);
    if (opts) *opts++ = '\0';

    regex_t re;
    if (regcomp(&re, re_str, 0) != 0) { set_err(e, "Invalid regex"); return; }

    for (int i = a; i <= b; i++) {
        regmatch_t pm;
        if (regexec(&re, e->lines[i-1], 1, &pm, 0) == 0) {
            char new_ln[2048];
            snprintf(new_ln, sizeof(new_ln), "%.*s%s%s", 
                     (int)pm.rm_so, e->lines[i-1], repl_str, e->lines[i-1] + pm.rm_eo);
            free(e->lines[i-1]);
            e->lines[i-1] = strdup(new_ln);
            e->curr = i; e->dirty = 1;
        }
    }
    regfree(&re);
}

static int parse_addr(Ed *e, char **s, int *addr) {
    while (isspace(**s)) (*s)++;
    if (**s == '.') { (*s)++; *addr = e->curr; }
    else if (**s == '$') { (*s)++; *addr = e->count; }
    else if (isdigit(**s)) { *addr = (int)strtol(*s, s, 10); }
    else return 0;
    return 1;
}

int main(int argc, char **argv) {
    Ed e = {NULL, 0, 0, 0, 0, 0, NULL, "No error"};
    char *cmd = NULL; size_t clen = 0;

    if (argc > 1) {
        e.fname = strdup(argv[1]);
        FILE *f = fopen(e.fname, "r");
        if (f) {
            char *l = NULL; size_t n = 0, b = 0;
            while (getline(&l, &n, f) != -1) {
                l[strcspn(l, "\n")] = 0;
                add_line(&e, e.count, l);
                b += strlen(l) + 1;
            }
            printf("%zu\n", b); fclose(f); free(l); e.dirty = 0;
        } else fprintf(stdout, "?%s\n", e.fname);
    }

    while (getline(&cmd, &clen, stdin) != -1) {
        char *p = cmd;
        int a = e.curr, b = e.curr, has_a = 0;

        if (parse_addr(&e, &p, &a)) {
            has_a = 1; b = a;
            if (*p == ',' || *p == ';') {
                if (*p == ';') e.curr = a; 
                p++;
                if (!parse_addr(&e, &p, &b)) b = e.count;
            }
        } else if (*p == ',') { p++; a = 1; b = e.count; has_a = 1; }

        while (isspace(*p)) p++;
        char c = *p; p++;

        switch (c) {
            case 'a': {
                char *in = NULL; size_t in_n = 0;
                int at = has_a ? a : e.curr;
                while (getline(&in, &in_n, stdin) != -1 && strcmp(in, ".\n") != 0) {
                    in[strcspn(in, "\n")] = 0;
                    add_line(&e, at++, in);
                }
                free(in); break;
            }
            case 'p': for (int i = a; i <= b; i++) printf("%s\n", e.lines[i-1]); e.curr = b; break;
            case 's': do_sub(&e, a, b, p); break;
            case 'w': {
                while (isspace(*p)) p++; p[strcspn(p, "\n")] = 0;
                char *t = (*p) ? p : e.fname;
                if (!t) { set_err(&e, "No file"); break; }
                FILE *f = fopen(t, "w");
                if (!f) { set_err(&e, "Write fail"); break; }
                size_t tot = 0;
                for (int i = (has_a?a:1); i <= (has_a?b:e.count); i++) tot += fprintf(f, "%s\n", e.lines[i-1]);
                fclose(f); printf("%zu\n", tot); e.dirty = 0; break;
            }
            case 'h': printf("%s\n", e.last_err); break;
            case 'H': e.show_help = !e.show_help; break;
            case 'q': if (e.dirty) { set_err(&e, "Modified"); e.dirty = 0; } else goto end; break;
            case '\n': if (e.curr < e.count) { e.curr++; printf("%s\n", e.lines[e.curr-1]); } break;
            default: if (c && !isspace(c)) set_err(&e, "Unknown");
        }
    }
end:
    for (size_t i = 0; i < e.count; i++) free(e.lines[i]);
    free(e.lines); free(e.fname); free(cmd);
    return 0;
}
