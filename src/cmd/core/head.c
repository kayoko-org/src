#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void head(FILE *fp, long n) {
    int ch;
    while (n > 0 && (ch = getc(fp)) != EOF) {
        putchar(ch);
        if (ch == '\n') n--;
    }
}

int main(int argc, char *argv[]) {
    long n = 10;
    int opt;

    while ((opt = getopt(argc, argv, "n:")) != -1) {
        if (opt == 'n') n = atol(optarg);
        else return 1;
    }

    argc -= optind;
    argv += optind;

    if (argc == 0) {
        head(stdin, n);
    } else {
        for (int i = 0; i < argc; i++) {
            FILE *fp = fopen(argv[i], "r");
            if (!fp) {
                perror(argv[i]);
                continue;
            }
            head(fp, n);
            fclose(fp);
        }
    }
    return 0;
}
