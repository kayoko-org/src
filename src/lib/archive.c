#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <utime.h>
#include <pwd.h>
#include <grp.h>
#include <dirent.h>
#include <sys/stat.h>

#include <kayoko/files/archive/pmain.h>

/**
 * UTILITY FUNCTIONS
 * These provide the low-level formatting for USTAR and CPIO.
 */

void pax_format_octal(char *dest, int len, long val) {
    snprintf(dest, len, "%0*lo", (int)len - 1, val);
}

void pax_set_chksum(struct ustar_hdr *h) {
    memset(h->chksum, ' ', 8);
    unsigned int sum = 0;
    unsigned char *p = (unsigned char *)h;
    for (int i = 0; i < BLKSIZE; i++) {
        sum += p[i];
    }
    snprintf(h->chksum, 8, "%06o", sum & 0777777);
    h->chksum[6] = '\0';
    h->chksum[7] = ' ';
}

int pax_split_path(const char *path, struct ustar_hdr *h) {
    size_t len = strlen(path);
    if (len <= 100) {
        strncpy(h->name, path, 100);
        return 0;
    }
    /* Try to split into prefix/name for USTAR POSIX compatibility */
    for (int i = (int)len - 1; i >= 0; i--) {
        if (path[i] == '/' && i <= 155 && (len - i - 1) <= 100) {
            strncpy(h->prefix, path, i);
            strncpy(h->name, path + i + 1, 100);
            return 0;
        }
    }
    return -1;
}

void cpio_format_hex(char *dest, unsigned long val) {
    snprintf(dest, 9, "%08lx", val);
}

void cpio_write_padding(int fd, size_t current_size) {
    size_t p = (4 - (current_size % 4)) % 4;
    if (p > 0) {
        char zero[4] = {0, 0, 0, 0};
        write(fd, zero, p);
    }
}

/**
 * INTERNAL ARCHIVE HELPERS
 */

static void _write_cpio_entry(int arch_fd, const char *path, struct stat *st) {
    struct cpio_hdr ch;
    size_t namesize = strlen(path) + 1;

    memset(&ch, '0', sizeof(ch));
    memcpy(ch.c_magic, CPIO_MAGIC, 6);

    cpio_format_hex(ch.c_ino, st->st_ino);
    cpio_format_hex(ch.c_mode, st->st_mode);
    cpio_format_hex(ch.c_uid, st->st_uid);
    cpio_format_hex(ch.c_gid, st->st_gid);
    cpio_format_hex(ch.c_nlink, st->st_nlink);
    cpio_format_hex(ch.c_mtime, st->st_mtime);
    cpio_format_hex(ch.c_filesize, S_ISREG(st->st_mode) ? st->st_size : 0);
    cpio_format_hex(ch.c_devmajor, 0);
    cpio_format_hex(ch.c_devminor, 0);
    cpio_format_hex(ch.c_rdevmajor, 0);
    cpio_format_hex(ch.c_rdevminor, 0);
    cpio_format_hex(ch.c_namesize, namesize);
    cpio_format_hex(ch.c_check, 0);

    write(arch_fd, &ch, sizeof(ch));
    write(arch_fd, path, namesize);
    cpio_write_padding(arch_fd, sizeof(ch) + namesize);
}

static void _write_ustar_entry(int arch_fd, const char *path, struct stat *st) {
    struct ustar_hdr uh;
    memset(&uh, 0, sizeof(uh));
    
    if (pax_split_path(path, &uh) != 0) {
        fprintf(stderr, "pax: path too long for ustar: %s\n", path);
        return;
    }

    pax_format_octal(uh.mode, 8, st->st_mode & 07777);
    pax_format_octal(uh.uid, 8, st->st_uid);
    pax_format_octal(uh.gid, 8, st->st_gid);
    pax_format_octal(uh.size, 12, S_ISREG(st->st_mode) ? st->st_size : 0);
    pax_format_octal(uh.mtime, 12, st->st_mtime);

    uh.type = S_ISDIR(st->st_mode) ? TDIR : TVREG;
    memcpy(uh.magic, "ustar", 6);
    memcpy(uh.version, "00", 2);

    struct passwd *pw = getpwuid(st->st_uid);
    struct group *gr = getgrgid(st->st_gid);
    if (pw) strncpy(uh.uname, pw->pw_name, 32);
    if (gr) strncpy(uh.gname, gr->gr_name, 32);

    pax_set_chksum(&uh);
    write(arch_fd, &uh, BLKSIZE);
}

/**
 * PUBLIC API
 */

void pax_make_intermediate_dir(const char *path) {
    char buf[4096];
    char *p = NULL;
    snprintf(buf, sizeof(buf), "%s", path);
    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '/') buf[len - 1] = 0;

    for (p = buf + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            mkdir(buf, 0777);
            *p = '/';
        }
    }
}

void pax_archive_file(int arch_fd, const char *path) {
    struct stat st;

    /* Manual check for CPIO Trailer string */
    if (global_fmt == FMT_CPIO && strcmp(path, CPIO_TRAILER) == 0) {
        memset(&st, 0, sizeof(st));
        st.st_nlink = 1;
        _write_cpio_entry(arch_fd, path, &st);
        return;
    }

    if (lstat(path, &st) == -1) {
        fprintf(stderr, "pax: cannot stat %s: %s\n", path, strerror(errno));
        return;
    }

    /* 1. Write Header */
    if (global_fmt == FMT_CPIO) {
        _write_cpio_entry(arch_fd, path, &st);
    } else {
        _write_ustar_entry(arch_fd, path, &st);
    }

    /* 2. Write Data */
    if (S_ISREG(st.st_mode)) {
        int fd = open(path, O_RDONLY);
        if (fd >= 0) {
            char buf[BLKSIZE];
            ssize_t n;
            size_t total_written = 0;

            while ((n = read(fd, buf, BLKSIZE)) > 0) {
                if (global_fmt == FMT_USTAR) {
                    if (n < BLKSIZE) memset(buf + n, 0, (size_t)(BLKSIZE - n));
                    write(arch_fd, buf, BLKSIZE);
                } else {
                    write(arch_fd, buf, (size_t)n);
                }
                total_written += (size_t)n;
            }
            if (global_fmt == FMT_CPIO) {
                cpio_write_padding(arch_fd, total_written);
            }
            close(fd);
        }
    } else if (S_ISDIR(st.st_mode)) {
        DIR *d = opendir(path);
        struct dirent *de;
        if (d) {
            while ((de = readdir(d))) {
                if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) continue;
                char sub_path[4096];
                snprintf(sub_path, sizeof(sub_path), "%s/%s", path, de->d_name);
                pax_archive_file(arch_fd, sub_path);
            }
            closedir(d);
        }
    }
}

void pax_do_read_list(int fd, int extract) {
    if (global_fmt == FMT_USTAR) {
        struct ustar_hdr h;
        char buf[BLKSIZE];
        while (read(fd, &h, BLKSIZE) == BLKSIZE && h.name[0] != '\0') {
            char full[256] = {0};
            if (h.prefix[0]) snprintf(full, 256, "%s/%s", h.prefix, h.name);
            else strncpy(full, h.name, 100);

            if (vflag || !extract) printf("%s\n", full);

            long size = strtol(h.size, NULL, 8);
            if (extract) {
                mode_t mode = strtol(h.mode, NULL, 8);
                pax_make_intermediate_dir(full);
                if (h.type == TDIR) {
                    mkdir(full, mode);
                } else {
                    int out = open(full, O_WRONLY|O_CREAT|O_TRUNC, mode);
                    long rem = size;
                    while (rem > 0) {
                        ssize_t r = read(fd, buf, BLKSIZE);
                        if (r <= 0) break;
                        if (out >= 0) write(out, buf, (rem < r ? rem : r));
                        rem -= r;
                    }
                    if (out >= 0) close(out);
                }
            } else {
                lseek(fd, ((size + BLKSIZE - 1) / BLKSIZE) * BLKSIZE, SEEK_CUR);
            }
        }
    } else {
        /* CPIO extraction logic would be implemented here */
    }
}

void pax_copy_node(const char *src, const char *dest_folder) {
    struct stat st;
    if (lstat(src, &st) == -1) return;

    char target[4096];
    snprintf(target, sizeof(target), "%s/%s", dest_folder, src);
    pax_make_intermediate_dir(target);

    if (S_ISDIR(st.st_mode)) {
        mkdir(target, (st.st_mode & 07777) | 0700);
        DIR *d = opendir(src);
        struct dirent *de;
        if (d) {
            while ((de = readdir(d))) {
                if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) continue;
                char sub[4096];
                snprintf(sub, sizeof(sub), "%s/%s", src, de->d_name);
                pax_copy_node(sub, dest_folder);
            }
            closedir(d);
        }
    } else if (S_ISREG(st.st_mode)) {
        int sfd = open(src, O_RDONLY);
        int dfd = open(target, O_WRONLY | O_CREAT | O_TRUNC, st.st_mode & 07777);
        if (sfd >= 0 && dfd >= 0) {
            char buf[32256];
            ssize_t n;
            while ((n = read(sfd, buf, sizeof(buf))) > 0) write(dfd, buf, n);
        }
        if (sfd >= 0) close(sfd);
        if (dfd >= 0) close(dfd);
    }

    chmod(target, st.st_mode & 07777);
    struct utimbuf times = { .actime = st.st_atime, .modtime = st.st_mtime };
    utime(target, &times);
}
