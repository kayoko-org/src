#ifndef KAYOKO_FILES_ARCHIVE_CPIO_H
#define KAYOKO_FILES_ARCHIVE_CPIO_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/**
 * CPIO New ASCII Format (SVR4)
 * All fields are 8-byte hexadecimal strings.
 * The total header size is 110 bytes + the length of the filename.
 */
struct cpio_hdr {
    char c_magic[6];     /* "070701" */
    char c_ino[8];       /* Inode number */
    char c_mode[8];      /* File mode and type */
    char c_uid[8];       /* User ID */
    char c_gid[8];       /* Group ID */
    char c_nlink[8];     /* Number of links */
    char c_mtime[8];     /* Modification time */
    char c_filesize[8];  /* Size of file */
    char c_devmajor[8];  /* Device major */
    char c_devminor[8];  /* Device minor */
    char c_rdevmajor[8]; /* Special device major */
    char c_rdevminor[8]; /* Special device minor */
    char c_namesize[8];  /* Length of filename (including \0) */
    char c_check[8];     /* Checksum (0 for "070701") */
};

#define CPIO_MAGIC "070701"
#define CPIO_TRAILER "TRAILER!!!"

/* Helper to pad to 4-byte boundary (required by SVR4 cpio) */
static inline size_t cpio_pad(size_t size) {
    return (4 - (size % 4)) % 4;
}

#ifdef KAYOKO_FILES_ARCHIVE_IMPLEMENTATION

/* Formats a value into 8-char hex string for cpio */
static inline void cpio_format_hex(char *dest, unsigned long val) {
    snprintf(dest, 9, "%08lx", val);
}

/* Parses 8-char hex string back to long */
static inline unsigned long cpio_parse_hex(const char *src) {
    char buf[9];
    memcpy(buf, src, 8);
    buf[8] = '\0';
    return strtoul(buf, NULL, 16);
}

/**
 * CPIO requires the header + filename to be padded to a 4-byte boundary,
 * and the file data following it to also be padded to a 4-byte boundary.
 */
static inline void cpio_write_padding(int fd, size_t current_size) {
    size_t p = cpio_pad(current_size);
    if (p > 0) {
        char zero[4] = {0};
        write(fd, zero, p);
    }
}

#endif /* KAYOKO_FILES_ARCHIVE_IMPLEMENTATION */

#endif /* KAYOKO_FILES_ARCHIVE_CPIO_H */
