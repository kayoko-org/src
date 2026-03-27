#include "pax.h"
#include "pax.utils.h"

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

void do_copy(const char *dest, int argc, char **argv) {
    for (int i = 0; i < argc; i++) {
        char cmd[PATH_MAX * 2];
        snprintf(cmd, sizeof(cmd), "cp -R %s %s", argv[i], dest);
        system(cmd); // POSIX pax -rw is logically a recursive copy
    }
}

int main(int argc, char *argv[]) {
    int c, r = 0, w = 0; char *file = NULL;
    while ((c = getopt(argc, argv, "rwvf:")) != -1) {
        switch (c) {
            case 'r': r = 1; break;
            case 'w': w = 1; break;
            case 'v': vflag = 1; break;
            case 'f': file = optarg; break;
        }
    }
    int fd = (w && !r) ? STDOUT_FILENO : STDIN_FILENO;
    if (file) fd = (w && !r) ? open(file, O_WRONLY|O_CREAT|O_TRUNC, 0644) : open(file, O_RDONLY);

    if (r && w) do_copy(argv[argc-1], argc - optind - 1, &argv[optind]);
    else if (w) {
        for (int i = optind; i < argc; i++) archive_file(fd, argv[i]);
        char trl[BLKSIZE * 2] = {0}; write(fd, trl, sizeof(trl));
    } else do_read_list(fd, r);

    return 0;
}
