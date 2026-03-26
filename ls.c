#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>
#include <errno.h>
#include <stdint.h>

/* POSIX Standard Flags */
static int flag_a = 0, flag_l = 0, flag_R = 0, flag_t = 0, flag_F = 0, flag_1 = 0, flag_i = 0;

/* POSIX Error Formatting: utility_name: operand: message */
void report_error(const char *path) {
    fprintf(stderr, "ls: %s: %s\n", path, strerror(errno));
}

/* POSIX File Mode String (10 characters + null) */
void mode_to_str(mode_t mode, char *buf) {
    strcpy(buf, "----------");
    if (S_ISDIR(mode))      buf[0] = 'd';
    else if (S_ISLNK(mode))  buf[0] = 'l';
    else if (S_ISCHR(mode))  buf[0] = 'c';
    else if (S_ISBLK(mode))  buf[0] = 'b';
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

void list_dir(const char *path) {
    DIR *dir = opendir(path);
    if (!dir) {
        report_error(path);
        return;
    }

    struct dirent *entry;
    struct stat st;
    char fpath[1024];
    long total_blocks = 0;
    int count = 0;

    /* First pass: Calculate total blocks for -l (POSIX requirement) */
    if (flag_l) {
        while ((entry = readdir(dir)) != NULL) {
            if (!flag_a && entry->d_name[0] == '.') continue;
            snprintf(fpath, sizeof(fpath), "%s/%s", path, entry->d_name);
            if (lstat(fpath, &st) == 0) total_blocks += st.st_blocks;
        }
        rewinddir(dir);
        printf("total %ld\n", total_blocks);
    }

    /* Second pass: Print entries */
    while ((entry = readdir(dir)) != NULL) {
        if (!flag_a && entry->d_name[0] == '.') continue;

        snprintf(fpath, sizeof(fpath), "%s/%s", path, entry->d_name);
        if (lstat(fpath, &st) != 0) {
            report_error(entry->d_name);
            continue;
        }

        /* 1. Serial Number (-i) */
        if (flag_i) {
            printf("%ju ", (uintmax_t)st.st_ino);
        }

        /* 2. Format Core Data */
        if (flag_l) {
            char mode_s[11];
            struct passwd *pw = getpwuid(st.st_uid);
            struct group  *gr = getgrgid(st.st_gid);
            char time_s[20];

            mode_to_str(st.st_mode, mode_s);
            /* POSIX Date: %b %e %H:%M */
            strftime(time_s, sizeof(time_s), "%b %e %H:%M", localtime(&st.st_mtime));

            printf("%s %3ld %s %s %8ld %s %s", 
                mode_s, (long)st.st_nlink, 
                pw ? pw->pw_name : "unknown", 
                gr ? gr->gr_name : "unknown", 
                (long)st.st_size, time_s, entry->d_name);
        } else {
            printf("%s", entry->d_name);
        }

        /* 3. Classify (-F) */
        if (flag_F) {
            if (S_ISDIR(st.st_mode))      putchar('/');
            else if (S_ISLNK(st.st_mode)) putchar('@');
            else if (S_ISFIFO(st.st_mode)) putchar('|');
            else if (st.st_mode & S_IXUSR) putchar('*');
        }

        /* 4. Column/Line Separator */
        if (flag_l || flag_1 || !isatty(STDOUT_FILENO)) {
            putchar('\n');
        } else {
            putchar('\t');
        }
        count++;
    }

    if (count > 0 && !flag_l && !flag_1 && isatty(STDOUT_FILENO)) {
        putchar('\n');
    }

    closedir(dir);
}

int main(int argc, char *argv[]) {
    int opt;
    /* Only POSIX-defined flags included */
    while ((opt = getopt(argc, argv, "alRFti1")) != -1) {
        switch (opt) {
            case 'a': flag_a = 1; break;
            case 'l': flag_l = 1; break;
            case 'R': flag_R = 1; break;
            case 'F': flag_F = 1; break;
            case 't': flag_t = 1; break;
            case 'i': flag_i = 1; break;
            case '1': flag_1 = 1; break;
            default:  return 1; /* POSIX requires non-zero exit on error */
        }
    }

    const char *target = (optind < argc) ? argv[optind] : ".";
    list_dir(target);
    
    return 0;
}
