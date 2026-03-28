#ifndef ED_H
#define ED_H

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <regex.h>

typedef struct {
    char *text;
    int marks[26]; /* k command: marks a-z */
    int gmark;     /* Global command flag */
} Line;

typedef struct {
    Line *lines;
    size_t count, cap;
    int curr;       /* Dot (.) */
    int dirty;
    int show_help;
    int prompt;
    char *fname;
    char *last_re;  /* Global Search Register */
    char *last_err;
    char *rhs;      /* Last substitution RHS */
} Ed;

/* Core Logic */
void ed_init(Ed *e);
void ed_free(Ed *e);
void set_err(Ed *e, char *msg);
void add_line(Ed *e, int at, const char *s);
void del_lines(Ed *e, int a, int b);
int  get_addr(Ed *e, char **s, int *addr);

#endif
