#ifndef PAX_UTILS
#define PAX_UTILS
#include "pax.h"

int vflag = 0;

void pax_format_octal(char *dest, int len, long val) {
    snprintf(dest, len, "%0*lo", len - 1, val);
}

int pax_split_path(const char *path, struct ustar_hdr *h) {
    size_t len = strlen(path);
    if (len <= 100) { 
        strncpy(h->name, path, 100); 
        return 0; 
    }
    if (len > MAX_PATH_POSIX) return -1;
    for (int i = len - 1; i >= 0; i--) {
        if (path[i] == '/' && i <= 155 && (len - i - 1) <= 100) {
            strncpy(h->prefix, path, i);
            strncpy(h->name, path + i + 1, 100);
            return 0;
        }
    }
    return -1;
}

void pax_set_chksum(struct ustar_hdr *h) {
    memset(h->chksum, ' ', 8);
    unsigned int sum = 0;
    unsigned char *p = (unsigned char *)h;
    for (int i = 0; i < BLKSIZE; i++) sum += p[i];
    snprintf(h->chksum, 8, "%06o", sum & 0777777);
    h->chksum[6] = '\0'; 
    h->chksum[7] = ' ';
}

void pax_print_verbose(struct ustar_hdr *h, const char *full_path) {
    char mstr[11], tstr[20];
    mode_t mode = (mode_t)strtol(h->mode, NULL, 8);
    
    /* Map ustar typeflag back to stat modes for mode_to_str */
    if (h->type == '5') mode |= S_IFDIR;
    else if (h->type == '2') mode |= S_IFLNK;
    else if (h->type == '3') mode |= S_IFCHR;
    else if (h->type == '4') mode |= S_IFBLK;
    else if (h->type == '6') mode |= S_IFIFO;
    else mode |= S_IFREG;

    time_t mtime = (time_t)strtol(h->mtime, NULL, 8);
    long size = strtol(h->size, NULL, 8);

    /* Use functions imported from ls.h */
    mode_to_str(mode, mstr);
    format_time(mtime, tstr, sizeof(tstr));

    printf("%s  1 %-8s %-8s %8ld %s %s\n", 
           mstr, 
           h->uname[0] ? h->uname : h->uid, 
           h->gname[0] ? h->gname : h->gid, 
           size, tstr, full_path);
}

#endif /* PAX_UTILS */
