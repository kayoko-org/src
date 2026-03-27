#include <stdio.h>
#include <string.h>
#include <libgen.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "usage: dirname string\n");
        return 1;
    }

    char *path = argv[1];
    size_t len = strlen(path);

    /* POSIX step 1: If string is empty, return "." */
    if (len == 0) {
        printf(".\n");
        return 0;
    }

    /* POSIX step 2: Remove trailing slashes */
    while (len > 1 && path[len - 1] == '/') {
        len--;
    }

    /* POSIX step 3: If string is all slashes, return "/" */
    int all_slashes = 1;
    for (size_t i = 0; i < len; i++) {
        if (path[i] != '/') {
            all_slashes = 0;
            break;
        }
    }
    if (all_slashes) {
        printf("/\n");
        return 0;
    }

    /* POSIX step 4 & 5: Find the last slash */
    char *last_slash = NULL;
    for (size_t i = 0; i < len; i++) {
        if (path[i] == '/') {
            last_slash = &path[i];
        }
    }

    if (last_slash != NULL) {
        /* Handle multiple slashes before the last component */
        while (last_slash > path && *(last_slash - 1) == '/') {
            last_slash--;
        }
        
        if (last_slash == path) {
            printf("/\n");
        } else {
            /* Print everything up to the last slash */
            printf("%.*s\n", (int)(last_slash - path), path);
        }
    } else {
        /* POSIX step 7: If no slashes remain, return "." */
        printf(".\n");
    }

    return 0;
}
