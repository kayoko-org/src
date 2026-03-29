#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <libgen.h>
#include <limits.h>
#include <ftw.h>

char global_dst_root[PATH_MAX];
char global_src_root[PATH_MAX];
dev_t src_dev;

/* --- Robust File Copy (for Cross-Device Move) --- */
int copy_file_internal(const char *src, const char *dst, mode_t mode) {
    int src_fd, dst_fd;
    ssize_t n_read, n_written;
    char buffer[8192];

    if ((src_fd = open(src, O_RDONLY)) < 0) return -1;
    dst_fd = open(dst, O_WRONLY | O_CREAT | O_TRUNC, mode);
    if (dst_fd < 0) { close(src_fd); return -1; }

    while ((n_read = read(src_fd, buffer, sizeof(buffer))) > 0) {
        char *ptr = buffer;
        while (n_read > 0) {
            n_written = write(dst_fd, ptr, n_read);
            if (n_written <= 0) {
                if (errno == EINTR) continue;
                close(src_fd); close(dst_fd); return -1;
            }
            n_read -= n_written;
            ptr += n_written;
        }
    }
    close(src_fd);
    return close(dst_fd);
}

/* --- Callback for Cross-Device recursive move --- */
int move_callback(const char *fpath, const struct stat *sb, int tflag, struct FTW *ftwbuf) {
    char target_path[PATH_MAX];
    snprintf(target_path, sizeof(target_path), "%s/%s", global_dst_root, fpath + ftwbuf->base);

    if (tflag == FTW_DP) { // Directory, Post-order (children already handled)
        if (mkdir(target_path, sb->st_mode) < 0 && errno != EEXIST) return -1;
        return rmdir(fpath); // Delete source dir after copying contents
    } else if (tflag == FTW_F || tflag == FTW_SL) { // File or Symlink
        if (copy_file_internal(fpath, target_path, sb->st_mode) < 0) return -1;
        return unlink(fpath); // Delete source file after successful copy
    }
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s src target\n", argv[0]);
        return 1;
    }

    char *src = argv[1];
    char *dst = argv[2];
    struct stat st_src, st_dst;
    char final_dst[PATH_MAX];

    if (lstat(src, &st_src) < 0) { perror("mv: lstat src"); return 1; }

    // Handle case where destination is a directory (e.g., mv file /tmp/)
    if (stat(dst, &st_dst) == 0 && S_ISDIR(st_dst.st_mode)) {
        char src_copy[PATH_MAX];
        strncpy(src_copy, src, PATH_MAX - 1);
        src_copy[PATH_MAX - 1] = '\0';
        snprintf(final_dst, sizeof(final_dst), "%s/%s", dst, basename(src_copy));
        dst = final_dst;
    }

    // 1. Try atomic rename (works for files and dirs on same device)
    if (rename(src, dst) == 0) return 0;

    // 2. Handle Cross-Device Move (EXDEV)
    if (errno == EXDEV) {
        strncpy(global_src_root, src, PATH_MAX);
        strncpy(global_dst_root, dst, PATH_MAX);

        // FTW_DEPTH ensures we process directory contents before the directory itself
        // FTW_PHYS ensures we don't follow symlinks into infinite loops
        if (nftw(src, move_callback, 20, FTW_DEPTH | FTW_PHYS) < 0) {
            fprintf(stderr, "mv: cross-device move failed: %s\n", strerror(errno));
            return 1;
        }
    } else {
        perror("mv: rename failed");
        return 1;
    }

    return 0;
}
