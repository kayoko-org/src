/*
 * Kayoko - Source Code
 * 
 * Copyright (c) 2026 The Kayoko Project. All Rights Reserved
 * 
 * This file is licensed under the Common Development and Distribution License (CDDL).
 * 
 * See /usr/src/COPYING for details.
 */

#ifndef KAYOKO_FILES_ARCHIVE_UTIL_H
#define KAYOKO_FILES_ARCHIVE_UTIL_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
struct ustar_hdr;

/* Always visible prototypes */
void pax_format_octal(char *dest, int len, long val);
void pax_set_chksum(struct ustar_hdr *h);
int  pax_split_path(const char *path, struct ustar_hdr *h);
void cpio_format_hex(char *dest, unsigned long val);
void cpio_write_padding(int fd, size_t current_size);

#ifdef KAYOKO_FILES_ARCHIVE_IMPLEMENTATION

void pax_format_octal(char *dest, int len, long val) {
    snprintf(dest, len, "%0*lo", (int)len - 1, val);
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

int pax_split_path(const char *path, struct ustar_hdr *h) {
    size_t len = strlen(path);
    if (len <= 100) {
        strncpy(h->name, path, 100);
        return 0;
    }
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
#endif
#endif
