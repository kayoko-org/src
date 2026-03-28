#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <stdint.h>
#include <grp.h>
#include <pwd.h>

#include "kat.h"

/* --- Mapping table for the Human-Readable IR --- */
static struct {
    const char *name;
    uint64_t mask;
} priv_map[] = {
    {"SYS_TIME",     KAT_PRIV_SYS_TIME},
    {"SYS_MODULE",   KAT_PRIV_SYS_MODULE},
    {"SYS_REBOOT",   KAT_PRIV_SYS_REBOOT},
    {"SYS_CHROOT",   KAT_PRIV_SYS_CHROOT},
    {"SYS_SYSCTL",   KAT_PRIV_SYS_SYSCTL},
    {"PROC_EXEC",    KAT_PRIV_PROC_EXEC},
    {"PROC_DEBUG",   KAT_PRIV_PROC_DEBUG},
    {"PROC_SETID",   KAT_PRIV_PROC_SETID},
    {"PROC_NICE",    KAT_PRIV_PROC_NICE},
    {"NET_BIND",     KAT_PRIV_NET_BIND},
    {"NET_FIREWALL", KAT_PRIV_NET_FIREWALL},
    {"NET_RAW",      KAT_PRIV_NET_RAW},
    {"VFS_WRITE",    KAT_PRIV_VFS_WRITE},
    {"VFS_READ",     KAT_PRIV_VFS_READ},
    {"VFS_RETAIN",   KAT_PRIV_VFS_RETAIN},
    {"DEV_RAWIO",    KAT_PRIV_DEV_RAWIO},
    {NULL, 0}
};

void usage(const char *progname) {
    fprintf(stderr, "Kayako Kernel Access Table (KAT) Admin\n");
    fprintf(stderr, "Usage: %s <load | clear | status | list>\n", progname);
    exit(EXIT_FAILURE);
}

/* Parse "VFS_WRITE|VFS_ADMIN" style strings into a 64-bit mask */
uint64_t parse_privileges(char *priv_str) {
    uint64_t final_mask = 0;
    char *saveptr;
    char *token = strtok_r(priv_str, "|", &saveptr);

    while (token != NULL) {
        int found = 0;
        for (int i = 0; priv_map[i].name != NULL; i++) {
            if (strcmp(token, priv_map[i].name) == 0) {
                final_mask |= priv_map[i].mask;
                found = 1;
                break;
            }
        }
        if (!found) {
            /* Fallback: accept hex (0x) or decimal if name not found */
            final_mask |= strtoull(token, NULL, 0);
        }
        token = strtok_r(NULL, "|", &saveptr);
    }
    return final_mask;
}

/* Low-level: push a specific UID/Priv/Path rule to the character device */
void commit_rule(int fd, uid_t uid, uint64_t privs, const char *path, int type) {
    struct kat_rule rule;
    memset(&rule, 0, sizeof(rule));

    rule.uid = uid;
    rule.privileges = privs;
    rule.type = type;   /* KAT_TYPE_ALLOW or KAT_TYPE_RESTRICT */
    rule.active = 1;

    if (path && strlen(path) > 0) {
        strncpy(rule.path, path, KAT_MAX_PATH - 1);
    } else {
        rule.path[0] = '\0'; /* Global Mode */
    }

    if (ioctl(fd, KATIOC_ADD_RULE, &rule) == -1) {
        fprintf(stderr, "  [!] Kernel rejected UID %u: %s\n", uid, strerror(errno));
    }
}

void load_rules(int fd) {
    FILE *fp = fopen("/etc/kat.conf", "r");
    if (!fp) {
        perror("Could not open /etc/kat.conf");
        return;
    }

    char line[1024];
    int count = 0;

    if (ioctl(fd, KATIOC_CLEAR_RULES) == -1) {
        perror("Failed to clear existing KAT rules");
        fclose(fp);
        return;
    }

    while (fgets(line, sizeof(line), fp)) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
        line[strcspn(line, "\r\n")] = 0;

        char *first_colon = strchr(line, ':');
        char *last_colon = strrchr(line, ':');

        if (first_colon && last_colon && first_colon != last_colon) {
            *first_colon = '\0';
            *last_colon = '\0';

            char *identity = line;
            char *path = first_colon + 1;
            char *priv_str = last_colon + 1;

            /* --- Logic to handle the '!' Restriction prefix --- */
            int type = KAT_TYPE_ALLOW;
            if (priv_str[0] == '!') {
                type = KAT_TYPE_RESTRICT;
                priv_str++; /* Move pointer forward to skip the '!' */
            }

            uint64_t privs = parse_privileges(priv_str);

            if (identity[0] == '%') {
                struct group *gr = getgrnam(identity + 1);
                if (!gr) {
                    fprintf(stderr, "Warning: Group '%s' not found.\n", identity + 1);
                    continue;
                }
                for (int i = 0; gr->gr_mem[i] != NULL; i++) {
                    struct passwd *pw = getpwnam(gr->gr_mem[i]);
                    if (pw) {
                        commit_rule(fd, pw->pw_uid, privs, path, type);
                        count++;
                    }
                }
            } else {
                struct passwd *pw = getpwnam(identity);
                uid_t uid = pw ? pw->pw_uid : (uid_t)atoi(identity);
                commit_rule(fd, uid, privs, path, type);
                count++;
            }
        }
    }
    printf("Successfully committed %d rules (ALLOW/RESTRICT) to KAT.\n", count);
    fclose(fp);
}

void list_rules(int fd) {
    int total = 0;
    if (ioctl(fd, KATIOC_GET_COUNT, &total) == -1) {
        perror("KATIOC_GET_COUNT");
        return;
    }

    printf("%-4s %-8s %-18s %s\n", "IDX", "UID", "PRIVILEGES", "TARGET_PATH");
    printf("------------------------------------------------------------\n");
    for (int i = 0; i < total; i++) {
        struct kat_rule rule;
        /* Note: Driver must use 'active' or another field to fetch by index */
        if (ioctl(fd, KATIOC_GET_RULE, &rule) != -1) {
            printf("%-4d %-8u 0x%016llX %s\n", 
                   i, rule.uid, (unsigned long long)rule.privileges, 
                   rule.path[0] ? rule.path : "GLOBAL");
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) usage(argv[0]);

    int fd = open("/dev/" KAT_DEV_NAME, O_RDWR);
    if (fd < 0) {
        perror("Error opening /dev/kat (Check module status)");
        return EXIT_FAILURE;
    }

    if (strcmp(argv[1], "load") == 0) {
        load_rules(fd);
    } else if (strcmp(argv[1], "clear") == 0) {
        if (ioctl(fd, KATIOC_CLEAR_RULES) == 0) printf("KAT table cleared.\n");
    } else if (strcmp(argv[1], "status") == 0) {
        int count = 0;
        if (ioctl(fd, KATIOC_GET_COUNT, &count) == 0) 
            printf("KAT Status: %d active rules.\n", count);
    } else if (strcmp(argv[1], "list") == 0) {
        list_rules(fd);
    } else {
        usage(argv[0]);
    }

    close(fd);
    return EXIT_SUCCESS;
}
