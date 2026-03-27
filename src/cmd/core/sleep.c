#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s seconds\n", argv[0]);
        return 1;
    }

    /* POSIX: seconds must be a non-negative integer */
    unsigned int seconds = (unsigned int)strtoul(argv[1], NULL, 10);
    
    /* sleep() returns the number of unslept seconds if interrupted */
    while ((seconds = sleep(seconds)) > 0);

    return 0;
}
