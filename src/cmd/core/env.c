#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

extern char **environ;

int main(int argc, char *argv[]) {
    int opt;
    char **ep;

    /* Handle the -i (ignore environment) flag */
    while ((opt = getopt(argc, argv, "i")) != -1) {
        switch (opt) {
            case 'i':
                *environ = NULL;
                break;
            default:
                fprintf(stderr, "usage: env [-i] [name=value ...] [utility [argument ...]]\n");
                return 1;
        }
    }
    argc -= optind;
    argv += optind;

    /* Handle name=value assignments */
    while (*argv && strchr(*argv, '=')) {
        if (putenv(*argv) != 0) {
            perror("putenv");
            return 1;
        }
        argv++;
    }

    /* If no utility is specified, print the environment and exit */
    if (*argv == NULL) {
        for (ep = environ; *ep; ep++) {
            printf("%s\n", *ep);
        }
        return 0;
    }

    /* Execute the utility */
    execvp(*argv, argv);

    /* If we get here, execvp failed */
    perror(*argv);
    return 127;
}
