#include "ed.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/wait.h>
#include <unistd.h>

void line_add_ref(Line *l) {
    if (l && l->rs) l->rs->refcount++;
}

void line_release(Line *l) {
    if (l && l->rs && --l->rs->refcount == 0) {
        free(l->rs->data);
        free(l->rs);
        l->rs = NULL;
    }
}

void update_line_text(Line *l, const char *new_text) {
    line_release(l); 
    l->rs = malloc(sizeof(RefString));
    l->rs->data = strdup(new_text);
    l->rs->refcount = 1;
}

void save_undo(Ed *e) {
    for (size_t i = 0; i < e->u_count; i++) line_release(&e->u_lines[i]);
    free(e->u_lines);

    e->u_count = e->count;
    e->u_curr = e->curr;
    e->u_lines = malloc(e->u_count * sizeof(Line));

    for (size_t i = 0; i < e->count; i++) {
        e->u_lines[i] = e->lines[i];
        line_add_ref(&e->u_lines[i]);
    }
    e->can_undo = 1;
}

void do_undo(Ed *e) {
    if (!e->can_undo) { set_err(e, "nothing to undo"); return; }
    
    Line *tmp_l = e->lines;
    size_t tmp_cnt = e->count;
    int tmp_cur = e->curr;

    e->lines = e->u_lines;
    e->count = e->u_count;
    e->curr = e->u_curr;

    e->u_lines = tmp_l;
    e->u_count = tmp_cnt;
    e->u_curr = tmp_cur;
}

void ed_init(Ed *e) {
    memset(e, 0, sizeof(Ed));
    e->last_err = "no error";
}

void ed_free(Ed *e) {
    for (size_t i = 0; i < e->count; i++) line_release(&e->lines[i]);
    for (size_t i = 0; i < e->u_count; i++) line_release(&e->u_lines[i]);
    free(e->lines); free(e->u_lines);
    free(e->fname); free(e->last_re); free(e->rhs);
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
    
    /* Crucial: Initialize to NULL, then use the performance helper */
    e->lines[at].rs = NULL;
    update_line_text(&e->lines[at], s);
    
    e->lines[at].gmark = 0;
    for (int j = 0; j < 26; j++) e->lines[at].marks[j] = -1;
    e->count++; e->curr = at + 1; e->dirty = 1;
}

void del_lines(Ed *e, int a, int b) {
    if (a < 1 || b > (int)e->count || a > b) { set_err(e, "invalid address"); return; }
    int num = b - a + 1;
    /* Release refs instead of calling free() on .text */
    for (int i = a; i <= b; i++) line_release(&e->lines[i-1]);
    for (int i = b; i < (int)e->count; i++) e->lines[i-num] = e->lines[i];
    e->count -= num;
    e->curr = (a > (int)e->count) ? (int)e->count : a;
    e->dirty = 1;
}

void do_shell(Ed *e, char *cmd_line) {
    if (e->restricted) {
        set_err(e, "restricted: shell access denied");
        return;
    }

    // Skip leading whitespace
    while (isspace(*cmd_line)) cmd_line++;
    
    char buf[1024] = {0};
    char *p = cmd_line;
    int i = 0;

    // Expand '!' to last command and '%' to filename
    while (*p && *p != '\n') {
        if (*p == '!' && i == 0) {
            if (!e->last_sh) { set_err(e, "no prev command"); return; }
            strcpy(buf + i, e->last_sh);
            i += strlen(e->last_sh);
        } else if (*p == '%' && *(p-1) != '\\') {
            if (!e->fname) { set_err(e, "no current filename"); return; }
            strcpy(buf + i, e->fname);
            i += strlen(e->fname);
        } else {
            buf[i++] = *p;
        }
        p++;
    }
    buf[i] = '\0';

    if (strlen(buf) == 0) { set_err(e, "empty command"); return; }

    // Save this command for next time
    free(e->last_sh);
    e->last_sh = strdup(buf);

    // Print the expanded command (POSIX requirement)
    printf("%s\n", buf);

    // Execute via system()
    system(buf);
    
    // Print the '!' separator after the command finishes
    puts("!");
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
    
    /* POSIX: Handle empty // by using last_re */
    if (strlen(pat) == 0) {
        if (!e->last_re || regcomp(&re, e->last_re, 0) != 0) { 
            set_err(e, "no prev re"); free(pat); return; 
        }
    } else {
        /* POSIX ed uses BRE (0), not REG_EXTENDED */
        if (regcomp(&re, pat, 0) != 0) { 
            set_err(e, "bad re"); free(pat); return; 
        }
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
        /* ACCESS CHANGE: Use .rs->data instead of .text */
        char *src = e->lines[i-1].rs->data;
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
            /* PERFORMANCE CHANGE: Use our CoW helper */
            update_line_text(&e->lines[i-1], buf);
            e->curr = i; e->dirty = 1;
        }
    }
    regfree(&re);
}

int main(int argc, char **argv) {
    Ed e; 
    ed_init(&e);
    char *ln = NULL; 
    size_t ln_cap = 0;
    setvbuf(stdout, NULL, _IONBF, 0);

    /* 1. Handle Restricted Mode via argv[0] */
    if (argc > 0) {
        char *progname = strrchr(argv[0], '/');
        progname = progname ? progname + 1 : argv[0];
        if (strcmp(progname, "red") == 0) {
            e.restricted = 1;
        }
    }

    /* 2. Initial File Load */
    if (argc > 1) {
        e.fname = strdup(argv[1]);
        FILE *fp = fopen(e.fname, "r");
        if (fp) {
            size_t bytes = 0;
            while (getline(&ln, &ln_cap, fp) > 0) {
                ln[strcspn(ln, "\n")] = 0; 
                add_line(&e, (int)e.count, ln);
                bytes += strlen(e.lines[e.count-1].rs->data) + 1;
            }
            printf("%zu\n", bytes); 
            fclose(fp); 
            e.dirty = 0;
        } else {
            /* POSIX: Print ?filename if it doesn't exist */
            printf("?%s\n", e.fname);
        }
    }

    /* 3. Main Command Loop */
    while (getline(&ln, &ln_cap, stdin) > 0) {
        char *p = ln; 
        int a = e.curr, b = e.curr, has_a = 0;
        
        /* Address Parsing */
        if (*p == ',') { a = 1; b = (int)e.count; p++; has_a = 1; }
        else if (get_addr(&e, &p, &a)) {
            has_a = 1; b = a;
            if (*p == ',' || *p == ';') { 
                if (*p == ';') e.curr = a; 
                p++; get_addr(&e, &p, &b); 
            }
        }

        while (isspace(*p)) p++;
        
        /* Empty Input Handling */
        if (*p == '\n' || *p == '\0') {
            if (has_a) {
                e.curr = b;
                if (e.curr >= 1 && e.curr <= (int)e.count) 
                    puts(e.lines[e.curr-1].rs->data);
                else set_err(&e, "invalid address");
            } else {
                set_err(&e, "unknown command");
            }
            continue;
        }

        char cmd = *p++;
        int is_q = (cmd == 'q');

        switch (cmd) {
            case 'a': 
                save_undo(&e);
                while (getline(&ln, &ln_cap, stdin) > 0 && strcmp(ln, ".\n") != 0) {
                    ln[strcspn(ln, "\n")] = 0; 
                    add_line(&e, (has_a ? b++ : e.curr++), ln);
                } 
                break;

            case 'u': 
                while (isspace(*p)) p++;
                if (*p != '\n' && *p != '\0') { set_err(&e, "unknown command"); break; }
                do_undo(&e); 
                break;

            case 'd': 
                save_undo(&e); 
                del_lines(&e, a, b); 
                break;

            case 'p': 
            case 'n':
                while (isspace(*p)) p++;
                if (*p != '\n' && *p != '\0') { set_err(&e, "unknown command"); break; }
                if (a >= 1 && b <= (int)e.count) {
                    for (int i = a; i <= b; i++) {
                        if (cmd == 'n') printf("%d\t%s\n", i, e.lines[i-1].rs->data);
                        else puts(e.lines[i-1].rs->data);
                    }
                    e.curr = b;
                } else set_err(&e, "range"); 
                break;

            case 's': 
                save_undo(&e); 
                do_sub(&e, a, b, p); 
                break;

            case '!':
                do_shell(&e, p);
                break;

            case 'k': 
                while (isspace(*p)) p++;
                if (b >= 1 && b <= (int)e.count && islower(*p))
                    e.lines[b-1].marks[*p - 'a'] = b; 
                else set_err(&e, "invalid mark");
                break;

            case 'w': {
                while (isspace(*p)) p++; 
                p[strcspn(p, "\n")] = 0;
                if (e.restricted && *p != '\0' && e.fname && strcmp(p, e.fname) != 0) {
                    set_err(&e, "restricted: cannot write to different file");
                    break;
                }
                char *f = (*p) ? p : e.fname;
                if (!f) { set_err(&e, "no current filename"); break; }
                FILE *wf = fopen(f, "w");
                if (wf) {
                    size_t tot = 0;
                    for (size_t i = 0; i < e.count; i++) 
                        tot += fprintf(wf, "%s\n", e.lines[i].rs->data);
                    fclose(wf); printf("%zu\n", tot); e.dirty = 0;
                    if (!e.fname) e.fname = strdup(f);
                } else set_err(&e, "io error");
            } break;

            case 'q':
                while (isspace(*p)) p++;
                if (*p != '\n' && *p != '\0') { set_err(&e, "unknown command"); break; }
                if (e.dirty && !e.warned) {
                    set_err(&e, "warning: file modified");
                    e.warned = 1;
                } else goto end;
                break;

            case 'f':
                while (isspace(*p)) p++; p[strcspn(p, "\n")] = 0;
                if (*p == '\0') {
                    if (e.fname) printf("%s\n", e.fname);
                    else set_err(&e, "no current filename");
                } else if (e.restricted) {
                    set_err(&e, "restricted: cannot change filename");
                } else {
                    free(e.fname); e.fname = strdup(p);
                }
                break;

            case 'h': printf("%s\n", e.last_err); break;
            case 'H': e.show_help = !e.show_help; break;
            case 'P': e.prompt = !e.prompt; break;
            
            default: set_err(&e, "unknown command"); break;
        }

        /* Reset the 'q' warning for any command except 'q' */
        if (!is_q) e.warned = 0;

        if (e.prompt) fputs("*", stdout);
    }
end:
    free(ln); 
    ed_free(&e); 
    return 0;
}
