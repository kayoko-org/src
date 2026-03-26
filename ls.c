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

typedef struct {
 char *name;
 struct stat st;
} file_info;

int compare_entries(const void *a, const void *b) {
	    const file_info *fa = (file_info *)a;
	        const file_info *fb = (file_info *)b;

	    if (flag_t) {
	/* Sort by modification time (newest first) */
	if (fb->st.st_mtime != fa->st.st_mtime) 
	return (fb->st.st_mtime > fa->st.st_mtime) - (fb->st.st_mtime < fa->st.st_mtime);
        }
    /* Default: Alphabetical sort */
    return strcmp(fa->name, fb->name);
}

void format_time(time_t mtime, char *buf, size_t buflen) {
    struct tm *t = localtime(&mtime);
    time_t now = time(NULL);
    /* POSIX rule: if older than 6 months or in the future, show year */
    if (abs((long)(now - mtime)) > 15768000) {
        strftime(buf, buflen, "%b %e  %Y", t);
    } else {
        strftime(buf, buflen, "%b %e %H:%M", t);
    }
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


void list_dir(const char *path, int print_header) {
    DIR *dir = opendir(path);
    if (!dir) {
        report_error(path);
        return;
    }

    if (print_header) printf("%s:\n", path);

    /* 1. Collection Phase */
    file_info *entries = NULL;
    size_t count = 0, capacity = 16;
    entries = malloc(capacity * sizeof(file_info));

    struct dirent *entry;
    long total_blocks = 0;

    while ((entry = readdir(dir)) != NULL) {
        /* POSIX: Skip entries starting with dot unless -a is set */
        if (!flag_a && entry->d_name[0] == '.') continue;

        if (count >= capacity) {
            capacity *= 2;
            entries = realloc(entries, capacity * sizeof(file_info));
        }

        char fpath[4096];
        snprintf(fpath, sizeof(fpath), "%s/%s", path, entry->d_name);
        
        if (lstat(fpath, &entries[count].st) == 0) {
            entries[count].name = strdup(entry->d_name);
            total_blocks += entries[count].st.st_blocks;
            count++;
        }
    }

    /* 2. Sorting Phase (Handles alphabetical and -t) */
    qsort(entries, count, sizeof(file_info), compare_entries);

    /* 3. Printing Phase */
    if (flag_l && count > 0) printf("total %ld\n", total_blocks);

    for (size_t i = 0; i < count; i++) {
        if (flag_i) printf("%ju ", (uintmax_t)entries[i].st.st_ino);

        if (flag_l) {
            char mode_s[11], time_s[20];
            struct passwd *pw = getpwuid(entries[i].st.st_uid);
            struct group  *gr = getgrgid(entries[i].st.st_gid);

            mode_to_str(entries[i].st.st_mode, mode_s);
            format_time(entries[i].st.st_mtime, time_s, sizeof(time_s));

            printf("%s %3ju %-8s %-8s %8ju %s %s",
                mode_s, (uintmax_t)entries[i].st.st_nlink,
                pw ? pw->pw_name : "???", gr ? gr->gr_name : "???",
                (uintmax_t)entries[i].st.st_size, time_s, entries[i].name);

            /* POSIX: Resolve symbolic links in long format */
            if (S_ISLNK(entries[i].st.st_mode)) {
                char link_target[4096], link_path[4096];
                snprintf(link_path, sizeof(link_path), "%s/%s", path, entries[i].name);
                ssize_t len = readlink(link_path, link_target, sizeof(link_target) - 1);
                if (len != -1) {
                    link_target[len] = '\0';
                    printf(" -> %s", link_target);
                }
            }
        } else {
            printf("%s", entries[i].name);
        }

        if (flag_F) {
            mode_t m = entries[i].st.st_mode;
            if (S_ISDIR(m)) putchar('/');
            else if (S_ISLNK(m)) putchar('@');
            else if (S_ISFIFO(m)) putchar('|');
            else if (S_ISSOCK(m)) putchar('=');
            else if (m & S_IXUSR) putchar('*');
        }

        /* Determine spacing: Newline for lists/pipes, Tabs for terminal grid */
        if (flag_l || flag_1 || !isatty(STDOUT_FILENO)) {
            putchar('\n');
        } else {
            putchar('\t');
        }
    }
    if (!flag_l && !flag_1 && isatty(STDOUT_FILENO) && count > 0) putchar('\n');

    /* 4. Recursion Phase (-R) */
    if (flag_R) {
        for (size_t i = 0; i < count; i++) {
            if (S_ISDIR(entries[i].st.st_mode) && 
                strcmp(entries[i].name, ".") != 0 && strcmp(entries[i].name, "..") != 0) {
                char next_path[4096];
                snprintf(next_path, sizeof(next_path), "%s/%s", path, entries[i].name);
                printf("\n");
                list_dir(next_path, 1);
            }
        }
    }

    /* Cleanup */
    for (size_t i = 0; i < count; i++) free(entries[i].name);
    free(entries);
    closedir(dir);
}

static int global_exit_status = 0;
int main(int argc, char *argv[]) {
    int opt;
    while ((opt = getopt(argc, argv, "alRFti1")) != -1) {
        switch (opt) {
            case 'a': flag_a = 1; break;
            case 'l': flag_l = 1; break;
            case 'R': flag_R = 1; break;
            case 'F': flag_F = 1; break;
            case 't': flag_t = 1; break;
            case 'i': flag_i = 1; break;
            case '1': flag_1 = 1; break;
            default:  return 1;
        }
    }

    int num_args = argc - optind;
    
    /* Default to current directory if no args provided */
    if (num_args == 0) {
        list_dir(".", 0);
    } else {
        /* Technically, POSIX expects non-directory arguments to be 
           listed first. To keep it simple but functional:
        */
        for (int i = optind; i < argc; i++) {
            struct stat st;
            /* Check if the path exists */
            if (lstat(argv[i], &st) != 0) {
                report_error(argv[i]);
                global_exit_status = 1;
                continue;
            }

            /* If it's a directory, we list its contents */
            if (S_ISDIR(st.st_mode)) {
                /* Print header if we have multiple arguments */
                int show_header = (num_args > 1);
                
                /* If this isn't the first item being printed, add a newline */
                if (i > optind) printf("\n");
                
                list_dir(argv[i], show_header);
            } 
            /* If it's a file, just print the file info directly */
            else {
                /* For a truly perfect ls, you'd collect all file-args 
                   and print them together before starting on directories.
                */
                printf("%s\n", argv[i]); 
            }
        }
    }

    return global_exit_status;
}

