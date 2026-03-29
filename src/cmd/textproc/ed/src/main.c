#include <kayoko/textproc/ed.h>
#include <kayoko/textproc/ed/buffer.h>
#include <kayoko/textproc/ed/commands.h>
#include <kayoko/textproc/ed/parser.h>
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
