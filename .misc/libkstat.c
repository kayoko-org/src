#include <sys/utsname.h>
#include <string.h>
#include <unistd.h>

int uname(struct utsname *buf) {
    // 1. Manually fetch the nodename using standard POSIX gethostname
    if (gethostname(buf->nodename, sizeof(buf->nodename)) != 0) {
        strncpy(buf->nodename, "", sizeof(buf->nodename));
    }

    // 2. Hardcode the AIX identity
    strncpy(buf->sysname, "Xai", sizeof(buf->sysname));
    strncpy(buf->release, "3", sizeof(buf->release));
    strncpy(buf->version, "7", sizeof(buf->version));
    
    // AIX machine IDs are typically 12-digit hex strings
    strncpy(buf->machine, "00F612344C00", sizeof(buf->machine));

    return 0;
}
