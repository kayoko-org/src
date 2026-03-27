#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <utime.h>
#include <errno.h>
#include <dirent.h>
#include <pwd.h>
#include <grp.h>
#include "ls.h"

#define BLKSIZE 512

typedef enum { FMT_USTAR, FMT_CPIO } archive_fmt;

/* POSIX ustar Header */
struct ustar_hdr {
    char name[100];     char mode[8];       char uid[8];        char gid[8];
    char size[12];      char mtime[12];     char chksum[8];     char type;
    char link[100];     char magic[6];      char version[2];    char uname[32];
    char gname[32];     char devmaj[8];     char devmin[8];     char prefix[155];
    char pad[12];
};

/* POSIX cpio (odc) Header */
struct cpio_hdr {
    char magic[6];      char dev[6];        char ino[6];        char mode[6];
    char uid[6];        char gid[6];        char nlink[6];      char rdev[6];
    char mtime[11];     char namesize[6];   char filesize[11];
};

static int rf = 0, wf = 0, vf = 0;
static archive_fmt format = FMT_USTAR;

/* Calculate ustar checksum */
void set_chksum(struct ustar_hdr *h) {
    memset(h->chksum, ' ', 8);
    unsigned int sum = 0;
    unsigned char *p = (unsigned char *)h;
    for (int i = 0; i < BLKSIZE; i++) sum += p[i];
    sprintf(h->chksum, "%06o", sum & 0777777);
}

void print_verbose(const char *name, mode_t mode, uid_t uid, gid_t gid, long size, time_t mtime) {
    char mstr[11];
    char tstr[20];
    struct passwd *pw = getpwuid(uid);
    struct group *gr = getgrgid(gid);

    mode_to_str(mode, mstr);
    format_time(mtime, tstr, sizeof(tstr));

    printf("%s  1 %-8s %-8s %8ld %s %s\n",
           mstr,
           pw ? pw->pw_name : "root",
           gr ? gr->gr_name : "users",
           size, tstr, name);
}

/* Write a file to the archive */
void archive_file(int arch_fd, const char *path) {
    struct stat st;
    if (lstat(path, &st) == -1) {
        fprintf(stderr, "pax: %s: %s\n", path, strerror(errno));
        return;
    }

    if (vf && wf) printf("%s\n", path);

    if (format == FMT_USTAR) {
        struct ustar_hdr h;
        memset(&h, 0, sizeof(h));
        strncpy(h.name, path, 100);
        sprintf(h.mode, "%07o", st.st_mode & 07777);
        sprintf(h.uid, "%07o", st.st_uid);
        sprintf(h.gid, "%07o", st.st_gid);
        sprintf(h.size, "%011lo", S_ISREG(st.st_mode) ? (long)st.st_size : 0L);
        sprintf(h.mtime, "%011lo", (long)st.st_mtime);
        h.type = S_ISDIR(st.st_mode) ? '5' : '0';
        memcpy(h.magic, "ustar", 6);
        memcpy(h.version, "00", 2);
        set_chksum(&h);
        write(arch_fd, &h, BLKSIZE);
    } else {
        struct cpio_hdr h;
        memset(&h, 0, sizeof(h));
        memcpy(h.magic, "070707", 6);
        sprintf(h.mode, "%06o", st.st_mode & 0777777);
        sprintf(h.uid, "%06o", st.st_uid);
        sprintf(h.gid, "%06o", st.st_gid);
        sprintf(h.mtime, "%011lo", (long)st.st_mtime);
        sprintf(h.namesize, "%06o", (int)strlen(path) + 1);
        sprintf(h.filesize, "%011lo", S_ISREG(st.st_mode) ? (long)st.st_size : 0L);
        write(arch_fd, &h, sizeof(h));
        write(arch_fd, path, strlen(path) + 1);
    }

    if (S_ISREG(st.st_mode)) {
        int fd = open(path, O_RDONLY);
        char buf[BLKSIZE];
        ssize_t n;
        while ((n = read(fd, buf, BLKSIZE)) > 0) {
            if (format == FMT_USTAR) {
                if (n < BLKSIZE) memset(buf + n, 0, BLKSIZE - n);
                write(arch_fd, buf, BLKSIZE);
            } else {
                write(arch_fd, buf, n);
            }
        }
        close(fd);
    } else if (S_ISDIR(st.st_mode)) {
        DIR *d = opendir(path);
        struct dirent *de;
        if (!d) return;
        while ((de = readdir(d))) {
            if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;
            char subpath[1024];
            snprintf(subpath, sizeof(subpath), "%s/%s", path, de->d_name);
            archive_file(arch_fd, subpath);
        }
        closedir(d);
    }
}

/* Extract or List from archive */
void extract_archive(int arch_fd) {
    char buf[BLKSIZE];
    while (read(arch_fd, buf, (format == FMT_USTAR ? BLKSIZE : sizeof(struct cpio_hdr))) > 0) {
        char name[256];
        long size = 0, mtime = 0;
        int mode = 0, uid = 0, gid = 0;

        if (format == FMT_USTAR) {
            struct ustar_hdr *h = (struct ustar_hdr *)buf;
            if (h->name[0] == '\0') break;
            strncpy(name, h->name, 100);
            size = strtol(h->size, NULL, 8);
            mode = strtol(h->mode, NULL, 8);
            uid = strtol(h->uid, NULL, 8);
            gid = strtol(h->gid, NULL, 8);
            mtime = strtol(h->mtime, NULL, 8);

            if (vf) {
                print_verbose(name, mode, uid, gid, size, (time_t)mtime);
            } else if (!rf) {
                printf("%s\n", name);
            }

            if (rf) {
                if (h->type == '5') {
                    mkdir(name, mode);
                } else {
                    int out_fd = open(name, O_WRONLY|O_CREAT|O_TRUNC, mode);
                    long rem = size;
                    while (rem > 0) {
                        read(arch_fd, buf, BLKSIZE);
                        write(out_fd, buf, (rem < BLKSIZE ? rem : BLKSIZE));
                        rem -= BLKSIZE;
                    }
                    close(out_fd);
                }
            } else {
                long skip = ((size + BLKSIZE - 1) / BLKSIZE) * BLKSIZE;
                lseek(arch_fd, skip, SEEK_CUR);
            }
        } else {
            struct cpio_hdr *h = (struct cpio_hdr *)buf;
            if (strncmp(h->magic, "070707", 6) != 0) break;
            int nsize = strtol(h->namesize, NULL, 8);
            size = strtol(h->filesize, NULL, 8);
            mode = strtol(h->mode, NULL, 8);
            uid = strtol(h->uid, NULL, 8);
            gid = strtol(h->gid, NULL, 8);
            mtime = strtol(h->mtime, NULL, 8);

            read(arch_fd, name, nsize);
            if (strcmp(name, "TRAILER!!!") == 0) break;

            if (vf) {
                print_verbose(name, mode, uid, gid, size, (time_t)mtime);
            } else if (!rf) {
                printf("%s\n", name);
            }

            if (rf) {
                if (S_ISDIR(mode)) {
                    mkdir(name, mode & 0777);
                } else {
                    int out_fd = open(name, O_WRONLY|O_CREAT|O_TRUNC, mode & 0777);
                    long rem = size;
                    while (rem > 0) {
                        ssize_t r = read(arch_fd, buf, (rem > BLKSIZE ? BLKSIZE : rem));
                        write(out_fd, buf, r);
                        rem -= r;
                    }
                    close(out_fd);
                }
            } else {
                lseek(arch_fd, size, SEEK_CUR);
            }
        }
    }
}

int main(int argc, char *argv[]) {
    int c;
    char *file = NULL;

    while ((c = getopt(argc, argv, "rwvf:x:")) != -1) {
        switch (c) {
            case 'r': rf = 1; break;
            case 'w': wf = 1; break;
            case 'v': vf = 1; break;
            case 'f': file = optarg; break;
            case 'x':
                if (strcmp(optarg, "cpio") == 0) format = FMT_CPIO;
                else format = FMT_USTAR;
                break;
            default: exit(1);
        }
    }

    int fd;
    if (wf) {
        fd = file ? open(file, O_WRONLY|O_CREAT|O_TRUNC, 0644) : 1;
        for (int i = optind; i < argc; i++) archive_file(fd, argv[i]);
        if (format == FMT_USTAR) {
            char trailer[BLKSIZE * 2] = {0};
            write(fd, trailer, sizeof(trailer));
        } else {
            struct cpio_hdr h = { .magic = "070707", .namesize = "000013" };
            write(fd, &h, sizeof(h));
            write(fd, "TRAILER!!!\0", 11);
        }
    } else {
        fd = file ? open(file, O_RDONLY) : 0;
        extract_archive(fd);
    }

    if (file && fd >= 0) close(fd);
    return 0;
}
