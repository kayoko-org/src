/*
 * Kayoko - Source Code
 * 
 * Copyright (c) 2026 The Kayoko Project. All Rights Reserved
 * 
 * This file is licensed under the Common Development and Distribution License (CDDL).
 * 
 * See /usr/src/COPYING for details.
 */

#ifndef KAYOKO_TEXTPROC_PARSER_H
#define KAYOKO_TEXTPROC_PARSER_H

#include <kayoko/textproc/ed.h>

/* * Returns 1 if an address was found, 0 otherwise. 
 * Updates the string pointer 's' to the next character after the address.
 */
int get_addr(Ed *e, char **s, int *addr);

#endif
