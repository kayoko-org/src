#ifndef _XEXEBE_H_
#define _XEXEBE_H_

#include <stdint.h>

#define XEXEBE_MAGIC 0x5845 /* 'EX' */

struct xexebe_fhdr {
    uint16_t f_magic;         /* 0x5845 */
    uint16_t f_nscns;
    uint32_t f_timdat;
    uint64_t f_symptr;
    uint32_t f_nsyms;
    uint16_t f_opthdr;
    uint16_t f_flags;
} __attribute__((packed));

struct xexebe_ohdr {
    uint16_t o_magic;
    uint16_t o_vstamp;
    uint64_t o_tsize;
    uint64_t o_dsize;
    uint64_t o_bsize;
    uint64_t o_entry;
    uint64_t o_text_start;
} __attribute__((packed));

struct xexebe_shdr {
    char     s_name[8];
    uint64_t s_vaddr;
    uint64_t s_paddr;
    uint64_t s_size;
    uint64_t s_scnptr;
    uint64_t s_relptr;
    uint16_t s_nreloc;
    uint16_t s_pad;
    uint32_t s_flags;
} __attribute__((packed));

#endif
