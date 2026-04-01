#ifndef LS_UTIL_H
#define LS_UTIL_H

#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <stdio.h>
#include <string.h>

static void mode_to_str(mode_t mode, char *buf) {
    strcpy(buf, "----------");
    if (S_ISDIR(mode)) buf[0] = 'd';
    else if (S_ISLNK(mode)) buf[0] = 'l';
    else if (S_ISCHR(mode)) buf[0] = 'c';
    else if (S_ISBLK(mode)) buf[0] = 'b';
    else if (S_ISFIFO(mode)) buf[0] = 'p';
    else if (S_ISSOCK(mode)) buf[0] = 's';
    if (mode & S_IRUSR) buf[1] = 'r';
    if (mode & S_IWUSR) buf[2] = 'w';
    if (mode & S_IXUSR) buf[3] = (mode & S_ISUID) ? 's' : 'x';
    else if (mode & S_ISUID) buf[3] = 'S';
    if (mode & S_IRGRP) buf[4] = 'r';
    if (mode & S_IWGRP) buf[5] = 'w';
    if (mode & S_IXGRP) buf[6] = (mode & S_ISGID) ? 's' : 'x';
    else if (mode & S_ISGID) buf[6] = 'S';
    if (mode & S_IROTH) buf[7] = 'r';
    if (mode & S_IWOTH) buf[8] = 'w';
    if (mode & S_IXOTH) buf[9] = (mode & S_ISVTX) ? 't' : 'x';
    else if (mode & S_ISVTX) buf[9] = 'T';
}

static void format_time(time_t t, char *buf, size_t len) {
    struct tm *tmp = localtime(&t);
    time_t now = time(NULL);
    if (difftime(now, t) > 15768000 || difftime(now, t) < 0)
        strftime(buf, len, "%b %e  %Y", tmp);
    else
        strftime(buf, len, "%b %e %H:%M", tmp);
}

#endif
