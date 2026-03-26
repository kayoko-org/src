#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

/* POSIX limits: 255 is standard, but we add 1 for the null terminator */
#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 255
#endif

int main(int argc, char *argv[]) {
    char name[HOST_NAME_MAX + 1];

    // Case: No arguments - Print the current hostname
    if (argc == 1) {
        if (gethostname(name, sizeof(name)) != 0) {
            return 1;
        }
        printf("%s\n", name);
    } 
    
    // Case: One argument - Set the system hostname
    else if (argc == 2) {
        /* sethostname requires the length of the string provided */
        if (sethostname(argv[1], strlen(argv[1])) != 0) {
            // This will typically fail if not run as root (EPERM)
            return 1;
        }
    } 
    
    // Case: Too many arguments
    else {
        fprintf(stderr, "usage: hostname [...]\n");
        return 1;
    }

    return 0;
}
