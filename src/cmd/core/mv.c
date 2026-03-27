#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>

#define BUFFER_SIZE 4096

/* Fallback: Copy logic if rename() fails (Cross-device move) */
int copy_and_unlink(const char *src, const char *dst) {
    int src_fd, dst_fd;
    ssize_t n_read;
    char buffer[BUFFER_SIZE];
    struct stat st;

    if ((src_fd = open(src, O_RDONLY)) < 0) return -1;
    if (fstat(src_fd, &st) < 0) { close(src_fd); return -1; }

    dst_fd = open(dst, O_WRONLY | O_CREAT | O_TRUNC, st.st_mode);
    if (dst_fd < 0) { close(src_fd); return -1; }

    while ((n_read = read(src_fd, buffer, BUFFER_SIZE)) > 0) {
        if (write(dst_fd, buffer, n_read) != n_read) {
            close(src_fd); close(dst_fd);
            return -1;
        }
    }

    close(src_fd);
    close(dst_fd);

    // If copy succeeded, delete the original
    if (unlink(src) < 0) {
        perror("mv: warning: copy succeeded but failed to remove source");
        return -1;
    }
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s source target\n", argv[0]);
        return 1;
    }

    const char *src = argv[1];
    const char *dst = argv[2];

    // Attempt 1: Fast rename (Same filesystem)
    if (rename(src, dst) == 0) {
        return 0;
    }

    // Attempt 2: Handle cross-device move (EXDEV error)
    if (errno == EXDEV) {
        if (copy_and_unlink(src, dst) == 0) {
            return 0;
        }
    }

    // If we reached here, something went wrong
    perror("mv");
    return 1;
}
