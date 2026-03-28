#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>

#include "kat.h"

void usage(const char *progname) {
    fprintf(stderr, "Usage: %s <load | clear | status>\n", progname);
    exit(EXIT_FAILURE);
}

void load_rules(int fd) {
    FILE *fp = fopen("/etc/kat.conf", "r");
    if (!fp) {
        perror("Error opening /etc/kat.conf");
        return;
    }

    struct kat_rule rule;
    char line[512];
    int count = 0;

    while (fgets(line, sizeof(line), fp)) {
        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;

        // Remove trailing newline
        line[strcspn(line, "\r\n")] = 0;

        memset(&rule, 0, sizeof(rule));

        /* * Robust Parsing:
         * We look for the first colon and the last colon to isolate the path.
         * This handles '1000:/bin/shell:mask' AND '1000::mask' (Full Auto).
         */
        char *first_colon = strchr(line, ':');
        char *last_colon = strrchr(line, ':');

        if (first_colon && last_colon && first_colon != last_colon) {
            // Extract UID
            *first_colon = '\0';
            rule.uid = (uid_t)atoi(line);

            // Extract Privilege Mask
            rule.privileges = strtoull(last_colon + 1, NULL, 10);

            // Extract Path (everything between the colons)
            int path_len = last_colon - (first_colon + 1);
            if (path_len > 0) {
                if (path_len > 255) path_len = 255;
                strncpy(rule.path, first_colon + 1, path_len);
                rule.path[path_len] = '\0';
            } else {
                rule.path[0] = '\0'; // Full Auto Mode
            }

            if (ioctl(fd, KATIOC_ADD_RULE, &rule) == -1) {
                fprintf(stderr, "Failed to add rule for UID %u: %s\n", 
                        rule.uid, strerror(errno));
            } else {
                count++;
            }
        }
    }

    printf("Successfully loaded %d rules into KAT.\n", count);
    fclose(fp);
}

int main(int argc, char *argv[]) {
    if (argc < 2) usage(argv[0]);

    int fd = open("/dev/kat", O_RDWR);
    if (fd < 0) {
        perror("Error opening /dev/kat (are you root? is the module loaded?)");
        return EXIT_FAILURE;
    }

    if (strcmp(argv[1], "load") == 0) {
        load_rules(fd);
    }
    else if (strcmp(argv[1], "clear") == 0) {
        if (ioctl(fd, KATIOC_CLEAR_RULES) == 0) {
            printf("KAT table cleared.\n");
        } else {
            perror("KATIOC_CLEAR_RULES failed");
        }
    }
    else if (strcmp(argv[1], "status") == 0) {
        // Note: Ensure KATIOC_GET_COUNT is defined in your kat.h
        int count = 0;
        if (ioctl(fd, KATIOC_GET_COUNT, &count) == 0) {
            printf("KAT is active. Current rule count: %d\n", count);
        } else {
            perror("KATIOC_GET_COUNT failed");
        }
    }
    else {
        usage(argv[0]);
    }

    close(fd);
    return EXIT_SUCCESS;
}
