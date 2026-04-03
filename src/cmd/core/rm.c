#define _XOPEN_SOURCE 500
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ftw.h>
#include <errno.h>

int opt_f = 0;

int rm_fn(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    int rv = remove(fpath);
    if (rv != 0 && !opt_f) {
        perror(fpath);
    }
    return 0; // Continue even if one file fails
}

int main(int argc, char *argv[]) {
    int opt, recursive = 0;

    while ((opt = getopt(argc, argv, "frR")) != -1) {
        switch (opt) {
            case 'f': opt_f = 1; break;
            case 'r':
            case 'R': recursive = 1; break;
            default:
                fprintf(stderr, "usage: rm [-frR] file ...\n");
                return 1;
        }
    }

    argc -= optind;
    argv += optind;

    if (argc == 0 && !opt_f) {
        fprintf(stderr, "usage: rm [-frR] file ...\n");
        return 1;
    }

    for (int i = 0; i < argc; i++) {
        struct stat st;
        if (lstat(argv[i], &st) == -1) {
            if (!opt_f) perror(argv[i]);
            continue;
        }

        if (S_ISDIR(st.st_mode) && !recursive) {
            fprintf(stderr, "rm: %s: is a directory\n", argv[i]);
            continue;
        }

        if (recursive) {
            /* FTW_DEPTH: process children before directory itself
               FTW_PHYS:  don't follow symlinks (prevents escaping) */
            nftw(argv[i], rm_fn, 64, FTW_DEPTH | FTW_PHYS);
        } else {
            if (unlink(argv[i]) == -1 && !opt_f) {
                perror(argv[i]);
            }
        }
    }

    return 0;
}
