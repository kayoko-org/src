#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>

void vis(FILE *fp) {
    int ch;
    while ((ch = fgetc(fp)) != EOF) {
        if (isprint(ch) || isspace(ch)) {
            /* Keep it readable, but you could escape space/newline if needed */
            putchar(ch);
        } else {
            /* The classic BSD/C-style escape logic */
            switch (ch) {
                case '\n': printf("\\n"); break;
                case '\r': printf("\\r"); break;
                case '\t': printf("\\t"); break;
                case '\a': printf("\\a"); break;
                case '\b': printf("\\b"); break;
                case '\v': printf("\\v"); break;
                case '\f': printf("\\f"); break;
                case '\\': printf("\\\\"); break;
                default:
                    /* Fallback to octal for truly weird bytes */
                    printf("\\%03o", ch);
            }
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc == 1) {
        vis(stdin);
    } else {
        for (int i = 1; i < argc; i++) {
            FILE *fp = fopen(argv[i], "r");
            if (!fp) {
                perror(argv[i]);
                continue;
            }
            vis(fp);
            fclose(fp);
        }
    }
    return 0;
}
