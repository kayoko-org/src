#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <kayoko/textproc/ed/buffer.h>
#include <kayoko/textproc/regex/bre.h>

/* Stub to satisfy the linker for shared ed logic */
void set_err(Ed *e, char *msg) {
    (void)e;
    fprintf(stderr, "sed: %s\n", msg);
    exit(1);
}

void process_stream(FILE *conf, char *script, int nflag) {
    char *line = NULL;
    size_t len = 0;
    size_t lno = 0;

    /* Pattern Space */
    Ed e = {0};
    e.lines = calloc(1, sizeof(Line));
    e.count = 1;

    while (getline(&line, &len, conf) != -1) {
        lno++;
        line[strcspn(line, "\n")] = 0;
        update_line_text(&e.lines[0], line);

        char *p = script;
        int a = 1, b = (int)lno;

        /* Basic Address Parsing: '1,5s/...' or '10s/...' */
        if (isdigit(*p)) {
            a = strtol(p, &p, 10);
            if (*p == ',') {
                p++;
                b = strtol(p, &p, 10);
            } else {
                b = a;
            }
        }

        int match = (lno >= (size_t)a && lno <= (size_t)b);
        int deleted = 0;

        if (match) {
            if (*p == 's') {
                /* Skip 's' so do_sub sees the delimiter */
                do_sub(&e, 1, 1, p + 1);
            } else if (*p == 'd') {
                deleted = 1;
            } else if (*p == 'p') {
                printf("%s\n", e.lines[0].rs->data);
            }
        }

        if (!deleted && !nflag) {
            printf("%s\n", e.lines[0].rs->data);
        }
    }
    free(line);
    free(e.lines);
}

int main(int argc, char **argv) {
    int opt, nflag = 0;

    while ((opt = getopt(argc, argv, "n")) != -1) {
        if (opt == 'n') nflag = 1;
        else return 1;
    }

    if (optind >= argc) {
        fprintf(stderr, "usage: sed [-n] script [file ...]\n");
        return 1;
    }

    char *script = argv[optind++];

    /* If no files provided, read from stdin */
    if (optind == argc) {
        process_stream(stdin, script, nflag);
    } else {
        /* Iterate through all file arguments */
        for (int i = optind; i < argc; i++) {
            FILE *f = fopen(argv[i], "r");
            if (!f) {
                perror(argv[i]);
                continue;
            }
            process_stream(f, script, nflag);
            fclose(f);
        }
    }

    return 0;
}
