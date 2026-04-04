/*
 * Kayoko - Source Code
 * 
 * Copyright (c) 2026 The Kayoko Project. All Rights Reserved
 * 
 * This file is licensed under the Common Development and Distribution License (CDDL).
 * 
 * See /usr/src/COPYING for details.
 */

#ifndef KAYOKO_OS_H
#define KAYOKO_OS_H

#define OS_BASE_NAME "Kayoko"

/* --- 1. The Big Players (Mainstream Unix/Linux/BSD) --- */
#if defined(PERS_LINUX)
    #define OS_SUBTYPE "Linux"
#elif defined(PERS_SUNOS)
    #define OS_SUBTYPE "Solaris"
#elif defined(PERS_DARWIN)
    #define OS_SUBTYPE "macOS"
#elif defined(PERS_FREEBSD)
    #define OS_SUBTYPE "FreeBSD"
#elif defined(PERS_NETBSD)
    #define OS_SUBTYPE "NetBSD"
#elif defined(PERS_OPENBSD)
    #define OS_SUBTYPE "OpenBSD"
#elif defined(PERS_DRAGONFLY)
    #define OS_SUBTYPE "DragonFly"

/* --- 2. Classic Commercial Unix (The Titans) --- */
#elif defined(PERS_AIX)
    #define OS_SUBTYPE "AIX"
#elif defined(PERS_HPUX)
    #define OS_SUBTYPE "HP-UX"
#elif defined(PERS_IRIX)
    #define OS_SUBTYPE "IRIX"
    // #define OS_SUBTYPE "Tru64"
#elif defined(PERS_OSF1)
    #define OS_SUBTYPE "Digital-Unix"
#elif defined(PERS_ULTRIX)
    #define OS_SUBTYPE "Ultrix"
#elif defined(PERS_SCO_SV)
    #define OS_SUBTYPE "OpenServer"
#elif defined(PERS_UNIXWARE)
    #define OS_SUBTYPE "UnixWare"
#elif defined(PERS_RELIANTUNIX)
    #define OS_SUBTYPE "Reliant"
#elif defined(PERS_DYNIX)
    #define OS_SUBTYPE "Dynix/ptx"
#elif defined(PERS_AUX)
    #define OS_SUBTYPE "A/UX"

/* --- 3. Microkernels & Hobbyist Systems --- */
#elif defined(PERS_REDOX)
    #define OS_SUBTYPE "Redox"
#elif defined(PERS_MINIX)
    #define OS_SUBTYPE "MINIX"
#elif defined(PERS_QNX)
    #define OS_SUBTYPE "QNX"
#elif defined(PERS_HAIKU)
    #define OS_SUBTYPE "Haiku"
#elif defined(PERS_BEOS)
    #define OS_SUBTYPE "BeOS"
#elif defined(PERS_SYLLABLE)
    #define OS_SUBTYPE "Syllable"
#elif defined(PERS_HELENOS)
    #define OS_SUBTYPE "HelenOS"
#elif defined(PERS_PLAN9)
    #define OS_SUBTYPE "Plan9"
#elif defined(PERS_INFERNO)
    #define OS_SUBTYPE "Inferno"
#elif defined(PERS_AROS)
    #define OS_SUBTYPE "AROS"
#elif defined(PERS_MORPHOS)
    #define OS_SUBTYPE "MorphOS"
#elif defined(PERS_AMIGAOS)
    #define OS_SUBTYPE "AmigaOS"

/* --- 4. Mainframe & Real-time --- */
#elif defined(PERS_ZOS)
    #define OS_SUBTYPE "z/OS"
#elif defined(PERS_OS400)
    #define OS_SUBTYPE "OS/400"
#elif defined(PERS_VMS) || defined(PERS_OPENVMS)
    #define OS_SUBTYPE "OpenVMS"
#elif defined(PERS_LYNXOS)
    #define OS_SUBTYPE "LynxOS"
#elif defined(PERS_RTEMS)
    #define OS_SUBTYPE "RTEMS"
#elif defined(PERS_NUCLEUS)
    #define OS_SUBTYPE "Nucleus"
#elif defined(PERS_VXWORKS)
    #define OS_SUBTYPE "VxWorks"

/* --- 5. Mobile & Embedded --- */
#elif defined(PERS_ANDROID)
    #define OS_SUBTYPE "Android"
#elif defined(PERS_TIZEN)
    #define OS_SUBTYPE "Tizen"
#elif defined(PERS_SAILFISH)
    #define OS_SUBTYPE "Sailfish"
#elif defined(PERS_WEBOS)
    #define OS_SUBTYPE "webOS"

/* --- 6. The "Others" & Legacy --- */
#elif defined(PERS_CYGWIN_NT)
    #define OS_SUBTYPE "Cygwin"
#elif defined(PERS_MSYS_NT)
    #define OS_SUBTYPE "MSYS2"
#elif defined(PERS_GNU_KFREEBSD)
    #define OS_SUBTYPE "GNU/kFreeBSD"
#elif defined(PERS_GNU)
    #define OS_SUBTYPE "GNU Hurd"
#elif defined(PERS_GENODE)
    #define OS_SUBTYPE "Genode"
#elif defined(PERS_NEXTSTEP)
    #define OS_SUBTYPE "NeXTSTEP"
#elif defined(PERS_OPENSTEP)
    #define OS_SUBTYPE "OPENSTEP"
#elif defined(PERS_SUNOS)
    #define OS_SUBTYPE "Solaris"
#elif defined(PERS_KAYOKO)
    /* Native: Leave undefined to trigger the clean name */
    #undef OS_SUBTYPE
#else
    #undef OS_SUBTYPE
#endif

/* --- Final Name Construction --- */
#ifdef OS_SUBTYPE
    #define OS_FULL_NAME OS_BASE_NAME "/" OS_SUBTYPE
#else
    #define OS_FULL_NAME OS_BASE_NAME
#endif

#endif
