/*
 * Kayoko - Source Code
 * 
 * Copyright (c) 2026 The Kayoko Project. All Rights Reserved
 * 
 * This file is licensed under the Common Development and Distribution License (CDDL).
 * 
 * See /usr/src/COPYING for details.
 */

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
    {"sys.time",     KAT_PRIV_SYS_TIME},
    {"sys.module",   KAT_PRIV_SYS_MODULE},
    {"sys.reboot",   KAT_PRIV_SYS_REBOOT},
    {"sys.chroot",   KAT_PRIV_SYS_CHROOT},
    {"sys.sysctl",   KAT_PRIV_SYS_SYSCTL},
    {"proc.exec",    KAT_PRIV_PROC_EXEC},
    {"proc.debug",   KAT_PRIV_PROC_DEBUG},
    {"proc.setid",   KAT_PRIV_PROC_SETID},
    {"proc.nice",    KAT_PRIV_PROC_NICE},
    {"net.bind",     KAT_PRIV_NET_BIND},
    {"net.firewall", KAT_PRIV_NET_FIREWALL},
    {"net.raw",      KAT_PRIV_NET_RAW},
    {"vfs.write",    KAT_PRIV_VFS_WRITE},
    {"vfs.read",     KAT_PRIV_VFS_READ},
    {"vfs.retain",   KAT_PRIV_VFS_RETAIN},
    {"dev.rawio",    KAT_PRIV_DEV_RAWIO},
    {NULL, 0}
};

void usage(const char *progname) {
    fprintf(stderr, "Kayoko Kernel Access Table (KAT) Admin\n");
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
    FILE *fp = fopen("/etc/kat", "r");
    if (!fp) {
        perror("Could not open /etc/kat");
        return;
    }

    char line[1024];
    int count = 0;

    /* Wipe current kernel table before reloading */
    if (ioctl(fd, KATIOC_CLEAR_RULES) == -1) {
        perror("Failed to clear existing KAT rules");
        fclose(fp);
        return;
    }

    while (fgets(line, sizeof(line), fp)) {
        /* 1. Strip comments starting with '--' */
        char *comment_start = strstr(line, "--");
        if (comment_start) {
            *comment_start = '\0';
        }

        /* 2. Basic cleanup: trim trailing newline/spaces */
        line[strcspn(line, "\r\n")] = 0;

        /* 3. Extract the three columns: Identity, Path, Privilege */
        char identity[64], path[KAT_MAX_PATH], priv_raw[256];
        
        /* sscanf with %s automatically skips any amount of tabs or spaces */
        if (sscanf(line, "%63s %255s %255s", identity, path, priv_raw) != 3) {
            continue; /* Skip empty lines or malformed rows */
        }

        /* 4. Handle the '!' Restriction prefix */
        int type = KAT_TYPE_ALLOW;
        char *priv_ptr = priv_raw;
        if (priv_raw[0] == '!') {
            type = KAT_TYPE_RESTRICT;
            priv_ptr++; /* Move past the '!' */
        }

        uint64_t privs = parse_privileges(priv_ptr);

        /* 5. Logic for '*' Path (Global Mode) */
        const char *final_path = (strcmp(path, "*") == 0) ? "" : path;

        /* 6. Expand Groups (%) or Process UIDs */
        if (identity[0] == '%') {
            struct group *gr = getgrnam(identity + 1);
            if (!gr) {
                fprintf(stderr, "Warning: Group '%s' not found.\n", identity + 1);
                continue;
            }
            for (int i = 0; gr->gr_mem[i] != NULL; i++) {
                struct passwd *pw = getpwnam(gr->gr_mem[i]);
                if (pw) {
                    commit_rule(fd, pw->pw_uid, privs, final_path, type);
                    count++;
                }
            }
        } else {
            struct passwd *pw = getpwnam(identity);
            uid_t uid = pw ? pw->pw_uid : (uid_t)atoi(identity);
            commit_rule(fd, uid, privs, final_path, type);
            count++;
        }
    }

    printf("Successfully loaded %d rules from /etc/kat.conf\n", count);
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
