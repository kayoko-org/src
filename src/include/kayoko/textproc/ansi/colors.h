/*
 * Kayoko - Source Code
 * 
 * Copyright (c) 2026 The Kayoko Project. All Rights Reserved
 * 
 * This file is licensed under the Common Development and Distribution License (CDDL).
 * 
 * See /usr/src/COPYING for details.
 */

#ifndef KAYOKO_TEXTPROC_ANSI_COLORS_H
#define KAYOKO_TEXTPROC_ANSI_COLORS_H


/* --- ANSI Base Sequences --- */
#define KYK_ANSI_CSI         "\x1b["
#define KYK_ANSI_RESET_ALL   "0"

/* --- Text Attributes --- */
#define KYK_ATTR_RESET       KYK_ANSI_CSI "0m"
#define KYK_ATTR_BOLD        KYK_ANSI_CSI "1m"
#define KYK_ATTR_DIM         KYK_ANSI_CSI "2m"
#define KYK_ATTR_ITALIC      KYK_ANSI_CSI "3m"
#define KYK_ATTR_UNDERLINE   KYK_ANSI_CSI "4m"
#define KYK_ATTR_BLINK       KYK_ANSI_CSI "5m"
#define KYK_ATTR_REVERSE     KYK_ANSI_CSI "7m"
#define KYK_ATTR_HIDDEN      KYK_ANSI_CSI "8m"

/* --- Foreground Colors (Standard) --- */
#define KYK_FG_BLACK         KYK_ANSI_CSI "30m"
#define KYK_FG_RED           KYK_ANSI_CSI "31m"
#define KYK_FG_GREEN         KYK_ANSI_CSI "32m"
#define KYK_FG_YELLOW        KYK_ANSI_CSI "33m"
#define KYK_FG_BLUE          KYK_ANSI_CSI "34m"
#define KYK_FG_MAGENTA       KYK_ANSI_CSI "35m"
#define KYK_FG_CYAN          KYK_ANSI_CSI "36m"
#define KYK_FG_WHITE         KYK_ANSI_CSI "37m"
#define KYK_FG_DEFAULT       KYK_ANSI_CSI "39m"

/* --- Foreground Colors (Bright) --- */
#define KYK_FG_BRIGHT_BLACK   KYK_ANSI_CSI "90m"
#define KYK_FG_BRIGHT_RED     KYK_ANSI_CSI "91m"
#define KYK_FG_BRIGHT_GREEN   KYK_ANSI_CSI "92m"
#define KYK_FG_BRIGHT_YELLOW  KYK_ANSI_CSI "93m"
#define KYK_FG_BRIGHT_BLUE    KYK_ANSI_CSI "94m"
#define KYK_FG_BRIGHT_MAGENTA KYK_ANSI_CSI "95m"
#define KYK_FG_BRIGHT_CYAN    KYK_ANSI_CSI "96m"
#define KYK_FG_BRIGHT_WHITE   KYK_ANSI_CSI "97m"

/* --- Background Colors (Standard) --- */
#define KYK_BG_BLACK         KYK_ANSI_CSI "40m"
#define KYK_BG_RED           KYK_ANSI_CSI "41m"
#define KYK_BG_GREEN         KYK_ANSI_CSI "42m"
#define KYK_BG_YELLOW        KYK_ANSI_CSI "43m"
#define KYK_BG_BLUE          KYK_ANSI_CSI "44m"
#define KYK_BG_MAGENTA       KYK_ANSI_CSI "45m"
#define KYK_BG_CYAN          KYK_ANSI_CSI "46m"
#define KYK_BG_WHITE         KYK_ANSI_CSI "47m"
#define KYK_BG_DEFAULT       KYK_ANSI_CSI "49m"

/* --- Background Colors (Bright) --- */
#define KYK_BG_BRIGHT_BLACK   KYK_ANSI_CSI "100m"
#define KYK_BG_BRIGHT_RED     KYK_ANSI_CSI "101m"
#define KYK_BG_BRIGHT_GREEN   KYK_ANSI_CSI "102m"
#define KYK_BG_BRIGHT_YELLOW  KYK_ANSI_CSI "103m"
#define KYK_BG_BRIGHT_BLUE    KYK_ANSI_CSI "104m"
#define KYK_BG_BRIGHT_MAGENTA KYK_ANSI_CSI "105m"
#define KYK_BG_BRIGHT_CYAN    KYK_ANSI_CSI "106m"
#define KYK_BG_BRIGHT_WHITE   KYK_ANSI_CSI "107m"

/* --- Logical Intent Mapping --- */
typedef enum {
    KYK_COLOR_PRIMARY,   /* Default system color */
    KYK_COLOR_SUCCESS,   /* Positive outcomes (Green) */
    KYK_COLOR_INFO,      /* Neutral info (Cyan/Blue) */
    KYK_COLOR_WARNING,   /* Potential issues (Yellow) */
    KYK_COLOR_DANGER,    /* Errors/Critical (Red) */
    KYK_COLOR_MUTE,      /* De-emphasized text (Grey) */
    KYK_COLOR_HEADING    /* Titles/Section headers (Bold/Bright) */
} kyk_color_intent_t;

/**
 * kyk_get_color_str
 * Maps a logical intent to a physical ANSI escape string.
 * static inline to avoid linkage issues across multiple includes.
 */
static inline const char* kyk_get_color_str(kyk_color_intent_t intent) 
{
    switch (intent) {
        case KYK_COLOR_SUCCESS: return KYK_FG_GREEN;
        case KYK_COLOR_INFO:    return KYK_FG_CYAN;
        case KYK_COLOR_WARNING: return KYK_FG_YELLOW;
        case KYK_COLOR_DANGER:  return KYK_FG_RED;
        case KYK_COLOR_MUTE:    return KYK_FG_BRIGHT_BLACK;
        case KYK_COLOR_HEADING: return KYK_ATTR_BOLD KYK_FG_BRIGHT_WHITE;
        case KYK_COLOR_PRIMARY:
        default:                return KYK_ATTR_RESET;
    }
}

#endif /* KAYOKO_TEXTPROC_ANSI_COLORS_H */
