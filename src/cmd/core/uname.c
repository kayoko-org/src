#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <kayoko/dev/os.h>

/*
 * print_tag: Utility to handle space-separated output.
 * Uses const char* to safely handle macro strings from os.h.
 */
void print_tag(const char *data, int *first) {
    if (!*first) {
        putchar(' ');
    }
    fputs(data, stdout);
    *first = 0;
}

int main(int argc, char *argv[]) {
    struct utsname s;
    int c, first = 1;
    int aflag = 0, sflag = 0, nflag = 0, rflag = 0, vflag = 0, mflag = 0, oflag = 0;
    
    /* * Check if POSIXLY_WRONG is set in the environment.
     * If it is NULL, the variable does not exist.
     */
    char *posix_env = getenv("POSIXLY_WRONG");

    /* * We only include 'o' in the optstring if POSIXLY_WRONG is set.
     * Otherwise, passing -o will trigger the default error case.
     */
    const char *optstr = (posix_env != NULL) ? "asnrvmo" : "asnrvm";

    while ((c = getopt(argc, argv, optstr)) != -1) {
        switch (c) {
            case 'a': aflag = 1; break;
            case 's': sflag = 1; break;
            case 'n': nflag = 1; break;
            case 'r': rflag = 1; break;
            case 'v': vflag = 1; break;
            case 'm': mflag = 1; break;
            case 'o': 
                /* This case is only reachable if 'o' was in optstr */
                oflag = 1; 
                break;
            default:
                fprintf(stderr, "usage: %s [-%s]\n", argv[0], optstr);
                return 1;
        }
    }

    /* POSIX default: if no flags are specified, act as if -s was passed */
    if (!(aflag || sflag || nflag || rflag || vflag || mflag || oflag)) {
        sflag = 1;
    }

    if (uname(&s) < 0) {
        perror("uname");
        return 1;
    }

    /* Print fields in standard POSIX order */
    if (aflag || sflag) print_tag(s.sysname,  &first);
    if (aflag || nflag) print_tag(s.nodename, &first);
    if (aflag || rflag) print_tag(s.release,  &first);
    if (aflag || vflag) print_tag(s.version,  &first);
    if (aflag || mflag) print_tag(s.machine,  &first);
    
    /* * Output the Operating System personality.
     * Note: If -a is used, it will only show the OS personality 
     * if POSIXLY_WRONG is set.
     */
    if (posix_env != NULL) {
        if (aflag || oflag) {
            print_tag(OS_FULL_NAME, &first);
        }
    }

    putchar('\n');
    return 0;
}
