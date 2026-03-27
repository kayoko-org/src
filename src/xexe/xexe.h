#ifndef _XEXE_H_
#define _XEXE_H_

#include <stdint.h>

#define XEXE_MAGIC    0x4558      /* 'XE' in Little Endian */
#define XEXE_SCN_SIZE 40          /* Strict COFF regularity */

/* XEXE File Header (20 Bytes) */
struct xexe_fhdr {
    uint16_t f_magic;         /* Magic number (0x4558) */
    uint16_t f_nscns;         /* Number of sections */
    uint32_t f_timdat;        /* Time/Date stamp */
    uint32_t f_symptr;        /* File offset to symbol table */
    uint32_t f_nsyms;         /* Number of symbol entries */
    uint16_t f_opthdr;        /* Size of optional header */
    uint16_t f_flags;         /* Flags (e.g., 0x0001 = Executable) */
} __attribute__((packed));

/* XEXE Optional Header (28 Bytes) - Required for Executables */
struct xexe_ohdr {
    uint16_t o_magic;         /* 0x010B (Normal) or 0x0107 (Impure) */
    uint16_t o_vstamp;        /* Version stamp */
    uint32_t o_tsize;         /* Text size */
    uint32_t o_dsize;         /* Data size */
    uint32_t o_bsize;         /* BSS size */
    uint64_t o_entry;         /* Entry point (64-bit) */
    uint64_t o_text_start;    /* Base of text (64-bit) */
} __attribute__((packed));

/* XEXE Section Header (40 Bytes) */
struct xexe_shdr {
    char     s_name[8];       /* Section name (e.g., ".text\0\0\0") */
    uint64_t s_vaddr;         /* Virtual address */
    uint64_t s_paddr;         /* Physical address */
    uint32_t s_size;          /* Section size in bytes */
    uint32_t s_scnptr;        /* File offset to raw data */
    uint32_t s_relptr;        /* File offset to relocations */
    uint32_t s_lnnoptr;       /* File offset to line numbers */
    uint16_t s_nreloc;        /* Number of relocs */
    uint16_t s_nlnno;         /* Number of line numbers */
    uint32_t s_flags;         /* Flags (0x20 = Text, 0x40 = Data, etc.) */
} __attribute__((packed));

#endif
