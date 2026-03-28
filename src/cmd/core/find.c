#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <fnmatch.h>
#include <limits.h>
#include <libgen.h>

// Global variable to track if any errors occur
int had_errors = 0;

void usage() {
    fprintf(stderr, "Usage: find [path] [-name <pattern>] [-type <type>] [-exec <command> {} +]\n");
    exit(2);
}

void execute_command(const char *command, const char *path) {
    char cmd[PATH_MAX + 512];

    const char *placeholder = strstr(command, "{}");
    if (placeholder) {
        char before[256], after[256];
        size_t len = placeholder - command;

        strncpy(before, command, len);
        before[len] = '\0';
        strcpy(after, placeholder + 2);

        snprintf(cmd, sizeof(cmd), "%s\"%s\"%s", before, path, after);
    } else {
        snprintf(cmd, sizeof(cmd), "%s \"%s\"", command, path);
    }

    if (system(cmd) == -1) {
        perror("system");
        had_errors = 1;
    }
}

int matches_filters(const char *name, const struct stat *st, const char *name_pattern, char type) {
    if (type) {
        char target_type = S_ISDIR(st->st_mode) ? 'd' :
                           S_ISREG(st->st_mode) ? 'f' : '\0';
        if (type != target_type) return 0;
    }

    if (name_pattern && fnmatch(name_pattern, name, 0) != 0) {
        return 0;
    }

    return 1;
}

void find(const char *base, const char *name_pattern, char type, const char *exec_command) {
    struct stat st;

    if (lstat(base, &st) == -1) {
        perror(base);
        had_errors = 1;
        return;
    }

    // Get basename for matching
    char *base_copy = strdup(base);
    char *base_name = basename(base_copy);

    // Apply filters to current path
    if (matches_filters(base_name, &st, name_pattern, type)) {
        if (exec_command)
            execute_command(exec_command, base);
        else
            printf("%s\n", base);
    }

    free(base_copy);

    // If not a directory, stop here
    if (!S_ISDIR(st.st_mode)) {
        return;
    }

    DIR *dir = opendir(base);
    if (!dir) {
        perror(base);
        had_errors = 1;
        return;
    }

    struct dirent *entry;
    char path[PATH_MAX];

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        if (snprintf(path, sizeof(path), "%s/%s", base, entry->d_name) >= sizeof(path)) {
            fprintf(stderr, "Path too long: %s/%s\n", base, entry->d_name);
            had_errors = 1;
            continue;
        }

        // Recurse into each entry
        find(path, name_pattern, type, exec_command);
    }

    closedir(dir);
}

int main(int argc, char *argv[]) {
    char *paths[argc];
    int path_count = 0;

    char *name_pattern = NULL;
    char type = 0;
    char *exec_command = NULL;

    // Parse arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-name") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "find: missing argument for -name\n");
                usage();
            }
            name_pattern = argv[++i];

        } else if (strcmp(argv[i], "-type") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "find: missing argument for -type\n");
                usage();
            }

            if (argv[i + 1][0] != 'f' && argv[i + 1][0] != 'd') {
                fprintf(stderr, "find: invalid argument for -type: %s\n", argv[i + 1]);
                usage();
            }

            type = argv[++i][0];

        } else if (strcmp(argv[i], "-exec") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "find: missing argument for -exec\n");
                usage();
            }

            exec_command = argv[++i];

        } else if (argv[i][0] == '-') {
            fprintf(stderr, "find: unknown predicate '%s'\n", argv[i]);
            usage();

        } else {
            // Treat as a path
            paths[path_count++] = argv[i];
        }
    }

	if (path_count == 0) {
	    fprintf(stderr, "find: missing path\n");
	    usage();
	}
    // Run find on all paths
    for (int i = 0; i < path_count; i++) {
        find(paths[i], name_pattern, type, exec_command);
    }

    return had_errors ? 1 : 0;
}

