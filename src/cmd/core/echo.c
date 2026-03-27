#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[]) {
    int opt, i, fd;
    int exit_status = 0;

    while ((opt = getopt(argc, argv, "u")) != -1) {
        switch (opt) {
            case 'u': 
                /* Data is already unbuffered via read/write */
                break;
            default:
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
                    exit_status = 1; // Mark failure but keep going
                    continue;
                }
            }
            concat(fd, argv[i]);
            if (fd != STDIN_FILENO) close(fd);
        }
    }

    return exit_status;
}

