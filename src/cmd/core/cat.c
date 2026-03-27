#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

void concat(int fdin, const char *filename) {
    char buf[4096];
    ssize_t n;

    while ((n = read(fdin, buf, sizeof(buf))) > 0) {
        if (write(STDOUT_FILENO, buf, n) != n) {
            perror("cat: write error");
            return;
        }
    }
    if (n < 0) {
        perror(filename);
    }
}

int main(int argc, char *argv[]) {
    int opt, i, fd;
    
    /* POSIX cat only requires -u for unbuffered output */
    while ((opt = getopt(argc, argv, "u")) != -1) {
        if (opt != 'u') {
            fprintf(stderr, "usage: cat [-u] [file ...]\n");
            return 1;
        }
    }

    if (optind == argc) {
        concat(STDIN_FILENO, "stdin");
    } else {
        for (i = optind; i < argc; i++) {
            if (argv[i][0] == '-' && argv[i][1] == '\0') {
                fd = STDIN_FILENO;
            } else {
                fd = open(argv[i], O_RDONLY);
                if (fd < 0) {
                    perror(argv[i]);
                    continue;
                }
            }
            concat(fd, argv[i]);
            if (fd != STDIN_FILENO) close(fd);
        }
    }

    return 0;
}
