#ifndef KAYOKO_FILES_MTIME_NEEDS_UPDATE_H
#define KAYOKO_FILES_MTIME_NEEDS_UPDATE_H

#include <sys/stat.h>

/* Returns 1 if src is newer than targ, 0 otherwise */
int kayoko_needs_update(const char *src, const char *targ);

#ifdef KAYOKO_FILES_MTIME_NEEDS_UPDATE_IMPLEMENTATION

int kayoko_needs_update(const char *src, const char *targ) {
    struct stat s, t;

    /* Source must exist to compare */
    if (stat(src, &s) != 0) return -1;

    /* If target is missing, it definitely needs an update */
    if (stat(targ, &t) != 0) return 1;

    /* Simple, portable comparison of modification seconds */
    return (s.st_mtime > t.st_mtime);
}

#endif /* KAYOKO_FILES_MTIME_NEEDS_UPDATE_IMPLEMENTATION */
#endif /* KAYOKO_FILES_MTIME_NEEDS_UPDATE_H */
