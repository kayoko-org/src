#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

int main(int argc, char *argv[]) {
    char *shell;

    if (argc < 2) {
        fprintf(stderr, "usage: chroot newroot [command [arg ...]]\n");
        return 125;
    }

    if (chroot(argv[1]) != 0) {
        fprintf(stderr, "chroot: %s: %s\n", argv[1], strerror(errno));
        return 125;
    }

    if (argc == 2) {
        shell = getenv("SHELL");
        if (!shell || !*shell) {
            shell = "/bin/sh";
        }
        execlp(shell, shell, (char *)NULL);
    } else {
        execvp(argv[2], &argv[2]);
    }

    /* If we reach here, exec failed */
    fprintf(stderr, "chroot: %s: %s\n", (argc == 2) ? shell : argv[2], strerror(errno));
    return (errno == ENOENT) ? 127 : 126;
}
