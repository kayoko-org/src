#ifndef KAYOKO_FILES_ARCHIVE_USTAR_H
#define KAYOKO_FILES_ARCHIVE_USTAR_H

#define BLKSIZE 512
#define MAX_PATH_POSIX 255

struct ustar_hdr {
    char name[100];     char mode[8];     char uid[8];     char gid[8];
    char size[12];      char mtime[12];    char chksum[8];   char type;
    char link[100];     char magic[6];     char version[2];  char uname[32];
    char gname[32];     char devmaj[8];    char devmin[8];   char prefix[155];
    char pad[12];
};

#define TVREG  '0'
#define AREG   '\0'
#define TLNK   '1'
#define TSLNK  '2'
#define TCHR   '3'
#define TBLK   '4'
#define TDIR   '5'
#define TFIFO  '6'

#endif /* KAYOKO_FILES_ARCHIVE_USTAR_H */
