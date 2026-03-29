#ifndef ED_H
#define ED_H

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>

typedef struct {
    char *data;
    int refcount;
} RefString;

typedef struct {
    RefString *rs;
    int marks[26];
    int gmark;
} Line;

typedef struct {
    Line *lines;
    size_t count, cap;
    int curr;
    int dirty;
    int show_help;
    int prompt;
    char *fname;
    char *last_re;
    char *rhs;
    char *last_err;

    /* Performant Undo */
    Line *u_lines;
    size_t u_count, u_cap;
    int u_curr;
    int can_undo;
    int restricted;
    char *last_sh;
    int warned;
} Ed;

/* Core Logic */
void ed_init(Ed *e);
void ed_free(Ed *e);
void set_err(Ed *e, char *msg);
void add_line(Ed *e, int at, const char *s);
void del_lines(Ed *e, int a, int b);
int  get_addr(Ed *e, char **s, int *addr);

#endif
