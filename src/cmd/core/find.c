/*
 * Kayoko - Source Code
 * 
 * Copyright (c) 2026 The Kayoko Project. All Rights Reserved
 * 
 * This file is licensed under the Common Development and Distribution License (CDDL).
 * 
 * See /usr/src/COPYING for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fnmatch.h>
#include <limits.h>
#include <errno.h>
#include <libgen.h>
#include <time.h>

static char *argv0;

void msg(const char *fmt, const char *s) {
    fprintf(stderr, "%s: ", argv0);
    fprintf(stderr, fmt, s);
    fprintf(stderr, "\n");
}

/* POSIX find primaries and operators */
enum type { OPERATOR, PRIMARY };
enum op   { OP_AND, OP_OR, OP_NOT, OP_LPAR, OP_RPAR };

typedef struct Node {
    int (*eval)(struct Node *self, const char *path, const struct stat *st);
    enum type type;
    enum op op;
    char **args;
    struct Node *left, *right;
} Node;

/* --- Primaries --- */

int p_print(Node *n, const char *p, const struct stat *s) {
    return printf("%s\n", p) >= 0;
}

int p_name(Node *n, const char *p, const struct stat *s) {
    char *tmp = strdup(p);
    char *b = basename(tmp);
    int r = fnmatch(n->args[0], b, 0) == 0;
    free(tmp);
    return r;
}

void usage(void) {
    fprintf(stderr, "usage: find path... [operand_expression...]\n");
    exit(2);
}

int compare_int(long val, const char *str) {
    char *end;
    long target = strtol((*str == '-' || *str == '+') ? str + 1 : str, &end, 10);
    if (*str == '+') return val > target;
    if (*str == '-') return val < target;
    return val == target;
}


int p_perm(Node *n, const char *p, const struct stat *s) {
    mode_t target = 0, f_mode = s->st_mode & 07777;
    char *m_str = n->args[0];
    if (*m_str == '-') return (f_mode & (mode_t)strtol(m_str+1, NULL, 8)) == (mode_t)strtol(m_str+1, NULL, 8);
    if (*m_str == '+') return (f_mode & (mode_t)strtol(m_str+1, NULL, 8)) != 0;
    return f_mode == (mode_t)strtol(m_str, NULL, 8);
}


int p_time(Node *n, const char *p, const struct stat *s) {
    time_t now = time(NULL);
    time_t target_time;
    /* Determine which time to check based on a hidden flag or arg index */
    if (n->type == 10) target_time = s->st_atime;
    else if (n->type == 11) target_time = s->st_ctime;
    else target_time = s->st_mtime;
    
    return compare_int((now - target_time) / 86400, n->args[0]);
}

int p_size(Node *n, const char *p, const struct stat *s) {
    char *str = n->args[0];
    long size = (str[strlen(str)-1] == 'c') ? s->st_size : (s->st_size + 511) / 512;
    return compare_int(size, str);
}

int p_type(Node *n, const char *p, const struct stat *s) {
    mode_t m = s->st_mode;
    switch(n->args[0][0]) {
        case 'b': return S_ISBLK(m);
        case 'c': return S_ISCHR(m);
        case 'd': return S_ISDIR(m);
        case 'f': return S_ISREG(m);
        case 'l': return S_ISLNK(m);
        case 'p': return S_ISFIFO(m);
        case 's': return S_ISSOCK(m);
    }
    return 0;
}

int p_exec(Node *n, const char *p, const struct stat *s) {
    pid_t pid;
    int status;
    char **argv = n->args;
    
    /* Replace {} with current path */
    for (int i = 0; argv[i]; i++) {
        if (strcmp(argv[i], "{}") == 0) argv[i] = (char *)p;
    }

    if ((pid = fork()) == 0) {
        execvp(argv[0], argv);
        _exit(1);
    }
    waitpid(pid, &status, 0);
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

/* --- Logic Evaluator --- */

int evaluate(Node *n, const char *path, const struct stat *st) {
    if (!n) return 1;
    if (n->type == PRIMARY) return n->eval(n, path, st);

    switch (n->op) {
        case OP_AND: return evaluate(n->left, path, st) && evaluate(n->right, path, st);
        case OP_OR:  return evaluate(n->left, path, st) || evaluate(n->right, path, st);
        case OP_NOT: return !evaluate(n->left, path, st);
        default: return 0;
    }
}

/* --- Tree Walker --- */

void walk(const char *path, Node *tree) {
    struct stat st;
    if (lstat(path, &st) == -1) {
        fprintf(stderr, "%s: %s: %s\n", argv0, path, strerror(errno));
        return;
    }
    evaluate(tree, path, &st);
    if (S_ISDIR(st.st_mode)) {
        DIR *d = opendir(path);
        if (!d) {
            fprintf(stderr, "%s: %s: %s\n", argv0, path, strerror(errno));
            return;
        }
        struct dirent *e;
        while ((e = readdir(d))) {
            if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
            char next[PATH_MAX];
            if (snprintf(next, sizeof(next), "%s/%s", path, e->d_name) < (int)sizeof(next))
                walk(next, tree);
        }
        closedir(d);
    }
}

/* --- Parser --- */
Node *parse(int *pos, int argc, char **argv) {
    if (*pos >= argc) return NULL;
    char *a = argv[*pos];
    if (!strcmp(a, "(")) {
        (*pos)++; Node *n = parse(pos, argc, argv);
        if (*pos < argc && !strcmp(argv[*pos], ")")) (*pos)++;
        return n;
    }
    if (!strcmp(a, "!")) {
        (*pos)++; Node *n = calloc(1, sizeof(Node));
        n->type = 3; n->left = parse(pos, argc, argv);
        return n;
    }

    Node *n = calloc(1, sizeof(Node));
    if (!strcmp(a, "-name")) { n->eval = p_name; n->args = &argv[++(*pos)]; }
    else if (!strcmp(a, "-type")) { n->eval = p_type; n->args = &argv[++(*pos)]; }
else if (!strcmp(a, "-perm")) { n->eval = p_perm; n->args = &argv[++(*pos)]; }
    else if (!strcmp(a, "-mtime")) { n->eval = p_time; n->args = &argv[++(*pos)]; n->type = 0; }
    else if (!strcmp(a, "-atime")) { n->eval = p_time; n->args = &argv[++(*pos)]; n->type = 10; }
    else if (!strcmp(a, "-ctime")) { n->eval = p_time; n->args = &argv[++(*pos)]; n->type = 11; }
    else if (!strcmp(a, "-size")) { n->eval = p_size; n->args = &argv[++(*pos)]; }
    else if (!strcmp(a, "-print")) { n->eval = p_print; }
    else if (!strcmp(a, "-exec")) {
        n->eval = p_exec; n->args = &argv[++(*pos)];
        while (*pos < argc && strcmp(argv[*pos], ";") != 0) (*pos)++;
        if (*pos >= argc) { msg("missing argument to `-exec'", ""); exit(2); }
        argv[*pos] = NULL;
    } else { msg("unknown primary %s", a); usage(); }

    if (*pos + 1 < argc && (strcmp(argv[*pos+1], ")") != 0)) {
        char *next = argv[*pos + 1];
        int next_type = !strcmp(next, "-o") ? 2 : 1;
        if (!strcmp(next, "-a") || next_type == 2) *pos += 2; else (*pos)++;
        Node *op = calloc(1, sizeof(Node));
        op->type = next_type; op->left = n; op->right = parse(pos, argc, argv);
        return op;
    }
    return n;
}

int main(int argc, char **argv) {
    argv0 = argv[0];
    if (argc < 2) usage();

    int i = 1, p_count = 0;
    char *paths[128];
    while (i < argc && argv[i][0] != '-' && strcmp(argv[i], "(") != 0 && strcmp(argv[i], "!") != 0)
        paths[p_count++] = argv[i++];
    
    if (p_count == 0) usage();

    int pos = i;
    Node *tree = (pos < argc) ? parse(&pos, argc, argv) : NULL;
    
    /* Default -print logic */
    if (!tree) { tree = calloc(1, sizeof(Node)); tree->eval = p_print; }

    for (int j = 0; j < p_count; j++) walk(paths[j], tree);
    return 0;
}

