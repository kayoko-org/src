#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int mkpath(char *path, mode_t mode);

void usage(void) {
    fprintf(stderr, "usage: mkdir [-p] [-m mode] directory ...\n");
    exit(1);
}

int main(int argc, char *argv[]) {
    int pflag = 0;
    mode_t mode = 0777;
    int ch, error = 0;
    char *ep;

    while ((ch = getopt(argc, argv, "pm:")) != -1) {
        switch (ch) {
        case 'p':
            pflag = 1;
            break;
        case 'm':
            mode = strtol(optarg, &ep, 8);
            if (*ep != '\0') {
                fprintf(stderr, "mkdir: invalid file mode: %s\n", optarg);
                exit(1);
            }
            break;
        default:
            usage();
        }
    }

    argc -= optind;
    argv += optind;

    if (argc < 1) usage();

    for (; *argv; ++argv) {
        if (pflag) {
            if (mkpath(*argv, mode) != 0) error = 1;
        } else {
            if (mkdir(*argv, mode) != 0) {
                perror(*argv);
                error = 1;
            }
        }
    }

    return error;
}

static int mkpath(char *path, mode_t mode) {
    char *p;
    struct stat sb;

    for (p = path + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            /* Intermediate dirs: POSIX says rwxrwxrwx & ~umask */
            if (mkdir(path, S_IRWXU | S_IRWXG | S_IRWXO) != 0 && errno != EEXIST) {
                perror(path);
                return -1;
            }
            *p = '/';
        }
    }

    /* Final component */
    if (mkdir(path, mode) != 0) {
        if (errno != EEXIST) {
            perror(path);
            return -1;
        }
        /* If EEXIST, POSIX requires we verify it is actually a directory */
        if (stat(path, &sb) != 0 || !S_ISDIR(sb.st_mode)) {
            fprintf(stderr, "mkdir: %s: File exists\n", path);
            return -1;
        }
    }
    return 0;
}
