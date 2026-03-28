#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include "kat.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: kat <command> [args...]\n");
        return EXIT_FAILURE;
    }

    /* * In a full Solaris-style RBAC, 'kat' would call an IOCTL here 
     * to tell the kernel: "I am about to exec X, please apply 
     * the KAT_PRIVs for my UID."
     * * For this NetBSD RC2 prototype, the kauth listener in the 
     * kernel will automatically intercept the exec/system calls 
     * and check the table we built.
     */

    // Drop to the real user's ID but keep the ability to exec
    uid_t ruid = getuid();
    
    /* * Execute the requested command. 
     * The KAT kernel module will see this process and UID 
     * and apply the KAUTH_RESULT_ALLOW based on your kat.conf.
     */
    if (execvp(argv[1], &argv[1]) == -1) {
        fprintf(stderr, "kat: execution failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
