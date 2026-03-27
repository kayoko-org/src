#include "pax.h"
#include "pax.utils.h"
#include <utime.h>

void archive_file(int arch_fd, const char *path) {
    struct stat st;
    if (lstat(path, &st) == -1) return;
    struct ustar_hdr h;
    memset(&h, 0, sizeof(h));
    if (pax_split_path(path, &h) != 0) return;

    pax_format_octal(h.mode, 8, st.st_mode & 07777);
    pax_format_octal(h.uid, 8, st.st_uid);
    pax_format_octal(h.gid, 8, st.st_gid);
    pax_format_octal(h.size, 12, S_ISREG(st.st_mode) ? st.st_size : 0);
    pax_format_octal(h.mtime, 12, st.st_mtime);
    h.type = S_ISDIR(st.st_mode) ? '5' : '0';
    memcpy(h.magic, "ustar", 6);
    memcpy(h.version, "00", 2);
    
    struct passwd *pw = getpwuid(st.st_uid);
    struct group *gr = getgrgid(st.st_gid);
    if (pw) strncpy(h.uname, pw->pw_name, 32);
    if (gr) strncpy(h.gname, gr->gr_name, 32);
    
    pax_set_chksum(&h);
    write(arch_fd, &h, BLKSIZE);

    if (S_ISREG(st.st_mode)) {
        int fd = open(path, O_RDONLY);
        char buf[BLKSIZE];
        ssize_t n;
        while ((n = read(fd, buf, BLKSIZE)) > 0) {
            if (n < BLKSIZE) memset(buf + n, 0, BLKSIZE - n);
            write(arch_fd, buf, BLKSIZE);
        }
        close(fd);
    } else if (S_ISDIR(st.st_mode)) {
        DIR *d = opendir(path);
        struct dirent *de;
        while ((de = readdir(d))) {
            if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) continue;
            char sub[PATH_MAX];
            snprintf(sub, sizeof(sub), "%s/%s", path, de->d_name);
            archive_file(arch_fd, sub);
        }
        closedir(d);
    }
}

void do_read_list(int fd, int extract) {
    struct ustar_hdr h;
    char buf[BLKSIZE];
    while (read(fd, &h, BLKSIZE) == BLKSIZE && h.name[0] != '\0') {
        char full[256] = {0};
        if (h.prefix[0]) snprintf(full, 256, "%s/%s", h.prefix, h.name);
        else strncpy(full, h.name, 100);

        if (vflag) pax_print_verbose(&h, full);
        else if (!extract) printf("%s\n", full);

        long size = strtol(h.size, NULL, 8);
        if (extract) {
            mode_t mode = strtol(h.mode, NULL, 8);
            if (h.type == '5') {
                mkdir(full, mode);
            } else {
                int out = open(full, O_WRONLY|O_CREAT|O_TRUNC, mode);
                long rem = size;
                while (rem > 0) {
                    ssize_t r = read(fd, buf, BLKSIZE);
                    if (r <= 0) break;
                    write(out, buf, (rem < r ? rem : r));
                    rem -= r;
}
                close(out);
            }
        } else {
            /* Skip data blocks for listing mode */
            lseek(fd, ((size + BLKSIZE - 1) / BLKSIZE) * BLKSIZE, SEEK_CUR);
        }
    }
}

void make_intermediate_dir(const char *path) {
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

void copy_node(const char *src, const char *dest_folder) {
    struct stat st;
    if (lstat(src, &st) == -1) return;

    char target[4096];
    snprintf(target, sizeof(target), "%s/%s", dest_folder, src);

    make_intermediate_dir(target);

    if (S_ISDIR(st.st_mode)) {
        if (mkdir(target, (st.st_mode & 07777) | 0700) == -1) {
            struct stat check;
            if (stat(target, &check) == -1 || !S_ISDIR(check.st_mode)) return;
        }

        DIR *d = opendir(src);
        struct dirent *de;
        if (d) {
            while ((de = readdir(d))) {
                if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) continue;
                char sub[4096];
                snprintf(sub, sizeof(sub), "%s/%s", src, de->d_name);
                copy_node(sub, dest_folder);
            }
            closedir(d);
        }
    } else if (S_ISREG(st.st_mode)) {
        int sfd = open(src, O_RDONLY);
        // Create with exact source mode. Note: S_ISUID/S_ISGID may be cleared by kernel
        int dfd = open(target, O_WRONLY | O_CREAT | O_TRUNC, st.st_mode & 07777);
        if (sfd >= 0) {
            if (dfd >= 0) {
                char buf[32256];
                ssize_t n;
                while ((n = read(sfd, buf, sizeof(buf))) > 0) write(dfd, buf, n);
                close(dfd);
            }
            close(sfd);
        }
    }

    // Without root:
    // 1. We skip chown() because it will fail/EPERM for other users.
    // 2. We re-apply chmod to ensure bits like S_ISVTX (sticky) or SGID are set
    //    if the open()/mkdir() call stripped them due to umask.
    chmod(target, st.st_mode & 07777);

    // 3. Preserve Timestamps
    struct utimbuf times = { .actime = st.st_atime, .modtime = st.st_mtime };
    utime(target, &times);
}

void do_copy(const char *dest, int argc, char **argv, int optind) {
    struct stat st;
    if (stat(dest, &st) == -1 || !S_ISDIR(st.st_mode) || access(dest, W_OK) == -1) {
        return;
    }

    for (int i = optind; i < argc - 1; i++) {
        copy_node(argv[i], dest);
    }
}

int main(int argc, char *argv[]) {
    int c, r = 0, w = 0; 
    char *file = NULL;

    while ((c = getopt(argc, argv, "rwvf:")) != -1) {
        switch (c) {
            case 'r': r = 1; break;
            case 'w': w = 1; break;
            case 'v': vflag = 1; break;
            case 'f': file = optarg; break;
        }
    }

    if (r && w) {
        // Copy Mode: destination is the last argument
        if (optind < argc - 1) {
            do_copy(argv[argc - 1], argc, argv, optind);
        }
    } else if (w) {
        // Write Mode
        int fd = file ? open(file, O_WRONLY|O_CREAT|O_TRUNC, 0644) : STDOUT_FILENO;
        for (int i = optind; i < argc; i++) {
            archive_file(fd, argv[i]);
        }
        char trl[BLKSIZE * 2] = {0};
        write(fd, trl, sizeof(trl));
        if (file) close(fd);
    } else {
        // List or Read Mode
        int fd = file ? open(file, O_RDONLY) : STDIN_FILENO;
        do_read_list(fd, r);
        if (file) close(fd);
    }

    return 0;
}
