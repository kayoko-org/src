#ifndef _XEXE_H_
#define _XEXE_H_

#include <stdint.h>

#define XEXE_MAGIC 0x4558 /* 'XE' */

/* File Header: Always 24 bytes */
struct xexe_fhdr {
    uint16_t f_magic;         /* Magic number (0x4558) */
    uint16_t f_nscns;         /* Number of sections */
    uint32_t f_timdat;        /* Unix timestamp */
    uint64_t f_symptr;        /* 64-bit offset to Symbol Table */
    uint32_t f_nsyms;         /* Number of symbol entries */
    uint16_t f_opthdr;        /* Size of Optional Header */
    uint16_t f_flags;         /* F_EXEC (0x1), F_RELFLG (0x2), etc. */
} __attribute__((packed));

/* Optional Header: Always 48 bytes (Required for Executables) */
struct xexe_ohdr {
    uint16_t o_magic;         /* 0x010B */
    uint16_t o_vstamp;        /* Version */
    uint64_t o_tsize;         /* Text size */
    uint64_t o_dsize;         /* Data size */
    uint64_t o_bsize;         /* BSS size */
    uint64_t o_entry;         /* Entry point (Virtual Address) */
    uint64_t o_text_start;    /* Base of text (Virtual Address) */
} __attribute__((packed));

/* Section Header: Always 48 bytes */
struct xexe_shdr {
    char     s_name[8];       /* Name (e.g. ".text\0\0\0") */
    uint64_t s_vaddr;         /* Virtual address */
    uint64_t s_paddr;         /* Physical address (usually == vaddr) */
    uint64_t s_size;          /* Size of section */
    uint64_t s_scnptr;        /* File offset to raw data */
    uint64_t s_relptr;        /* File offset to relocations */
    uint16_t s_nreloc;        /* Number of relocs */
    uint16_t s_pad;           /* Alignment padding */
    uint32_t s_flags;         /* Flags (0x20=Text, 0x40=Data) */
} __attribute__((packed));

struct xexe_reloc {
    uint64_t r_vaddr;     /* Address of reference */
    uint32_t r_symndx;    /* Index into symbol table */
    uint16_t r_type;      /* Relocation type (Arch-specific) */
    uint16_t r_pad;       /* Keep it 16-byte aligned */
} __attribute__((packed));

#endif

