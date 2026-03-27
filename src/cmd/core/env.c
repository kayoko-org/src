#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern char **environ;

int main(int argc, char *argv[]) {
    int opt;
    static char *empty[] = { NULL };

    /* * POSIX requires -i to ignore the inherited environment.
     * We point environ to an empty array to be safer than just setting *environ = NULL.
     */
    while ((opt = getopt(argc, argv, "i")) != -1) {
        switch (opt) {
            case 'i':
                environ = empty;
                break;
            default:
                fprintf(stderr, "usage: env [-i] [name=value ...] [utility [argument ...]]\n");
                return 1;
        }
    }
    argv += optind;

    /* * Handle name=value assignments. 
     * POSIX says these must precede the utility name.
     */
    while (*argv && strchr(*argv, '=')) {
        if (putenv(*argv) != 0) {
            perror("putenv");
            return 1;
        }
        argv++;
    }

    /* * If no utility is specified, print the resulting environment and exit.
     * puts() is used here for a slightly leaner line count than printf.
     */
    if (*argv == NULL) {
        char **ep;
        for (ep = environ; *ep; ep++) {
            puts(*ep);
        }
        return 0;
    }

    /* * Execute the utility with the modified environment.
     */
    execvp(*argv, argv);

    /* * If execvp returns, an error occurred.
     * POSIX 127: Utility not found.
     * POSIX 126: Utility found but not executable.
     */
    fprintf(stderr, "env: %s: %s\n", *argv, strerror(errno));
    return (errno == ENOENT) ? 127 : 126;
}
