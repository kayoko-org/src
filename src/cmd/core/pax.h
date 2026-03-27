#ifndef PAX_H
#define PAX_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <limits.h>
#include "ls.h"

#define BLKSIZE 512
#define MAX_PATH_POSIX 255

/* Subsystem: Modes */
typedef enum { 
    MODE_LIST,   /* Default: List contents */
    MODE_READ,   /* -r: Extract from archive */
    MODE_WRITE,  /* -w: Create archive */
    MODE_COPY    /* -rw: Pass-through copy */
} pax_mode;

/* Subsystem: Formats */
typedef enum { 
    FMT_USTAR, 
    FMT_CPIO 
} archive_fmt;

/* POSIX ustar Structure */
struct ustar_hdr {
    char name[100];     char mode[8];     char uid[8];     char gid[8];
    char size[12];     char mtime[12];    char chksum[8];   char type;
    char link[100];    char magic[6];     char version[2];  char uname[32];
    char gname[32];    char devmaj[8];    char devmin[8];   char prefix[155];
    char pad[12];
};

/* --- Global Subsystem State --- */
extern int vflag;
extern archive_fmt global_format;

/* --- Modular Function Prototypes --- */

/* Path & Header Logic */
int  pax_split_path(const char *path, struct ustar_hdr *h);
void pax_set_chksum(struct ustar_hdr *h);
void pax_format_octal(char *dest, int len, long val);

/* Archive Operations */
void archive_entry(int fd, const char *path);
void extract_archive(int fd);
void list_archive(int fd);

/* Tree Migration (Copy Mode) */
void copy_tree(const char *dest, int argc, char **argv);

#endif /* PAX_H */
