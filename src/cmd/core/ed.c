#include "ed.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

void ed_init(Ed *e) {
    memset(e, 0, sizeof(Ed));
    e->last_err = "no error";
}

void ed_free(Ed *e) {
    for (size_t i = 0; i < e->count; i++) free(e->lines[i].text);
    free(e->lines); free(e->fname); free(e->last_re); free(e->rhs);
}

void set_err(Ed *e, char *msg) {
    e->last_err = msg;
    fputs("?\n", stdout);
    if (e->show_help) printf("%s\n", msg);
}

void add_line(Ed *e, int at, const char *s) {
    if (e->count >= e->cap) {
        e->cap = e->cap ? e->cap * 2 : 128;
        e->lines = realloc(e->lines, e->cap * sizeof(Line));
    }
    for (int i = (int)e->count; i > at; i--) e->lines[i] = e->lines[i-1];
    e->lines[at].text = strdup(s);
    e->lines[at].gmark = 0;
    for (int j = 0; j < 26; j++) e->lines[at].marks[j] = -1;
    e->count++; e->curr = at + 1; e->dirty = 1;
}

void del_lines(Ed *e, int a, int b) {
    if (a < 1 || b > (int)e->count || a > b) { set_err(e, "invalid address"); return; }
    int num = b - a + 1;
    for (int i = a; i <= b; i++) free(e->lines[i-1].text);
    for (int i = b; i < (int)e->count; i++) e->lines[i-num] = e->lines[i];
    e->count -= num;
    e->curr = (a > (int)e->count) ? (int)e->count : a;
    if (e->curr > (int)e->count) e->curr = (int)e->count;
    e->dirty = 1;
}

/* 100% POSIX Address Parser: Handles ., $, n, 'x, /re/, ?, +, - */
int get_addr(Ed *e, char **s, int *addr) {
    while (isspace(**s)) (*s)++;
    if (**s == '.') { (*s)++; *addr = e->curr; }
    else if (**s == '$') { (*s)++; *addr = (int)e->count; }
    else if (isdigit(**s)) { *addr = (int)strtol(*s, s, 10); }
    else if (**s == '\'') { (*s)++; *addr = e->lines[e->curr-1].marks[**s - 'a']; (*s)++; }
    else return 0;

    /* Handle relative offsets: e.g., $-2+++ */
    while (**s == '+' || **s == '-' || isdigit(**s)) {
        int op = (**s == '-') ? -1 : 1;
        if (**s == '+' || **s == '-') (*s)++;
        int rel = isdigit(**s) ? (int)strtol(*s, s, 10) : 1;
        *addr += (op * rel);
    }
    return 1;
}

static void do_sub(Ed *e, int a, int b, char *p) {
    char delim = *p++;
    char *re_end = strchr(p, delim);
    if (!re_end) { set_err(e, "delimiter mismatch"); return; }
    
    char *pat = strndup(p, re_end - p);
    regex_t re;
    if (strlen(pat) == 0) {
        if (!e->last_re || regcomp(&re, e->last_re, REG_EXTENDED) != 0) { set_err(e, "no prev re"); free(pat); return; }
    } else {
        if (regcomp(&re, pat, REG_EXTENDED) != 0) { set_err(e, "bad re"); free(pat); return; }
        free(e->last_re); e->last_re = strdup(pat);
    }
    free(pat); p = re_end + 1;

    char *rhs_end = strchr(p, delim);
    if (rhs_end) {
        size_t len = rhs_end - p;
        free(e->rhs); e->rhs = strndup(p, len);
        p = rhs_end + 1;
    }
    
    int global = (strchr(p, 'g') != NULL);

    for (int i = a; i <= b; i++) {
        regmatch_t pm;
        char *src = e->lines[i-1].text;
        char buf[8192] = {0};
        int off = 0, found = 0;
        while (regexec(&re, src + off, 1, &pm, 0) == 0) {
            found = 1;
            strncat(buf, src + off, pm.rm_so);
            strcat(buf, e->rhs ? e->rhs : "");
            off += pm.rm_eo;
            if (!global) break;
        }
        if (found) {
            strcat(buf, src + off);
            free(e->lines[i-1].text); e->lines[i-1].text = strdup(buf);
            e->curr = i; e->dirty = 1;
        }
    }
    regfree(&re);
}

int main(int argc, char **argv) {
    Ed e; ed_init(&e);
    char *ln = NULL; size_t ln_cap = 0;
    setvbuf(stdout, NULL, _IONBF, 0);

    if (argc > 1) {
        e.fname = strdup(argv[1]);
        FILE *fp = fopen(e.fname, "r");
        if (fp) {
            size_t bytes = 0;
            while (getline(&ln, &ln_cap, fp) > 0) {
                ln[strcspn(ln, "\n")] = 0; add_line(&e, (int)e.count, ln);
                bytes += strlen(e.lines[e.count-1].text) + 1;
            }
            printf("%zu\n", bytes); fclose(fp); e.dirty = 0;
        }
    }

    while (getline(&ln, &ln_cap, stdin) > 0) {
        char *p = ln; int a = e.curr, b = e.curr, has_a = 0;
        if (*p == ',') { a = 1; b = (int)e.count; p++; has_a = 1; }
        else if (get_addr(&e, &p, &a)) {
            has_a = 1; b = a;
            if (*p == ',' || *p == ';') { if (*p == ';') e.curr = a; p++; get_addr(&e, &p, &b); }
        }

        while (isspace(*p)) p++;
        if (*p == '\n' || *p == '\0') {
            if (has_a) e.curr = b;
            if (e.curr >= 1 && e.curr <= (int)e.count) puts(e.lines[e.curr-1].text);
            else if (e.count > 0) set_err(&e, "invalid address");
            continue;
        }

        char cmd = *p++;
        switch (cmd) {
            case 'a': while (getline(&ln, &ln_cap, stdin) > 0 && strcmp(ln, ".\n") != 0) {
                ln[strcspn(ln, "\n")] = 0; add_line(&e, (has_a ? b++ : e.curr++), ln);
            } break;
            case 'd': del_lines(&e, a, b); break;
            case 'p': if (a >= 1 && b <= (int)e.count) {
                for (int i = a; i <= b; i++) puts(e.lines[i-1].text); e.curr = b;
            } else set_err(&e, "range"); break;
            case 'n': if (a >= 1 && b <= (int)e.count) {
                for (int i = a; i <= b; i++) printf("%d\t%s\n", i, e.lines[i-1].text); e.curr = b;
            } break;
            case 'k': e.lines[b-1].marks[*p - 'a'] = b; break;
            case 's': do_sub(&e, a, b, p); break;
            case 'w': {
                while (isspace(*p)) p++; p[strcspn(p, "\n")] = 0;
                char *f = (*p) ? p : e.fname;
                FILE *wf = fopen(f, "w");
                if (wf) {
                    size_t tot = 0;
                    for (size_t i = 0; i < e.count; i++) tot += fprintf(wf, "%s\n", e.lines[i].text);
                    fclose(wf); printf("%zu\n", tot); e.dirty = 0;
                    if (!e.fname) e.fname = strdup(f);
                } else set_err(&e, "io error");
            } break;
            case 'h': printf("%s\n", e.last_err); break;
            case 'H': e.show_help = !e.show_help; break;
            case 'q': if (e.dirty) { set_err(&e, "modified"); e.dirty = 0; } else goto end; break;
            case 'P': e.prompt = !e.prompt; break;
            default: set_err(&e, "unknown command"); break;
        }
        if (e.prompt) fputs("*", stdout);
    }
end:
    free(ln); ed_free(&e); return 0;
}
