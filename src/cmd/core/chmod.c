#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>

static int recursive = 0;
static int status = 0;

/* Helper to parse symbolic mode strings (e.g., "u+x,g-w") */
mode_t parse_symbolic(const char *s, mode_t current) {
    mode_t new_mode = current;
    mode_t who, op, perm;
    
    while (*s) {
        who = 0;
        /* Who: u, g, o, a */
        while (*s == 'u' || *s == 'g' || *s == 'o' || *s == 'a') {
            if (*s == 'u') who |= S_IRWXU;
            if (*s == 'g') who |= S_IRWXG;
            if (*s == 'o') who |= S_IRWXO;
            if (*s == 'a') who |= (S_IRWXU | S_IRWXG | S_IRWXO);
            s++;
        }
        if (who == 0) who = (S_IRWXU | S_IRWXG | S_IRWXO);

        /* Op: +, -, = */
        op = *s++;
        if (op != '+' && op != '-' && op != '=') return (mode_t)-1;

        /* Perm: r, w, x */
        perm = 0;
        while (*s == 'r' || *s == 'w' || *s == 'x') {
            if (*s == 'r') perm |= (S_IRUSR | S_IRGRP | S_IROTH);
            if (*s == 'w') perm |= (S_IWUSR | S_IWGRP | S_IWOTH);
            if (*s == 'x') perm |= (S_IXUSR | S_IXGRP | S_IXOTH);
            s++;
        }
        perm &= who;

        if (op == '+') new_mode |= perm;
        else if (op == '-') new_mode &= ~perm;
        else if (op == '=') new_mode = (new_mode & ~who) | perm;

        if (*s == ',') s++;
        else if (*s) return (mode_t)-1;
    }
    return new_mode;
}

void apply_chmod(const char *path, const char *mode_str) {
    struct stat st;
    if (lstat(path, &st) == -1) {
        fprintf(stderr, "chmod: %s: %s\n", path, strerror(errno));
        status = 1;
        return;
    }

    mode_t new_mode;
    char *endptr;
    /* Try octal first */
    new_mode = (mode_t)strtoul(mode_str, &endptr, 8);
    
    /* If not octal, parse as symbolic */
    if (*endptr != '\0') {
        new_mode = parse_symbolic(mode_str, st.st_mode);
        if (new_mode == (mode_t)-1) {
            fprintf(stderr, "chmod: invalid mode: %s\n", mode_str);
            exit(2);
        }
    }

    if (chmod(path, new_mode & 07777) == -1) {
        fprintf(stderr, "chmod: %s: %s\n", path, strerror(errno));
        status = 1;
    }

    if (recursive && S_ISDIR(st.st_mode)) {
        DIR *dir = opendir(path);
        if (!dir) return;
        struct dirent *de;
        while ((de = readdir(dir)) != NULL) {
            if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
                continue;
            char subpath[1024];
            snprintf(subpath, sizeof(subpath), "%s/%s", path, de->d_name);
            apply_chmod(subpath, mode_str);
        }
        closedir(dir);
    }
}

int main(int argc, char *argv[]) {
    int i = 1;

    /* Handle options manually if they exist */
    while (i < argc && argv[i][0] == '-') {
        /* If it's exactly "-R", set the flag */
        if (strcmp(argv[i], "-R") == 0) {
            recursive = 1;
            i++;
        } 
        /* If it's "--", stop parsing options */
        else if (strcmp(argv[i], "--") == 0) {
            i++;
            break;
        } 
        /* * If it's something like "-x", "+x", or "=r", 
         * it's a MODE, not an option. Stop parsing options.
         */
        else if (argv[i][1] == 'x' || argv[i][1] == 'r' || argv[i][1] == 'w' || 
                 argv[i][1] == 's' || argv[i][1] == 't') {
            break; 
        }
        else {
            fprintf(stderr, "%s: unknown option %s\n", argv[0], argv[i]);
            return 2;
        }
    }

    if (i >= argc) {
        fprintf(stderr, "usage: %s [-R] mode file ...\n", argv[0]);
        return 2;
    }

    char *mode_str = argv[i++]; // The mode is here

    for (; i < argc; i++) {
        apply_chmod(argv[i], mode_str);
    }

    return status;
}
