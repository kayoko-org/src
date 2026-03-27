#ifndef _XEXEBE_H_
#define _XEXEBE_H_

#include <stdint.h>

/* * XEXE Big Endian Magic Number: "EXEX" 
 * This allows the loader to immediately identify a byte-swapped binary.
 */
#define XEXEBE_MAGIC 0x58455845  /* 'EXEX' - The inverse of XEXE for BE detection */

/*
 * XEXEBE File Header
 * Logic remains the same as xexe.h, but used for Big Endian architectures.
 */
struct xexebe_file_header {
    uint32_t f_magic;         /* Magic number (XEXEBE_MAGIC) */
    uint32_t f_hware;         /* HWARE Pattern (e.g., 'PPC_64', 'SPARC') */
    uint32_t f_timedat;       /* Time/Date for audit */
    uint32_t f_symptr;        /* Symbol table pointer */
    uint32_t f_nsyms;         /* Number of symbols */
    uint16_t f_opthdr;        /* Size of optional header */
    uint16_t f_flags;         /* F_EXEC, F_CERTIFIED, etc. */
};

/* * Optional Header for XEXEBE 
 */
struct xexebe_opt_header {
    uint16_t  opt_magic;      
    uint16_t  vstamp;         
    uint32_t  tsize;          
    uint32_t  dsize;          
    uint32_t  bsize;          
    uint64_t  entry;          /* 64-bit Entry Point */
    uint64_t  text_start;     
    uint64_t  data_start;     
};

/* Section Header remains structurally identical for parity */
struct xexebe_section_header {
    char     s_name[8];       
    uint64_t s_paddr;         
    uint64_t s_vaddr;         
    uint32_t s_size;          
    uint32_t s_scnptr;        
    uint32_t s_relptr;        
    uint16_t s_nreloc;        
    uint32_t s_flags;         
};

/* Inheritance of flags from the main spec */
#define XEXEBE_F_EXEC         0x0001
#define XEXEBE_F_CERTIFIED    0x0002
#define XEXEBE_F_STRIPPED     0x0004

#endif /* _XEXEBE_H_ */
