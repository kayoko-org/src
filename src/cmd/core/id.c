#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <sys/types.h>
#include <string.h>

/* Helper to print name/ID pairs: tag=%u(%s) */
/* Per POSIX: if name lookup fails, the (%s) part is omitted */
void print_full_id(const char *tag, uid_t id, int is_group, int first) {
    if (!first) putchar(' ');
    printf("%s=%u", tag, (unsigned int)id);
    
    if (is_group) {
        struct group *gr = getgrgid(id);
        if (gr) printf("(%s)", gr->gr_name);
    } else {
        struct passwd *pw = getpwuid(id);
        if (pw) printf("(%s)", pw->pw_name);
    }
}

int main(int argc, char *argv[]) {
    int opt;
    int uflag = 0, gflag = 0, Gflag = 0, nflag = 0, rflag = 0;
    
    while ((opt = getopt(argc, argv, "ugGnr")) != -1) {
        switch (opt) {
            case 'u': uflag = 1; break;
            case 'g': gflag = 1; break;
            case 'G': Gflag = 1; break;
            case 'n': nflag = 1; break;
            case 'r': rflag = 1; break;
            default:
                fprintf(stderr, "usage: id [user]\n       id -G [-n] [user]\n       id -g [-nr] [user]\n       id -u [-nr] [user]\n");
                return 1;
        }
    }

    uid_t ruid, euid;
    gid_t rgid, egid;
    gid_t *groups;
    int ngroups;

    if (optind < argc) {
        /* Lookup specific user provided as operand */
        struct passwd *pw = getpwnam(argv[optind]);
        if (!pw) {
            fprintf(stderr, "id: %s: no such user\n", argv[optind]);
            return 1;
        }
        ruid = euid = pw->pw_uid;
        rgid = egid = pw->pw_gid;
        
        /* Fetch groups for selected user */
        ngroups = 100; 
        groups = malloc(ngroups * sizeof(gid_t));
        if (getgrouplist(argv[optind], rgid, groups, &ngroups) == -1) {
            groups = realloc(groups, ngroups * sizeof(gid_t));
            getgrouplist(argv[optind], rgid, groups, &ngroups);
        }
    } else {
        /* Use invoking process IDs */
        ruid = getuid();
        euid = geteuid();
        rgid = getgid();
        egid = getegid();
        ngroups = getgroups(0, NULL);
        groups = malloc(ngroups * sizeof(gid_t));
        ngroups = getgroups(ngroups, groups);
    }

    if (uflag) {
        uid_t target = rflag ? ruid : euid;
        if (nflag) {
            struct passwd *pw = getpwuid(target);
            if (pw) printf("%s\n", pw->pw_name);
            else printf("%u\n", (unsigned int)target);
        } else printf("%u\n", (unsigned int)target);
    } 
    else if (gflag) {
        gid_t target = rflag ? rgid : egid;
        if (nflag) {
            struct group *gr = getgrgid(target);
            if (gr) printf("%s\n", gr->gr_name);
            else printf("%u\n", (unsigned int)target);
        } else printf("%u\n", (unsigned int)target);
    } 
    else if (Gflag) {
        /* -G uses format "%u" with space separation */
        for (int i = 0; i < ngroups; i++) {
            if (nflag) {
                struct group *gr = getgrgid(groups[i]);
                printf("%s", gr ? gr->gr_name : "");
            } else {
                printf("%u", (unsigned int)groups[i]);
            }
            printf("%s", (i == ngroups - 1) ? "" : " ");
        }
        printf("\n");
    } 
    else {
        /* Default Case: uid=... gid=... groups=... */
        print_full_id("uid", ruid, 0, 1);
        print_full_id("gid", rgid, 1, 0);

        if (euid != ruid) print_full_id("euid", euid, 0, 0);
        if (egid != rgid) print_full_id("egid", egid, 1, 0);

        if (ngroups > 0) {
            printf(" groups=");
            for (int i = 0; i < ngroups; i++) {
                printf("%u", (unsigned int)groups[i]);
                struct group *gr = getgrgid(groups[i]);
                if (gr) printf("(%s)", gr->gr_name);
                if (i < ngroups - 1) putchar(',');
            }
        }
        printf("\n");
    }

    free(groups);
    return 0;
}
