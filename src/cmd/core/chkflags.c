#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

const char* format_flags(unsigned long flags) {
    static char fstr[64];
    int pos = 0;

    if (flags == 0) return "-";

    // System flags (only root can change)
    if (flags & SF_IMMUTABLE) pos += sprintf(fstr + pos, "schg,");
    if (flags & SF_APPEND)    pos += sprintf(fstr + pos, "sappnd,");
    #ifdef SF_NOUNLINK
       if (flags & SF_NOUNLINK)  pos += sprintf(fstr + pos, "sunlnk,");
    #endif

    #ifdef SF_ARCHIVED
      if (flags & SF_ARCHIVED)  pos += sprintf(fstr + pos, "arch,");
    #endif
    // User flags
    if (flags & UF_IMMUTABLE) pos += sprintf(fstr + pos, "uchg,");
    if (flags & UF_APPEND)    pos += sprintf(fstr + pos, "uappnd,");
    if (flags & UF_NODUMP)    pos += sprintf(fstr + pos, "nodump,");
    if (flags & UF_OPAQUE)    pos += sprintf(fstr + pos, "opaque,");

    // Clean up trailing comma
    if (pos > 0 && fstr[pos-1] == ',') fstr[pos-1] = '\0';

    return fstr;
}


int main(int argc, char *argv[]) {
    struct stat st;
    int opt, v_flag = 0;


    while ((opt = getopt(argc, argv, "v")) != -1) {
        if (opt == 'v') v_flag = 1;
        else {
            fprintf(stderr, "usage: %s [-v] [file ...]\n", argv[0]);
            return 1;
        }
    }

    /* Check if no files were provided */
    if (optind >= argc) {
        fprintf(stderr, "usage: %s [-v] [file ...]\n", argv[0]);
        exit(2);
    }

    for (int i = optind; i < argc; i++) {
        if (lstat(argv[i], &st) != 0) {
            fprintf(stderr, "chkflags: %s: %s\n", argv[i], strerror(errno));
            continue;
        }

        if (v_flag) {
            printf("%s:\t0x%08lx\t%s\n", argv[i], (unsigned long)st.st_flags, format_flags(st.st_flags));
        } else {
            printf("%-15s %s\n", argv[i], format_flags(st.st_flags));
        }
    }

    return 0;
}
