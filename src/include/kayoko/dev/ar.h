/*
 * Kayoko - Source Code
 * 
 * Copyright (c) 2026 The Kayoko Project. All Rights Reserved
 * 
 * This file is licensed under the Common Development and Distribution License (CDDL).
 * 
 * See /usr/src/COPYING for details.
 */

#ifndef KAYOKO_DEV_AR_H
#define KAYOKO_DEV_AR_H

#include <stdio.h>
#include <stdint.h>
#include <sys/stat.h>
#include <time.h>
#include <stdarg.h>

#define ARMAG  "!<arch>\n"
#define SARMAG 8
#define ARFMAG "`\n"

/* * POSIX Fixed-width Header Structure 
 * All fields are ASCII, left-justified, and space-padded.
 * There are NO null terminators between these fields.
 */
struct kayoko_ar_hdr {
    char ar_name[16];  /* File member name */
    char ar_date[12];  /* File member date */
    char ar_uid[6];    /* User ID */
    char ar_gid[6];    /* Group ID */
    char ar_mode[8];   /* File mode (octal) */
    char ar_size[10];  /* File size */
    char ar_fmag[2];   /* Termination characters (`\n) */
};

/* Function Prototypes */
int kayoko_ar_write_hdr(FILE *archive, const char *name, struct stat *st);
int kayoko_ar_is_valid(FILE *archive);

#ifdef KAYOKO_DEV_AR_IMPLEMENTATION

#include <string.h>

/**
 * Validates the global "!<arch>\n" header 
 */
int kayoko_ar_is_valid(FILE *archive) {
    char magic[SARMAG];
    if (fread(magic, 1, SARMAG, archive) != SARMAG) return 0;
    return memcmp(magic, ARMAG, SARMAG) == 0;
}

/**
 * Helper: Safely copies formatted data into a fixed-width field.
 * This avoids the 'format-truncation' error by using a local buffer
 * and then performing a raw memcpy of only the required bytes.
 */
static void ar_format_field(char *dest, size_t size, const char *fmt, ...) {
    char buf[64]; 
    va_list ap;
    va_start(ap, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    if (len < 0) return;
    
    /* Copy the formatted string (up to field size) */
    size_t copy_len = (size_t)len > size ? size : (size_t)len;
    memcpy(dest, buf, copy_len);
    
    /* Pad the remainder with spaces as per POSIX spec */
    if (copy_len < size) {
        memset(dest + copy_len, ' ', size - copy_len);
    }
}

/**
 * Formats and writes a POSIX-compliant 60-byte member header.
 */
int kayoko_ar_write_hdr(FILE *archive, const char *name, struct stat *st) {
    struct kayoko_ar_hdr hdr;
    
    /* Ensure the struct is clean before filling */
    memset(&hdr, ' ', sizeof(struct kayoko_ar_hdr));

    ar_format_field(hdr.ar_name, sizeof(hdr.ar_name), "%-16s", name);
    ar_format_field(hdr.ar_date, sizeof(hdr.ar_date), "%-12ld", (long)st->st_mtime);
    ar_format_field(hdr.ar_uid,  sizeof(hdr.ar_uid),  "%-6u",   (unsigned)st->st_uid);
    ar_format_field(hdr.ar_gid,  sizeof(hdr.ar_gid),  "%-6u",   (unsigned)st->st_gid);
    ar_format_field(hdr.ar_mode, sizeof(hdr.ar_mode), "%-8o",   (unsigned)st->st_mode);
    ar_format_field(hdr.ar_size, sizeof(hdr.ar_size), "%-10lld", (long long)st->st_size);
    
    /* The mandatory ending backtick and newline */
    memcpy(hdr.ar_fmag, ARFMAG, 2);

    return fwrite(&hdr, sizeof(struct kayoko_ar_hdr), 1, archive) == 1;
}

#endif /* KAYOKO_DEV_AR_IMPLEMENTATION */
#endif /* KAYOKO_DEV_AR_H */
