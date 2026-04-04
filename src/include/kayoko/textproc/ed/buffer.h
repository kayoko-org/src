/*
 * Kayoko - Source Code
 * 
 * Copyright (c) 2026 The Kayoko Project. All Rights Reserved
 * 
 * This file is licensed under the Common Development and Distribution License (CDDL).
 * 
 * See /usr/src/COPYING for details.
 */

#ifndef KAYOKO_TEXTPROC_BUFFER_H
#define KAYOKO_TEXTPROC_BUFFER_H

#include <kayoko/textproc/ed.h>

/* Memory Management */
void line_add_ref(Line *l);
void line_release(Line *l);
void update_line_text(Line *l, const char *new_text);

/* Buffer Operations */
void add_line(Ed *e, int at, const char *s);
void del_lines(Ed *e, int a, int b);
void join_lines(Ed *e, int a, int b);

/* Undo System */
void save_undo(Ed *e);
void do_undo(Ed *e);

#endif
