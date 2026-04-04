/*
 * Kayoko - Source Code
 * 
 * Copyright (c) 2026 The Kayoko Project. All Rights Reserved
 * 
 * This file is licensed under the Common Development and Distribution License (CDDL).
 * 
 * See /usr/src/COPYING for details.
 */

#ifndef KAYOKO_TEXTPROC_PAGER_MORE_H
#define KAYOKO_TEXTPROC_PAGER_MORE_H

#if defined(__sun) || defined(__hpux)
#include <sys/termios.h>
#endif
#include <stdio.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <signal.h>
#include <kayoko/textproc/ansi/colors.h>

#ifdef KAYOKO_TEXTPROC_PAGER_MORE_IMPLEMENTATION

/* Global for terminal state restoration */
static struct termios orig_termios;

void kyk_pager_init() {
    struct termios newt;
    tcgetattr(STDIN_FILENO, &orig_termios);
    newt = orig_termios;
    /* Ignore ^C (ISIG) and disable echoing/canonical mode */
    newt.c_lflag &= ~(ICANON | ECHO | ISIG);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
}

void kyk_pager_restore() {
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
}

int kyk_get_term_height() {
    struct winsize w;
    return (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) ? w.ws_row : 24;
}

void kyk_draw_footer(int perc) {
    fprintf(stderr, "%s-- More -- (%d%%)%s", KYK_ATTR_REVERSE, perc, KYK_ATTR_RESET);
    fflush(stderr);
}

void kyk_clear_footer() {
    fprintf(stderr, "\r\033[K");
    fflush(stderr);
}

#endif
#endif
