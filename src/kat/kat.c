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
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <limits.h>

#include "kat.h"

int
main(int argc, char *argv[])
{
    int fd;
    uid_t ruid = getuid(); // This is 'Lars' (UID 1000)

    if (argc < 2) {
        fprintf(stderr, "Usage: kat <command> [args...]\n");
        return EXIT_FAILURE;
    }

    /* * 1. Check-in with the Kernel Module.
     * Since 'kat' is SUID root, it has permission to open /dev/kat.
     */
    fd = open("/dev/" KAT_DEV_NAME, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "kat: kernel module not active or no permission (%s)\n", 
                strerror(errno));
        return EXIT_FAILURE;
    }
    
    /* * We don't strictly need to do anything with the FD yet, 
     * but closing it marks the end of our privileged initialization.
     */
    close(fd);

    /* * 2. DROP PRIVILEGES (The Fix).
     * We drop the Effective UID (root) and set it to the Real UID (Lars).
     * After this call, the process is no longer root.
     */
    if (setuid(ruid) == -1) {
        perror("setuid");
        return EXIT_FAILURE;
    }

    /* * 3. Execute the command.
     * Now that we are back to being UID 1000, when 'execvp' runs 'rm', 
     * the kernel will check standard permissions (Access Denied) 
     * and THEN call your KAT kauth listener to see if an exception exists.
     */
    if (execvp(argv[1], &argv[1]) == -1) {
        fprintf(stderr, "kat: execution failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
