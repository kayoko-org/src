#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>

#define BUFFER_SIZE 4096

void copy_file(const char *src, const char *dst) {
    int src_fd, dst_fd;
    ssize_t n_read;
    char buffer[BUFFER_SIZE];
    struct stat st;

    // 1. Open source and get its permissions
    if ((src_fd = open(src, O_RDONLY)) < 0) {
        perror("cp: cannot open source");
        exit(1);
    }

    if (fstat(src_fd, &st) < 0) {
        perror("cp: cannot stat source");
        close(src_fd);
        exit(1);
    }

    // 2. Open destination (Create if doesn't exist, truncate if it does)
    // S_IRUSR | S_IWUSR etc. are the POSIX permissions from the source
    dst_fd = open(dst, O_WRONLY | O_CREAT | O_TRUNC, st.st_mode);
    if (dst_fd < 0) {
        perror("cp: cannot create destination");
        close(src_fd);
        exit(1);
    }

    // 3. The Core Loop: Read/Write
    while ((n_read = read(src_fd, buffer, BUFFER_SIZE)) > 0) {
        if (write(dst_fd, buffer, n_read) != n_read) {
            perror("cp: write error");
            break;
        }
    }

    if (n_read < 0) perror("cp: read error");

    // 4. Cleanup
    close(src_fd);
    close(dst_fd);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s src target\n", argv[0]);
        return 1;
    }

    copy_file(argv[1], argv[2]);
    return 0;
}
