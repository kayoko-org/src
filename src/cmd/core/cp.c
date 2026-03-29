#define _XOPEN_SOURCE 700 /* Required for nftw */
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

/* --- The original robust copy_file logic --- */
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

/* --- The NFTW Callback --- */
int copy_callback(const char *fpath, const struct stat *sb, int tflag, struct FTW *ftwbuf) {
    char target_path[PATH_MAX];
    
    // Construct target path by replacing the src_root prefix with dst_root
    snprintf(target_path, sizeof(target_path), "%s/%s", 
             global_dst_root, fpath + ftwbuf->base);

    if (tflag == FTW_D) {
        // It's a directory: create it with the same permissions
        if (mkdir(target_path, sb->st_mode) < 0 && errno != EEXIST) {
            perror("mkdir failed");
            return -1;
        }
    } else if (tflag == FTW_F) {
        // It's a file: copy it
        if (copy_file_internal(fpath, target_path, sb->st_mode) < 0) {
            fprintf(stderr, "Failed to copy %s to %s: %s\n", fpath, target_path, strerror(errno));
            return -1;
        }
    }
    return 0;
}

int main(int argc, char *argv[]) {
    int recursive = 0;
    char *src, *dst;

    if (argc == 4 && strcmp(argv[1], "-R") == 0) {
        recursive = 1;
        src = argv[2];
        dst = argv[3];
    } else if (argc == 3) {
        src = argv[1];
        dst = argv[2];
    } else {
        fprintf(stderr, "usage: %s [-R] src target\n", argv[0]);
        return 1;
    }

    struct stat st_src, st_dst;
    if (stat(src, &st_src) < 0) { perror("stat src"); return 1; }

    // If recursive, prepare the NFTW roots
    if (recursive && S_ISDIR(st_src.st_mode)) {
        strncpy(global_src_root, src, PATH_MAX);
        strncpy(global_dst_root, dst, PATH_MAX);

        // Create the top-level destination directory if it doesn't exist
        if (mkdir(dst, st_src.st_mode) < 0 && errno != EEXIST) {
            perror("mkdir dst");
            return 1;
        }

        // Walk the tree: 20 is the max open file descriptors for the walk
        if (nftw(src, copy_callback, 20, FTW_PHYS) < 0) {
            perror("nftw failed");
            return 1;
        }
    } else {
        // Standard non-recursive copy logic (handling the dir-target case)
        char final_dst[PATH_MAX];
        if (stat(dst, &st_dst) == 0 && S_ISDIR(st_dst.st_mode)) {
            char src_copy[PATH_MAX];
            strncpy(src_copy, src, PATH_MAX);
            snprintf(final_dst, sizeof(final_dst), "%s/%s", dst, basename(src_copy));
            dst = final_dst;
        }
        if (copy_file_internal(src, dst, st_src.st_mode) < 0) {
            perror("copy failed");
            return 1;
        }
    }

    return 0;
}
