#ifndef KAYOKO_FILES_ARCHIVE_PMAIN_H
#define KAYOKO_FILES_ARCHIVE_PMAIN_H

#include <sys/stat.h>
#include <dirent.h>
#include "cpio.h"
#include "ustar.h"
#include "utils.h"

/* --- Global State --- */
extern int vflag;
typedef enum { FMT_USTAR, FMT_CPIO } arch_fmt_t;
extern arch_fmt_t global_fmt;

/* --- Engine Prototypes --- */
void pax_archive_file(int arch_fd, const char *path);
void pax_do_read_list(int fd, int extract);
void pax_copy_node(const char *src, const char *dest_folder);
void pax_make_intermediate_dir(const char *path);

#endif
