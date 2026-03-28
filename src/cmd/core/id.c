#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>

extern char **environ;  // Include if you decide to use extra logic later

void print_uid(uid_t uid, int numeric) {
    if (numeric) {
        printf("%u", (unsigned int)uid);
    } else {
        struct passwd *pw = getpwuid(uid);
        if (pw) {
            printf("%s", pw->pw_name);
        } else {
            fprintf(stderr, "id: no name for user ID %u\n", (unsigned int)uid);
            exit(1);
        }
    }
}

void print_gid(gid_t gid, int numeric, const char *prefix) {
    if (numeric) {
        printf("%s%u", prefix, (unsigned int)gid);
    } else {
        struct group *gr = getgrgid(gid);
        if (gr) {
            printf("%s%s", prefix, gr->gr_name);
        } else {
            fprintf(stderr, "id: no name for group ID %u\n", (unsigned int)gid);
            exit(1);
        }
    }
}

void print_groups(int numeric) {
    int ngroups = getgroups(0, NULL);
    if (ngroups > 0) {
        gid_t *groups = malloc(ngroups * sizeof(gid_t));
        ngroups = getgroups(ngroups, groups);
        for (int i = 0; i < ngroups; i++) {
            if (i > 0) putchar(' ');
            if (numeric) {
                printf("%u", (unsigned int)groups[i]);
            } else {
                struct group *gr = getgrgid(groups[i]);
                if (gr) {
                    printf("%s", gr->gr_name);
                } else {
                    fprintf(stderr, "id: no name for group ID %u\n", (unsigned int)groups[i]);
                    free(groups);
                    exit(1);
                }
            }
        }
        free(groups);
    }
    putchar('\n');
}

int main(int argc, char *argv[]) {
    int opt;
    int show_uid = 0, show_gid = 0, show_groups = 0;
    int numeric = 1;  // Default to numeric output

    while ((opt = getopt(argc, argv, "ugGn")) != -1) {
        switch (opt) {
            case 'u':
                show_uid = 1;
                break;
            case 'g':
                show_gid = 1;
                break;
            case 'G':
                show_groups = 1;
                break;
            case 'n':
                numeric = 0;
                break;
            default:
                fprintf(stderr, "usage: id [-u] [-g] [-G] [-n]\n");
                return 1;
        }
    }

    if (show_uid) {
        uid_t uid = getuid();  // Real user ID
        print_uid(uid, numeric);
        putchar('\n');
    }

    if (show_gid) {
        gid_t gid = getgid();  // Real group ID
        print_gid(gid, numeric, "");
        putchar('\n');
    }

    if (show_groups) {
        print_groups(numeric);
    }

    // Default case: Print everything
    if (!(show_uid || show_gid || show_groups)) {
        uid_t ruid = getuid();
        gid_t rgid = getgid();
        uid_t euid = geteuid();
        gid_t egid = getegid();

        print_uid(ruid, 1); putchar(' ');
        print_gid(rgid, 1, "gid=");

        // Show effective IDs if different
        if (ruid != euid) {
            putchar(' '); print_uid(euid, 1);
        }
        if (rgid != egid) {
            putchar(' '); print_gid(egid, 1, " egid=");
        }

        printf(" groups=");
        print_groups(1);
    }

    return 0;
}
