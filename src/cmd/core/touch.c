#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

void usage(void) {
    fprintf(stderr, "usage: touch [-acm] [-t time] file ...\n");
    exit(1);
}

int main(int argc, char *argv[]) {
    int c;
    int aflag = 0, cflag = 0, mflag = 0;
    struct timeval times[2];
    struct timeval *timeptr = NULL;
    char *t_str = NULL;

    while ((c = getopt(argc, argv, "acmt:")) != -1) {
        switch (c) {
            case 'a': aflag = 1; break;
            case 'c': cflag = 1; break;
            case 'm': mflag = 1; break;
            case 't': t_str = optarg; break;
            default: usage();
        }
    }

    if (optind >= argc) usage();

    // If neither -a nor -m, POSIX requires both
    if (!aflag && !mflag) aflag = mflag = 1;

    // Handle -t [[CC]YY]MMDDhhmm[.SS]
    if (t_str) {
        struct tm tm;
        time_t now = time(NULL);
        localtime_r(&now, &tm); // Initialize with current year/zone
        tm.tm_sec = 0;
        tm.tm_isdst = -1;

        const char *format;
        size_t len = strlen(t_str);
        
        if (len == 8) format = "%m%d%H%M";           // MMDDhhmm
        else if (len == 10) format = "%y%m%d%H%M";   // YYMMDDhhmm
        else if (len == 11) format = "%m%d%H%M.%S";  // MMDDhhmm.SS
        else if (len == 12) format = "%Y%m%d%H%M";   // CCYYMMDDhhmm
        else if (len == 13) format = "%y%m%d%H%M.%S";// YYMMDDhhmm.SS
        else if (len == 15) format = "%Y%m%d%H%M.%S";// CCYYMMDDhhmm.SS
        else usage();

        if (strptime(t_str, format, &tm) == NULL) usage();
        
        time_t user_time = mktime(&tm);
        times[0].tv_sec = times[1].tv_sec = user_time;
        times[0].tv_usec = times[1].tv_usec = 0;
        timeptr = times;
    }

    for (; optind < argc; optind++) {
        const char *path = argv[optind];
        struct stat st;

        if (stat(path, &st) == -1) {
            if (cflag) continue;
            // Create file: POSIX 0666 modified by umask
            int fd = open(path, O_WRONLY | O_CREAT | O_NONBLOCK | O_NOCTTY, 0666);
            if (fd == -1) {
                perror(path);
                continue;
            }
            close(fd);
            if (stat(path, &st) == -1) continue;
        }

        // Prepare times for utimes()
        struct timeval new_times[2];
        new_times[0].tv_sec = aflag ? (t_str ? times[0].tv_sec : time(NULL)) : st.st_atime;
        new_times[1].tv_sec = mflag ? (t_str ? times[1].tv_sec : time(NULL)) : st.st_mtime;
        new_times[0].tv_usec = new_times[1].tv_usec = 0;

        if (utimes(path, new_times) == -1) perror(path);
    }

    return 0;
}
