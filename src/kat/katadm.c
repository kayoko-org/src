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

/* * A simple parser for a hypothetical /etc/kat.conf
 * Format: user_id:path:priv_mask
 * Example: 1000:/usr/sbin/settimeofday:2
 */
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
        if (line[0] == '#' || line[0] == '\n') continue;

        // Removed the stray semicolon and added 'if'
        if (sscanf(line, "%u:%255[^:]:%llu", &rule.uid, rule.path, &rule.privileges) == 3) {
            if (ioctl(fd, KATIOC_ADD_RULE, &rule) == -1) {
                fprintf(stderr, "Failed to add rule for UID %u: %s\n", rule.uid, strerror(errno));
            } else {
                count++;
            }
        }
    }
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
        }
    } 
    else if (strcmp(argv[1], "status") == 0) {
        int count = 0;
        if (ioctl(fd, KATIOC_GET_COUNT, &count) == 0) {
            printf("KAT is active. Current rule count: %d\n", count);
        }
    } 
    else {
        usage(argv[0]);
    }

    close(fd);
    return EXIT_SUCCESS;
}
