#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    FILE *f1, *f2;
    int c1, c2;
    size_t byte = 1, line = 1;
    int sflag = 0;

    while ((c1 = getopt(argc, argv, "ls")) != -1) {
        if (c1 == 's') sflag = 1;
        else return 2; // 'l' omitted for brevity in this draft
    }

    if (argc - optind < 2) return 2;

    if (!(f1 = fopen(argv[optind], "r")) || !(f2 = fopen(argv[optind+1], "r"))) {
        perror("cmp");
        return 2;
    }

    for (;;) {
        c1 = getc(f1);
        c2 = getc(f2);

        if (c1 == c2) {
            if (c1 == EOF) return 0;
            if (c1 == '\n') line++;
            byte++;
            continue;
        }

        if (!sflag) {
            if (c1 == EOF || c2 == EOF) {
                fprintf(stderr, "cmp: EOF on %s\n", c1 == EOF ? argv[optind] : argv[optind+1]);
            } else {
                printf("%s %s differ: char %zu, line %zu\n", argv[optind], argv[optind+1], byte, line);
            }
        }
        return 1;
    }
}
