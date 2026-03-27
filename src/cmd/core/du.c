#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ftw.h>

long total_blocks = 0;
int opt_a = 0, opt_s = 0, opt_k = 0;

int display_usage(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    /* POSIX says du counts 512-byte blocks. st_blocks is usually 512-byte. */
    long blocks = sb->st_blocks;
    
    /* If -k is specified, convert to 1024-byte blocks */
    long display_val = opt_k ? (blocks + 1) / 2 : blocks;

    if (typeflag == FTW_D || opt_a) {
        if (!opt_s) {
            printf("%ld\t%s\n", display_val, fpath);
        }
    }

    total_blocks += blocks;
    return 0;
}

int main(int argc, char *argv[]) {
    int opt;
    while ((opt = getopt(argc, argv, "ask")) != -1) {
        switch (opt) {
            case 'a': opt_a = 1; break;
            case 's': opt_s = 1; break;
            case 'k': opt_k = 1; break;
            default: exit(1);
        }
    }

    const char *path = (optind < argc) ? argv[optind] : ".";

    /* FTW_PHYS: Don't follow symlinks (standard du behavior) */
    if (nftw(path, display_usage, 20, FTW_PHYS) == -1) {
        perror("nftw");
        return 1;
    }

    if (opt_s) {
        long final_val = opt_k ? (total_blocks + 1) / 2 : total_blocks;
        printf("%ld\t%s\n", final_val, path);
    }

    return 0;
}
