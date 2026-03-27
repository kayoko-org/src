#ifndef _XEXE_H_
#define _XEXE_H_

#include <stdint.h>

/* * XEXE Magic Number: "XEXE" 
 * Identifies the file as a Xao Executable.
 */
#define XEXE_MAGIC 0x45584558  /* 'XEXE' in Little Endian */

/*
 * XEXE File Header
 * This is the root of the binary. It must be at offset 0.
 */
struct xexe_file_header {
    uint32_t f_magic;         /* Magic number (XEXE) */
    uint32_t f_hware;         /* HWARE Pattern: Hardware/Arch identifier (e.g., 'X86_64', 'ARM64') */
    uint32_t f_timedat;       /* Time and date stamp (Internal audit trail) */
    uint32_t f_symptr;        /* File pointer to symbol table (if not stripped) */
    uint32_t f_nsyms;         /* Number of entries in symbol table */
    uint16_t f_opthdr;        /* Size of the optional header (XEXE_OPT_HEADER) */
    uint16_t f_flags;         /* F_EXEC, F_CERTIFIED, F_RELFLG, etc. */
};

/*
 * XEXE Optional Header (Standard for Executables)
 * This contains the entry point and memory layout hints.
 */
struct xexe_opt_header {
    uint16_t  opt_magic;      /* Optional header magic */
    uint16_t  vstamp;         /* Version stamp (e.g., 0x0001 for Beta) */
    uint32_t  tsize;          /* Text size in bytes */
    uint32_t  dsize;          /* Initialized data size */
    uint32_t  bsize;          /* Uninitialized data size (BSS) */
    uint64_t  entry;          /* Entry point virtual address */
    uint64_t  text_start;     /* Base address of text segment */
    uint64_t  data_start;     /* Base address of data segment */
};

/* * XEXE Section Header 
 * Defines the various "bricks" of the binary (text, data, compliance_metadata).
 */
struct xexe_section_header {
    char     s_name[8];       /* Section name (e.g., ".text", ".cert") */
    uint64_t s_paddr;         /* Physical address */
    uint64_t s_vaddr;         /* Virtual address */
    uint32_t s_size;          /* Section size */
    uint32_t s_scnptr;        /* File pointer to raw data */
    uint32_t s_relptr;        /* File pointer to relocation entries */
    uint16_t s_nreloc;        /* Number of relocation entries */
    uint32_t s_flags;         /* Section flags (Read, Write, Exec, Secure) */
};

/* XEXE Header Flags */
#define XEXE_F_EXEC         0x0001  /* File is executable */
#define XEXE_F_CERTIFIED    0x0002  /* Xao-Specific: Binary has passed compliance check */
#define XEXE_F_STRIPPED     0x0004  /* Local symbols removed */
#define XEXE_F_RELFLG       0x0008  /* Relocation info stripped */

#endif /* _XEXE_H_ */
