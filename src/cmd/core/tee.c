#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

int main(int argc, char *argv[]) {
    int append = 0;
    int opt;
    int *fds;
    int nfds = 0;
    char buf[4096];
    ssize_t nread, nwritten;

    /* POSIX only requires -a (append) and -i (ignore interrupts) */
    /* We'll focus on -a; -i is often a shell builtin or handled via signal() */
    while ((opt = getopt(argc, argv, "ai")) != -1) {
        switch (opt) {
            case 'a': append = 1; break;
            case 'i': /* Ignore SIGINT is optional for minimalists but POSIX */
                signal(SIGINT, SIG_IGN);
                break;
            default:
                fprintf(stderr, "usage: tee [-ai] [file ...]\n");
                return 1;
        }
    }

    argc -= optind;
    argv += optind;

    /* Allocate array for file descriptors + 1 for stdout */
    fds = malloc((argc + 1) * sizeof(int));
    if (!fds) {
        perror("malloc");
        return 1;
    }

    fds[nfds++] = STDOUT_FILENO;

    for (int i = 0; i < argc; i++) {
        int flags = O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC);
        int fd = open(argv[i], flags, 0666);
        if (fd < 0) {
            perror(argv[i]);
        } else {
            fds[nfds++] = fd;
        }
    }

    while ((nread = read(STDIN_FILENO, buf, sizeof(buf))) > 0) {
        for (int i = 0; i < nfds; i++) {
            ssize_t total_w = 0;
            while (total_w < nread) {
                nwritten = write(fds[i], buf + total_w, nread - total_w);
                if (nwritten < 0) {
                    perror("write error");
                    break;
                }
                total_w += nwritten;
            }
        }
    }

    /* Clean up */
    for (int i = 1; i < nfds; i++) {
        close(fds[i]);
    }
    free(fds);

    return 0;
}
