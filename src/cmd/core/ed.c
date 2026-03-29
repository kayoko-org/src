#include "ed.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/wait.h>
#include <unistd.h>
#include <regex.h>
static Ed *global_e = NULL;

void handle_hup(int sig) {
    if (!global_e || !global_e->dirty || global_e->count == 0) {
        exit(sig);
    }

    /* POSIX: try to save to ed.hup if a hangup occurs */
    FILE *hf = fopen("ed.hup", "w");
    if (hf) {
        for (size_t i = 0; i < global_e->count; i++) {
            fprintf(hf, "%s\n", global_e->lines[i].rs->data);
        }
        fclose(hf);
    }
    exit(sig);
}

/* Forward declaration so do_global can call it */
void exec_cmd(Ed *e, int a, int b, char *p, int *is_q, int has_a);

/* --- Core Memory & String Management --- */

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

/* --- Undo System --- */

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

/* --- Editor Lifecycle --- */

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

/* --- Basic Buffer Operations --- */

void add_line(Ed *e, int at, const char *s) {
    if (e->count >= e->cap) {
        e->cap = e->cap ? e->cap * 2 : 128;
        e->lines = realloc(e->lines, e->cap * sizeof(Line));
    }
    for (int i = (int)e->count; i > at; i--) e->lines[i] = e->lines[i-1];

    e->lines[at].rs = NULL;
    update_line_text(&e->lines[at], s);
    e->lines[at].gmark = 0;
    for (int j = 0; j < 26; j++) e->lines[at].marks[j] = -1;
    
    e->count++; e->curr = at + 1; e->dirty = 1;
}

void del_lines(Ed *e, int a, int b) {
    if (a < 1 || b > (int)e->count || a > b) { set_err(e, "invalid address"); return; }
    int num = b - a + 1;
    for (int i = a; i <= b; i++) line_release(&e->lines[i-1]);
    for (int i = b; i < (int)e->count; i++) e->lines[i-num] = e->lines[i];
    e->count -= num;
    e->curr = (a > (int)e->count) ? (int)e->count : a;
    if (e->curr == 0 && e->count > 0) e->curr = 1;
    e->dirty = 1;
}

void join_lines(Ed *e, int a, int b) {
    if (a < 1 || b > (int)e->count || a > b) { set_err(e, "invalid address"); return; }
    if (a == b) return; 

    size_t total_len = 0;
    for (int i = a; i <= b; i++) total_len += strlen(e->lines[i-1].rs->data);

    char *joined = malloc(total_len + 1);
    if (!joined) { set_err(e, "out of memory"); return; }
    joined[0] = '\0';

    for (int i = a; i <= b; i++) strcat(joined, e->lines[i-1].rs->data);

    update_line_text(&e->lines[a-1], joined);
    free(joined);

    del_lines(e, a + 1, b);
    e->curr = a;
}

/* --- Block Movement --- */

void move_lines(Ed *e, int a, int b, int t) {
    if (a < 1 || b > (int)e->count || a > b || (t >= a && t < b)) {
        set_err(e, "invalid address or destination"); return;
    }
    int num = b - a + 1;
    Line *tmp = malloc(num * sizeof(Line));
    if (!tmp) return;

    memcpy(tmp, &e->lines[a - 1], num * sizeof(Line));
    int remaining = (int)e->count - b;
    if (remaining > 0) memmove(&e->lines[a - 1], &e->lines[b], remaining * sizeof(Line));
    e->count -= num;

    int real_t = (t > b) ? (t - num) : t;
    int move_cnt = (int)e->count - real_t;
    if (move_cnt > 0) memmove(&e->lines[real_t + num], &e->lines[real_t], move_cnt * sizeof(Line));

    memcpy(&e->lines[real_t], tmp, num * sizeof(Line));
    free(tmp);
    e->count += num; e->curr = real_t + num; e->dirty = 1;
}

void transfer_lines(Ed *e, int a, int b, int t) {
    if (a < 1 || b > (int)e->count || a > b) { set_err(e, "invalid address"); return; }
    int num = b - a + 1;
    Line *tmp = malloc(num * sizeof(Line));
    if (!tmp) return;
    
    memcpy(tmp, &e->lines[a - 1], num * sizeof(Line));
    for (int i = 0; i < num; i++) line_add_ref(&tmp[i]);

    if (e->count + num > e->cap) {
        e->cap = e->cap ? e->cap * 2 : 128;
        e->lines = realloc(e->lines, e->cap * sizeof(Line));
    }

    int move_cnt = (int)e->count - t;
    if (move_cnt > 0) memmove(&e->lines[t + num], &e->lines[t], move_cnt * sizeof(Line));

    memcpy(&e->lines[t], tmp, num * sizeof(Line));
    free(tmp);
    e->count += num; e->curr = t + num; e->dirty = 1;
}

/* --- Shell Execution --- */

void do_shell(Ed *e, char *cmd_line) {
    if (e->restricted) { set_err(e, "restricted: shell access denied"); return; }
    while (isspace(*cmd_line)) cmd_line++;

    char buf[1024] = {0}; char *p = cmd_line; int i = 0;
    while (*p && *p != '\n') {
        if (*p == '!' && i == 0) {
            if (!e->last_sh) { set_err(e, "no prev command"); return; }
            strcpy(buf + i, e->last_sh); i += strlen(e->last_sh);
        } else if (*p == '%' && *(p-1) != '\\') {
            if (!e->fname) { set_err(e, "no current filename"); return; }
            strcpy(buf + i, e->fname); i += strlen(e->fname);
        } else buf[i++] = *p;
        p++;
    }
    buf[i] = '\0';
    if (i == 0) { set_err(e, "empty command"); return; }

    free(e->last_sh); e->last_sh = strdup(buf);
    printf("%s\n", buf);
    system(buf);
    puts("!");
}

/* --- Address Parsing --- */

int get_addr(Ed *e, char **s, int *addr) {
    while (isspace(**s)) (*s)++;
    if (**s == '.') { (*s)++; *addr = e->curr; }
    else if (**s == '$') { (*s)++; *addr = (int)e->count; }
    else if (isdigit(**s)) { *addr = (int)strtol(*s, s, 10); }
    else if (**s == '\'') { (*s)++; *addr = e->lines[e->curr-1].marks[**s - 'a']; (*s)++; }
    else return 0;

    while (**s == '+' || **s == '-' || isdigit(**s)) {
        int op = (**s == '-') ? -1 : 1;
        if (**s == '+' || **s == '-') (*s)++;
        int rel = isdigit(**s) ? (int)strtol(*s, s, 10) : 1;
        *addr += (op * rel);
    }
    return 1;
}

/* --- Complex Commands --- */

void do_global(Ed *e, int a, int b, char *p, int invert) {
    if (a < 1 || b > (int)e->count) { set_err(e, "invalid address"); return; }

    char delim = *p++;
    if (delim == '\n' || delim == '\0') { set_err(e, "invalid delimiter"); return; }
    char *re_end = strchr(p, delim);
    if (!re_end) { set_err(e, "delimiter mismatch"); return; }

    char *pat = strndup(p, re_end - p); regex_t re;
    if (regcomp(&re, pat, 0) != 0) { set_err(e, "bad re"); free(pat); return; }

    free(e->last_re); e->last_re = strdup(pat); free(pat);

    for (int i = 0; i < (int)e->count; i++) e->lines[i].gmark = 0;
    for (int i = a - 1; i < b; i++) {
        int match = (regexec(&re, e->lines[i].rs->data, 0, NULL, 0) == 0);
        if (match == !invert) e->lines[i].gmark = 1;
    }
    regfree(&re);

    char *cmd_list = re_end + 1;
    if (*cmd_list == '\n' || *cmd_list == '\0') cmd_list = "p\n";

    for (size_t i = 0; i < e->count; i++) {
        if (e->lines[i].gmark) {
            e->lines[i].gmark = 0; 
            e->curr = (int)i + 1;

            char *work_cmd = strdup(cmd_list); char *saveptr;
            char *token = strtok_r(work_cmd, "\n", &saveptr);

            while (token) {
                int dummy_q = 0; size_t old_cnt = e->count;
                char exec_buf[1024]; snprintf(exec_buf, sizeof(exec_buf), "%s\n", token);
                exec_cmd(e, e->curr, e->curr, exec_buf, &dummy_q, 1);
                
                if (e->count != old_cnt) i = (i + e->count) - old_cnt;
                token = strtok_r(NULL, "\n", &saveptr);
            }
            free(work_cmd);
        }
    }
}

static void do_sub(Ed *e, int a, int b, char *p) {
    char delim = *p++;
    char *re_end = strchr(p, delim);
    if (!re_end) { set_err(e, "delimiter mismatch"); return; }

    char *pat = strndup(p, re_end - p); regex_t re;
    if (strlen(pat) == 0) {
        if (!e->last_re || regcomp(&re, e->last_re, 0) != 0) {
            set_err(e, "no prev re"); free(pat); return;
        }
    } else {
        if (regcomp(&re, pat, 0) != 0) { set_err(e, "bad re"); free(pat); return; }
        free(e->last_re); e->last_re = strdup(pat);
    }
    free(pat); p = re_end + 1;

    char *rhs_end = strchr(p, delim);
    if (rhs_end) {
        free(e->rhs); e->rhs = strndup(p, rhs_end - p);
        p = rhs_end + 1;
    }

    int global = (strchr(p, 'g') != NULL);

    for (int i = a; i <= b; i++) {
        regmatch_t pm; char *src = e->lines[i-1].rs->data;
        char buf[8192] = {0}; int off = 0, found = 0; // Note: 8192 static limit

        while (regexec(&re, src + off, 1, &pm, 0) == 0) {
            found = 1;
            strncat(buf, src + off, pm.rm_so);
            strcat(buf, e->rhs ? e->rhs : "");
            off += pm.rm_eo;
            if (!global) break;
        }
        if (found) {
            strcat(buf, src + off);
            update_line_text(&e->lines[i-1], buf);
            e->curr = i; e->dirty = 1;
        }
    }
    regfree(&re);
}

/* --- Core Command Dispatcher --- */

void exec_cmd(Ed *e, int a, int b, char *p, int *is_q, int has_a) {
    while (isspace(*p) && *p != '\n') p++;

    if (*p == '\n' || *p == '\0') {
        if (has_a) {
            e->curr = b;
            if (e->curr >= 1 && e->curr <= (int)e->count) puts(e->lines[e->curr-1].rs->data);
            else set_err(e, "invalid address");
        } else set_err(e, "unknown command");
        return;
    }

    char cmd = *p++;
    if (cmd == 'q') *is_q = 1;

    switch (cmd) {
        case 'a':
        case 'i':
        case 'c': {
            save_undo(e);
            if (cmd == 'c') del_lines(e, a, b);
            int target = (cmd == 'i') ? (has_a ? a - 1 : e->curr - 1) : (has_a ? b : e->curr);
            char *ln = NULL; size_t ln_cap = 0;
            while (getline(&ln, &ln_cap, stdin) > 0 && strcmp(ln, ".\n") != 0) {
                ln[strcspn(ln, "\n")] = 0;
                add_line(e, target++, ln);
            }
            free(ln);
        } break;
        case 'u':
            while (isspace(*p)) p++;
            if (*p != '\n' && *p != '\0') { set_err(e, "unknown command"); break; }
            do_undo(e); break;
        case 'd': save_undo(e); del_lines(e, a, b); break;
        case 'j': save_undo(e); join_lines(e, a, b); break;
        case 'p':
        case 'n':
            while (isspace(*p)) p++;
            if (*p != '\n' && *p != '\0') { set_err(e, "unknown command"); break; }
            if (a >= 1 && b <= (int)e->count) {
                for (int i = a; i <= b; i++) {
                    if (cmd == 'n') printf("%d\t%s\n", i, e->lines[i-1].rs->data);
                    else puts(e->lines[i-1].rs->data);
                }
                e->curr = b;
            } else set_err(e, "range");
            break;
        case 's': save_undo(e); do_sub(e, a, b, p); break;
        case '!': do_shell(e, p); break;
        case 'k':
            while (isspace(*p)) p++;
            if (b >= 1 && b <= (int)e->count && islower(*p)) e->lines[b-1].marks[*p - 'a'] = b;
            else set_err(e, "invalid mark");
            break;
        case 'm':
        case 't': {
            save_undo(e); int target = 0;
            if (!get_addr(e, &p, &target)) set_err(e, "destination expected");
            else if (cmd == 'm') move_lines(e, a, b, target);
            else transfer_lines(e, a, b, target);
        } break;
        case 'g': do_global(e, a, b, p, 0); break;
        case 'v': do_global(e, a, b, p, 1); break;
        case 'w': {
            while (isspace(*p)) p++; p[strcspn(p, "\n")] = 0;
            if (e->restricted && *p != '\0' && e->fname && strcmp(p, e->fname) != 0) {
                set_err(e, "restricted: cannot write to different file"); break;
            }
            char *f = (*p) ? p : e->fname;
            if (!f) { set_err(e, "no current filename"); break; }
            FILE *wf = fopen(f, "w");
            if (wf) {
                size_t tot = 0;
                for (size_t i = 0; i < e->count; i++) tot += fprintf(wf, "%s\n", e->lines[i].rs->data);
                fclose(wf); printf("%zu\n", tot); e->dirty = 0;
                if (!e->fname) e->fname = strdup(f);
            } else set_err(e, "io error");
        } break;
        case 'q':
            while (isspace(*p)) p++;
            if (*p != '\n' && *p != '\0') { set_err(e, "unknown command"); break; }
            if (e->dirty && !e->warned) {
                set_err(e, "warning: file modified"); e->warned = 1; *is_q = 0;
            }
            break;
        case 'f':
            while (isspace(*p)) p++; p[strcspn(p, "\n")] = 0;
            if (*p == '\0') {
                if (e->fname) printf("%s\n", e->fname);
                else set_err(e, "no current filename");
            } else if (e->restricted) set_err(e, "restricted: cannot change filename");
            else { free(e->fname); e->fname = strdup(p); }
            break;
        case 'h': printf("%s\n", e->last_err); break;
        case 'H': e->show_help = !e->show_help; break;
        case 'P': e->prompt = !e->prompt; break;
        default: set_err(e, "unknown command"); break;
    }
}

/* --- Main Loop --- */

int main(int argc, char **argv) {
    Ed e; ed_init(&e);
    char *ln = NULL; size_t ln_cap = 0;
    global_e = &e;
    setvbuf(stdout, NULL, _IONBF, 0);

    /* Register SIGHUP handler */
    struct sigaction sa;
    sa.sa_handler = handle_hup;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGHUP, &sa, NULL);

    if (argc > 0) {
        char *progname = strrchr(argv[0], '/');
        progname = progname ? progname + 1 : argv[0];
        if (strcmp(progname, "red") == 0) e.restricted = 1;
    }

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
            fclose(fp); e.dirty = 0;
        } else printf("?%s\n", e.fname);
    }

    if (e.prompt) fputs("*", stdout);

    while (getline(&ln, &ln_cap, stdin) > 0) {
        char *p = ln;
        int a = e.curr, b = e.curr, has_a = 0;
        int is_q = 0;

        while (isspace(*p) && *p != '\n') p++;

        if (*p == ',') { a = 1; b = (int)e.count; p++; has_a = 1; }
        else if (*p == '%') { a = 1; b = (int)e.count; p++; has_a = 1; }
        else if (get_addr(&e, &p, &a)) {
            has_a = 1; b = a;
            while (isspace(*p)) p++;
            if (*p == ',' || *p == ';') {
                if (*p == ';') e.curr = a;
                p++;
                if (!get_addr(&e, &p, &b)) b = (int)e.count;
            }
        }

        char *cmd_p = p;
        while (isspace(*cmd_p) && *cmd_p != '\n') cmd_p++;

        if (*cmd_p == 'g' || *cmd_p == 'v') {
            char *full_cmd = strdup(p);
            size_t len = strlen(full_cmd);

            /* POSIX Multiline Command Collection */
            while (len > 1 && full_cmd[len-2] == '\\') {
                full_cmd[len-2] = '\0'; 
                char *next = NULL; size_t n_cap = 0;
                if (getline(&next, &n_cap, stdin) <= 0) break;
                
                full_cmd = realloc(full_cmd, len + strlen(next) + 1);
                strcat(full_cmd, next);
                len = strlen(full_cmd);
                free(next);
            }
            
            // Advance past leading spaces to the actual 'g' or 'v' char, 
            // then pass the rest of the string to do_global
            char *actual_cmd = full_cmd;
            while(isspace(*actual_cmd)) actual_cmd++;
            
            do_global(&e, a, b, actual_cmd + 1, (*actual_cmd == 'v'));
            free(full_cmd);
        } else {
            exec_cmd(&e, a, b, p, &is_q, has_a);
        }

        if (is_q) goto end;
        if (*cmd_p != 'q') e.warned = 0;
        if (e.prompt) fputs("*", stdout);
    }

end:
    free(ln);
    ed_free(&e);
    return 0;
}
