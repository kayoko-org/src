/*
 * Kayoko - Source Code
 * 
 * Copyright (c) 2026 The Kayoko Project. All Rights Reserved
 * 
 * This file is licensed under the Common Development and Distribution License (CDDL).
 * 
 * See /usr/src/COPYING for details.
 */

#include <kayoko/textproc/ed.h>
#include <kayoko/textproc/ed/buffer.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

/* --- Core Memory & String Management --- */

void line_add_ref(Line *l) {
    if (l && l->rs) l->rs->refcount++;
}

void line_release(Line *l) {
    if (l && l->rs && --l->rs->refcount == 0) {
        free(l->rs->data);
        free(l->rs);
        l->rs = NULL;
    }
}

void update_line_text(Line *l, const char *new_text) {
    line_release(l);
    l->rs = malloc(sizeof(RefString));
    l->rs->data = strdup(new_text);
    l->rs->refcount = 1;
}

/* --- Undo System --- */

void save_undo(Ed *e) {
    for (size_t i = 0; i < e->u_count; i++) line_release(&e->u_lines[i]);
    free(e->u_lines);

    e->u_count = e->count;
    e->u_curr = e->curr;
    e->u_lines = malloc(e->u_count * sizeof(Line));

    for (size_t i = 0; i < e->count; i++) {
        e->u_lines[i] = e->lines[i];
        line_add_ref(&e->u_lines[i]);
    }
    e->can_undo = 1;
}

void do_undo(Ed *e) {
    if (!e->can_undo) { set_err(e, "nothing to undo"); return; }

    Line *tmp_l = e->lines;
    size_t tmp_cnt = e->count;
    int tmp_cur = e->curr;

    e->lines = e->u_lines;
    e->count = e->u_count;
    e->curr = e->u_curr;

    e->u_lines = tmp_l;
    e->u_count = tmp_cnt;
    e->u_curr = tmp_cur;
}

void add_line(Ed *e, int at, const char *s) {
    if (e->count >= e->cap) {
        e->cap = e->cap ? e->cap * 2 : 128;
        e->lines = realloc(e->lines, e->cap * sizeof(Line));
    }
    for (int i = (int)e->count; i > at; i--) e->lines[i] = e->lines[i-1];

    e->lines[at].rs = NULL;
    update_line_text(&e->lines[at], s);
    e->lines[at].gmark = 0;
    for (int j = 0; j < 26; j++) e->lines[at].marks[j] = -1;

    e->count++; e->curr = at + 1; e->dirty = 1;
}

void del_lines(Ed *e, int a, int b) {
    if (a < 1 || b > (int)e->count || a > b) { set_err(e, "invalid address"); return; }
    int num = b - a + 1;
    for (int i = a; i <= b; i++) line_release(&e->lines[i-1]);
    for (int i = b; i < (int)e->count; i++) e->lines[i-num] = e->lines[i];
    e->count -= num;
    e->curr = (a > (int)e->count) ? (int)e->count : a;
    if (e->curr == 0 && e->count > 0) e->curr = 1;
    e->dirty = 1;
}
