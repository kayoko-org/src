#include <kayoko/textproc/ed.h>
#include <kayoko/textproc/ed/buffer.h>
#include <kayoko/textproc/ed/commands.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/wait.h>
#include <unistd.h>
#include <regex.h>


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
		while (isspace(*p)) {
			    p++;
		}
		p[strcspn(p, "\n")] = 0;
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
	while (isspace(*p)) {
	    p++;
	}
	p[strcspn(p, "\n")] = 0;
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
