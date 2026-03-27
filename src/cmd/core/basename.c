#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <string.h>
#include <libgen.h>

int main(int argc, char *argv[]) {
    if (argc < 2 || argc > 3) {
        fprintf(stderr, "usage: basename string [suffix]\n");
        return 1;
    }

    char *bname = basename(argv[1]);
    if (argc == 3) {
        size_t blen = strlen(bname);
        size_t slen = strlen(argv[2]);
        if (blen > slen && strcmp(bname + blen - slen, argv[2]) == 0) {
            bname[blen - slen] = '\0';
        }
    }

    printf("%s\n", bname);
    return 0;
}
