#include <stdio.h>
#include <unistd.h>
#include <sys/utsname.h>

void print_tag(char *data, int *first) {
    if (!*first) putchar(' ');
    fputs(data, stdout);
    *first = 0;
}

int main(int argc, char *argv[]) {
    struct utsname s;
    int c, first = 1;
    int aflag=0, sflag=0, nflag=0, rflag=0, vflag=0, mflag=0;

    while ((c = getopt(argc, argv, "asnrvm")) != -1) {
        switch (c) {
            case 'a': aflag = 1; break;
            case 's': sflag = 1; break;
            case 'n': nflag = 1; break;
            case 'r': rflag = 1; break;
            case 'v': vflag = 1; break;
            case 'm': mflag = 1; break;
            default: 
                fprintf(stderr, "usage: uname [-asnrvm]\n");
                return 1;
        }
    }

    /* POSIX: If there are extra operands (non-option arguments), it's an error */
    if (optind < argc) {
        fprintf(stderr, "uname: extra operand '%s'\n", argv[optind]);
        return 1;
    }

    /* FIX: If no flags at all were set, default to -s */
    if (!(aflag || sflag || nflag || rflag || vflag || mflag)) {
        sflag = 1;
    }

    if (uname(&s) < 0) {
        perror("uname");
        return 1;
    }

    /* POSIX order: sysname, nodename, release, version, machine */
    if (aflag || sflag) print_tag(s.sysname,  &first);
    if (aflag || nflag) print_tag(s.nodename, &first);
    if (aflag || rflag) print_tag(s.release,  &first);
    if (aflag || vflag) print_tag(s.version,  &first);
    if (aflag || mflag) print_tag(s.machine,  &first);

    putchar('\n');
    return 0;
}
