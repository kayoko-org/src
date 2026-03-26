#define _FILE_OFFSET_BITS 64
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
#include <ctype.h>

static int flag_a = 0, flag_b = 0, flag_c = 0, flag_d = 0, flag_f = 0;
static int flag_g = 0, flag_i = 0, flag_k = 0, flag_l = 0, flag_m = 0;
static int flag_n = 0, flag_o = 0, flag_p = 0, flag_q = 0, flag_r = 0;
static int flag_s = 0, flag_t = 0, flag_u = 0, flag_x = 0, flag_1 = 0;
static int flag_R = 0, flag_F = 0, flag_C = 0;
static int global_exit_status = 0;

typedef struct {
    char *name;
    char *fullpath;
    struct stat st;
    char *link_target;
    int is_arg;
} file_info;

void report_error(const char *path) {
    fprintf(stderr, "ls: %s: %s\n", path, strerror(errno));
    global_exit_status = 1;
}

static int get_terminal_width(void) {
    char *col_env = getenv("COLUMNS");
    if (col_env) {
        int width = atoi(col_env);
        if (width > 0) return width;
    }
    /* POSIX: if COLUMNS is unset, the width is implementation-defined. 
       80 is the standard historical fallback. */
    return 80;
}


void mode_to_str(mode_t mode, char *buf) {
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

int compare_entries(const void *a, const void *b) {
    if (flag_f) return 0;
    const file_info *fa = (const file_info *)a;
    const file_info *fb = (const file_info *)b;
    if (flag_t) {
        time_t ta = fa->st.st_mtime;
        time_t tb = fb->st.st_mtime;
        if (flag_c) { ta = fa->st.st_ctime; tb = fb->st.st_ctime; }
        else if (flag_u) { ta = fa->st.st_atime; tb = fb->st.st_atime; }
        if (ta != tb) return (tb > ta) - (tb < ta);
    }
    int res = strcmp(fa->name, fb->name);
    return flag_r ? -res : res;
}

void print_name_escaped(const char *name) {
    for (const char *p = name; *p; p++) {
        if (flag_q && (*p < 32 || *p == 127)) {
            putchar('?');
        } else if (flag_b && (*p < 32 || *p == 127)) {
            switch (*p) {
                case '\n': printf("\\n"); break;
                case '\r': printf("\\r"); break;
                case '\t': printf("\\t"); break;
                default: printf("\\%03o", (unsigned char)*p); break;
            }
        } else {
            putchar(*p);
        }
    }
}

void format_time(time_t t, char *buf, size_t len) {
    struct tm *tmp = localtime(&t);
    time_t now = time(NULL);
    
    /* POSIX rule: If the date is more than 6 months in the past 
       or any amount in the future, use the Year format. 
       6 months is defined as 15,768,000 seconds. */
    if (difftime(now, t) > 15768000 || difftime(now, t) < 0) {
        strftime(buf, len, "%b %e  %Y", tmp);
    } else {
        strftime(buf, len, "%b %e %H:%M", tmp);
    }
}

void print_long(file_info *f, int max_nlink, int max_size, int max_user, int max_group, int max_block) {
    if (flag_i) printf("%ju ", (uintmax_t)f->st.st_ino);
    if (flag_s) {
        uintmax_t b = (uintmax_t)f->st.st_blocks;
        if (flag_k) b = (b + 1) / 2;
        printf("%*ju ", max_block, b);
    }
    char mode[11], time_str[20];
    mode_to_str(f->st.st_mode, mode);
    time_t dt = f->st.st_mtime;
    if (flag_c) dt = f->st.st_ctime;
    else if (flag_u) dt = f->st.st_atime;
    format_time(dt, time_str, sizeof(time_str));
    printf("%s %*ju ", mode, max_nlink, (uintmax_t)f->st.st_nlink);
    if (!flag_g) {
        struct passwd *pw = getpwuid(f->st.st_uid);
        if (flag_n || !pw) printf("%-*u ", max_user, (unsigned int)f->st.st_uid);
        else printf("%-*s ", max_user, pw->pw_name);
    }
    if (!flag_o) {
        struct group *gr = getgrgid(f->st.st_gid);
        if (flag_n || !gr) printf("%-*u ", max_group, (unsigned int)f->st.st_gid);
        else printf("%-*s ", max_group, gr->gr_name);
    }
    if (S_ISCHR(f->st.st_mode) || S_ISBLK(f->st.st_mode)) {
        printf("%*u, %*u ", 4, (unsigned int)((f->st.st_rdev >> 8) & 0xff), 4, (unsigned int)(f->st.st_rdev & 0xff));
    } else {
        printf("%*ju ", max_size, (uintmax_t)f->st.st_size);
    }
    printf("%s ", time_str);
    print_name_escaped(f->name);
    if (f->link_target) printf(" -> %s", f->link_target);
    if (flag_F || flag_p) {
        if (S_ISDIR(f->st.st_mode)) putchar('/');
        else if (flag_F) {
            if (S_ISLNK(f->st.st_mode)) putchar('@');
            else if (S_ISFIFO(f->st.st_mode)) putchar('|');
            else if (S_ISSOCK(f->st.st_mode)) putchar('=');
            else if (f->st.st_mode & S_IXUSR) putchar('*');
        }
    }
    putchar('\n');
}

void list_dir(const char *path, int need_header, int first);

void process_entries(file_info *entries, size_t count, int is_dir_list) {
    if (count == 0) return;
    if (!flag_f) qsort(entries, count, sizeof(file_info), compare_entries);
    
    int max_nlink = 0, max_size = 0, max_user = 0, max_group = 0, max_block = 0, max_ino = 0;
    long total_blocks = 0;

    for (size_t i = 0; i < count; i++) {
        char buf[64];
        int n = snprintf(buf, sizeof(buf), "%ju", (uintmax_t)entries[i].st.st_nlink);
        if (n > max_nlink) max_nlink = n;
        n = snprintf(buf, sizeof(buf), "%ju", (uintmax_t)entries[i].st.st_size);
        if (n > max_size) max_size = n;
        
        uintmax_t b = (uintmax_t)entries[i].st.st_blocks;
        if (flag_k) b = (b + 1) / 2;
        n = snprintf(buf, sizeof(buf), "%ju", b);
        if (n > max_block) max_block = n;
 
        n = snprintf(buf, sizeof(buf), "%ju", (uintmax_t)entries[i].st.st_ino);
        if (n > max_ino) max_ino = n;
        
        total_blocks += b;
        
        if (!flag_g) {
            struct passwd *pw = getpwuid(entries[i].st.st_uid);
            int un = (flag_n || !pw) ? snprintf(buf, sizeof(buf), "%u", entries[i].st.st_uid) : (int)strlen(pw->pw_name);
            if (un > max_user) max_user = un;
        }
        if (!flag_o) {
            struct group *gr = getgrgid(entries[i].st.st_gid);
            int gn = (flag_n || !gr) ? snprintf(buf, sizeof(buf), "%u", entries[i].st.st_gid) : (int)strlen(gr->gr_name);
            if (gn > max_group) max_group = gn;
        }
    }

    if (flag_l && is_dir_list) printf("total %ld\n", total_blocks);

    if (flag_l) {
        for (size_t i = 0; i < count; i++) print_long(&entries[i], max_nlink, max_size, max_user, max_group, max_block);
    } else if (flag_m) {
        for (size_t i = 0; i < count; i++) {
            if (flag_i) printf("%ju ", (uintmax_t)entries[i].st.st_ino);
            if (flag_s) printf("%ju ", flag_k ? ((uintmax_t)entries[i].st.st_blocks + 1) / 2 : (uintmax_t)entries[i].st.st_blocks);
            print_name_escaped(entries[i].name);
            if (i < count - 1) printf(", ");
        }
        putchar('\n');
    } else {
        /* Multi-column logic (Standard POSIX COLUMNS) */
        char *col_env = getenv("COLUMNS");
        int term_width = (col_env) ? atoi(col_env) : 80;
        if (term_width <= 0) term_width = 80;

        int max_name = 0;
        for (size_t i = 0; i < count; i++) {
            int len = (int)strlen(entries[i].name);
            if (flag_i) len += max_ino + 1;
            if (flag_s) len += max_block + 1;
            if (flag_F || flag_p) len++;
            if (len > max_name) max_name = len;
        }
        max_name += 2; 

	int cols;
        if (flag_1) {
            cols = 1;
        } else {
            int term_width = get_terminal_width();
            cols = term_width / max_name;
            if (cols < 1) cols = 1;
        }
        
	if (cols < 1) cols = 1;
        int rows = (count + cols - 1) / cols;

        for (int r = 0; r < rows; r++) {
            for (int c = 0; c < cols; c++) {
                int idx = flag_x ? (r * cols + c) : (c * rows + r);
                if (idx < (int)count) {
                    int printed = 0;
                    if (flag_i) printed += printf("%*ju ", max_ino, (uintmax_t)entries[idx].st.st_ino);
                    if (flag_s) {
                        uintmax_t b = (uintmax_t)entries[idx].st.st_blocks;
                        if (flag_k) b = (b + 1) / 2;
                        printed += printf("%*ju ", max_block, b);
                    }
                    print_name_escaped(entries[idx].name);
                    printed += (int)strlen(entries[idx].name);
                    
                    if (flag_F || flag_p) {
                        if (S_ISDIR(entries[idx].st.st_mode)) { putchar('/'); printed++; }
                        else if (flag_F) {
                            if (S_ISLNK(entries[idx].st.st_mode)) { putchar('@'); printed++; }
                            else if (S_ISFIFO(entries[idx].st.st_mode)) { putchar('|'); printed++; }
                            else if (S_ISSOCK(entries[idx].st.st_mode)) { putchar('='); printed++; }
                            else if (entries[idx].st.st_mode & S_IXUSR) { putchar('*'); printed++; }
                        	}
                	    }
                    if (c < cols - 1 && (idx + (flag_x ? 1 : rows) < (int)count)) 
                        printf("%*s", max_name - printed, "");
                }
            }
            putchar('\n');
        }
    }
}


void list_dir(const char *path, int need_header, int first) {
    DIR *d = opendir(path);
    if (!d) { report_error(path); return; }
    if (!first) putchar('\n');
    if (need_header) printf("%s:\n", path);
    size_t count = 0, cap = 64;
    file_info *entries = malloc(cap * sizeof(file_info));
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (!flag_a && de->d_name[0] == '.') continue;
        if (count >= cap) { cap *= 2; entries = realloc(entries, cap * sizeof(file_info)); }
        entries[count].name = strdup(de->d_name);
        size_t plen = strlen(path) + strlen(de->d_name) + 2;
        entries[count].fullpath = malloc(plen);
        snprintf(entries[count].fullpath, plen, "%s/%s", path, de->d_name);
        if (lstat(entries[count].fullpath, &entries[count].st) != 0) {
            report_error(entries[count].fullpath);
            free(entries[count].name); free(entries[count].fullpath);
            continue;
        }
        entries[count].link_target = NULL;
        if (flag_l && S_ISLNK(entries[count].st.st_mode)) {
            char t[4096];
            ssize_t rl = readlink(entries[count].fullpath, t, sizeof(t) - 1);
            if (rl != -1) { t[rl] = '\0'; entries[count].link_target = strdup(t); }
        }
        count++;
    }
    closedir(d);
    process_entries(entries, count, 1);
    if (flag_R) {
        for (size_t i = 0; i < count; i++) {
            if (S_ISDIR(entries[i].st.st_mode) && strcmp(entries[i].name, ".") != 0 && strcmp(entries[i].name, "..") != 0) {
                list_dir(entries[i].fullpath, 1, 0);
            }
        }
    }
    for (size_t i = 0; i < count; i++) {
        free(entries[i].name); free(entries[i].fullpath);
        if (entries[i].link_target) free(entries[i].link_target);
    }
    free(entries);
}

int main(int argc, char *argv[]) {
    int opt;
    while ((opt = getopt(argc, argv, "abcdfgiklmnopqrstux1RFCR")) != -1) {
        switch (opt) {
            case 'a': flag_a = 1; break;
            case 'b': flag_b = 1; flag_q = 0; break;
            case 'c': flag_c = 1; flag_u = 0; break;
            case 'd': flag_d = 1; break;
            case 'f': flag_f = 1; flag_a = 1; flag_t = 0; flag_r = 0; flag_s = 0; break;
            case 'g': flag_g = 1; flag_l = 1; break;
            case 'i': flag_i = 1; break;
            case 'k': flag_k = 1; break;
            case 'l': flag_l = 1; flag_C = 0; flag_m = 0; flag_x = 0; flag_1 = 0; break;
            case 'm': flag_m = 1; flag_l = 0; flag_C = 0; flag_x = 0; flag_1 = 0; break;
            case 'n': flag_n = 1; flag_l = 1; break;
            case 'o': flag_o = 1; flag_l = 1; break;
            case 'p': flag_p = 1; break;
            case 'q': flag_q = 1; flag_b = 0; break;
            case 'r': flag_r = 1; break;
            case 's': flag_s = 1; break;
            case 't': flag_t = 1; break;
            case 'u': flag_u = 1; flag_c = 0; break;
            case 'x': flag_x = 1; flag_C = 0; flag_l = 0; flag_m = 0; flag_1 = 0; break;
            case '1': flag_1 = 1; flag_C = 0; flag_l = 0; flag_m = 0; flag_x = 0; break;
            case 'R': flag_R = 1; break;
            case 'F': flag_F = 1; flag_p = 0; break;
            case 'C': flag_C = 1; flag_l = 0; flag_m = 0; flag_x = 0; flag_1 = 0; break;
            default: fprintf(stderr, "usage: ls [-abcdfgiklmnopqrstux1RF] [file ...]\n"); return 1;
	        }
	    }
	/* * POSIX default behavior:
	 * 1. If stdout is a terminal, default is multi-column (-C).
	 * 2. If stdout is NOT a terminal, default is one-per-line (-1).
	 * 3. Any explicit flag (-l, -1, -m, -x, -C) overrides these defaults.
	 */
	if (!flag_l && !flag_1 && !flag_m && !flag_x && !flag_C) {
	    if (isatty(STDOUT_FILENO)) {
	        flag_C = 1;
	    } else {
	        flag_1 = 1;
	    }
	}
    int num_args = argc - optind;
    if (num_args == 0) {
        if (flag_d) {
            file_info dot; dot.name = "."; dot.fullpath = ".";
            lstat(".", &dot.st); dot.link_target = NULL;
            process_entries(&dot, 1, 0);
        } else list_dir(".", 0, 1);
    } else {
        file_info *files = malloc(num_args * sizeof(file_info));
        size_t f_count = 0;
        char **dirs = malloc(num_args * sizeof(char *));
        size_t d_count = 0;
        for (int i = optind; i < argc; i++) {
            struct stat st;
            if (lstat(argv[i], &st) != 0) { report_error(argv[i]); continue; }
            if (flag_d || !S_ISDIR(st.st_mode)) {
                files[f_count].name = argv[i];
                files[f_count].fullpath = argv[i];
                files[f_count].st = st;
                files[f_count].link_target = NULL;
                if (flag_l && S_ISLNK(st.st_mode)) {
                    char t[4096]; ssize_t rl = readlink(argv[i], t, 4095);
                    if (rl != -1) { t[rl] = '\0'; files[f_count].link_target = strdup(t); }
                }
                f_count++;
            } else dirs[d_count++] = argv[i];
        }
        if (f_count > 0) process_entries(files, f_count, 0);
        for (size_t i = 0; i < d_count; i++) {
            list_dir(dirs[i], (num_args > 1), (f_count == 0 && i == 0));
        }
        for (size_t i = 0; i < f_count; i++) if (files[i].link_target) free(files[i].link_target);
        free(files); free(dirs);
    }
    return global_exit_status;
}

