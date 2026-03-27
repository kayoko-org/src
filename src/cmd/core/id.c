#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <sys/types.h>

void print_user(uid_t uid) {
    struct passwd *pw = getpwuid(uid);
    printf("uid=%u", (unsigned int)uid);
    if (pw) printf("(%s)", pw->pw_name);
}

void print_group(gid_t gid, const char *prefix) {
    struct group *gr = getgrgid(gid);
    printf("%s=%u", prefix, (unsigned int)gid);
    if (gr) printf("(%s)", gr->gr_name);
}

int main(int argc, char *argv[]) {
    uid_t ruid, euid;
    gid_t rgid, egid;

    /* POSIX id usually supports -u, -g, -G for scripts, 
       but the default output is the most "core" part. */
    ruid = getuid();
    euid = geteuid();
    rgid = getgid();
    egid = getegid();

    print_user(ruid);
    putchar(' ');
    print_group(rgid, "gid");

    /* If effective IDs differ (SetUID/SetGID), POSIX requires showing them */
    if (ruid != euid) {
        putchar(' ');
        print_user(euid);
    }
    if (rgid != egid) {
        putchar(' ');
        print_group(egid, "egid");
    }

    /* Supplementary groups */
    int ngroups = getgroups(0, NULL);
    if (ngroups > 0) {
        gid_t *groups = malloc(ngroups * sizeof(gid_t));
        ngroups = getgroups(ngroups, groups);
        printf(" groups=");
        for (int i = 0; i < ngroups; i++) {
            struct group *gr = getgrgid(groups[i]);
            printf("%u", (unsigned int)groups[i]);
            if (gr) printf("(%s)", gr->gr_name);
            if (i < ngroups - 1) putchar(',');
        }
        free(groups);
    }
    putchar('\n');

    return 0;
}
