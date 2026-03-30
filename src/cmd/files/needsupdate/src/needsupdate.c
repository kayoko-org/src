#define KAYOKO_FILES_MTIME_NEEDS_UPDATE_IMPLEMENTATION
#include <kayoko/files/mtime/needs/update.h>

#include <stdio.h>
#include <stdlib.h>

/**
 * needsupdate: A utility for Kayoko to determine if a target needs rebuilding.
 * Usage: needsupdate <source> <target>
 * * Exit codes:
 * 0 - Target is up to date (or newer than source)
 * 1 - Target needs update (source is newer or target missing)
 * 2 - Error (e.g., source file not found)
 */

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s source target\n", argv[0]);
        return 2;
    }

    const char *src = argv[1];
    const char *targ = argv[2];

    int result = kayoko_needs_update(src, targ);

    if (result == -1) {
        fprintf(stderr, "needsupdate: %s: No such file or directory\n", src);
        return 2;
    }

    /* Return 1 if update needed, 0 if not */
    return (result == 1) ? 1 : 0;
}
