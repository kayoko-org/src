#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <grp.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

static gid_t target_gid;
static int recursive = 0;
static int status = 0;

void change_group(const char *path) {
    /* Use lchown to avoid following symlinks unless specified */
    if (lchown(path, -1, target_gid) == -1) {
        fprintf(stderr, "chgrp: %s: %s\n", path, strerror(errno));
        status = 1;
        return;
    }

    if (recursive) {
        struct stat st;
        if (lstat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
            DIR *dir = opendir(path);
            if (!dir) return;

            struct dirent *de;
            while ((de = readdir(dir)) != NULL) {
                if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
                    continue;

                char subpath[1024];
                snprintf(subpath, sizeof(subpath), "%s/%s", path, de->d_name);
                change_group(subpath);
            }
            closedir(dir);
        }
    }
}

int main(int argc, char *argv[]) {
    int opt;
    while ((opt = getopt(argc, argv, "R")) != -1) {
        if (opt == 'R') recursive = 1;
        else return 2;
    }

    if (argc - optind < 2) {
        fprintf(stderr, "usage: chgrp [-R] group file ...\n");
        return 2;
    }

    /* Resolve group name or GID */
    struct group *grp = getgrnam(argv[optind]);
    if (grp) {
        target_gid = grp->gr_gid;
    } else {
        char *endptr;
        target_gid = (gid_t)strtoul(argv[optind], &endptr, 10);
        if (*endptr != '\0') {
            fprintf(stderr, "chgrp: invalid group: %s\n", argv[optind]);
            return 2;
        }
    }

    for (int i = optind + 1; i < argc; i++) {
        change_group(argv[i]);
    }

    return status;
}
