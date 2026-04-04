/*
 * Kayoko - Source Code
 * 
 * Copyright (c) 2026 The Kayoko Project. All Rights Reserved
 * 
 * This file is licensed under the Common Development and Distribution License (CDDL).
 * 
 * See /usr/src/COPYING for details.
 */

#ifndef _KAT_H_
#define _KAT_H_

#include <sys/types.h>
#include <sys/ioccom.h>

/* --- Limits --- */
#define KAT_MAX_PATH    256
#define KAT_MAX_RULES   1024
#define KAT_DEV_NAME    "kat"
#define KAT_TYPE_ALLOW    1
#define KAT_TYPE_RESTRICT 2

/* * --- KAT Privilege Bitmask ---
 * These map to NetBSD 11 KAUTH scopes.
 */

/* KAUTH_SCOPE_SYSTEM */
#define KAT_PRIV_SYS_TIME       (1ULL << 0)  /* Change clock/ntp */
#define KAT_PRIV_SYS_MODULE     (1ULL << 1)  /* Load/Unload kmods */
#define KAT_PRIV_SYS_REBOOT     (1ULL << 2)  /* Shutdown/Reboot */
#define KAT_PRIV_SYS_CHROOT     (1ULL << 3)  /* Use chroot(2) */
#define KAT_PRIV_SYS_SYSCTL     (1ULL << 4)  /* Write to sensitive sysctls */

/* KAUTH_SCOPE_PROCESS */
#define KAT_PRIV_PROC_EXEC      (1ULL << 10) /* Execute authorized binaries */
#define KAT_PRIV_PROC_DEBUG     (1ULL << 11) /* ptrace/mem access to others */
#define KAT_PRIV_PROC_SETID     (1ULL << 12) /* setuid/setgid privilege */
#define KAT_PRIV_PROC_NICE      (1ULL << 13) /* Increase process priority */

/* KAUTH_SCOPE_NETWORK */
#define KAT_PRIV_NET_BIND       (1ULL << 20) /* Bind to ports < 1024 */
#define KAT_PRIV_NET_FIREWALL   (1ULL << 21) /* Modify NPF/IPF rules */
#define KAT_PRIV_NET_RAW        (1ULL << 22) /* Open raw sockets */

/* KAUTH_SCOPE_VFS */
#define KAT_PRIV_VFS_WRITE      (1ULL << 30) /* Bypass file write perms */
#define KAT_PRIV_VFS_READ       (1ULL << 31) /* Bypass file read perms */
#define KAT_PRIV_VFS_RETAIN     (1ULL << 32) /* Sticky bit/Ownership bypass */

/* KAUTH_SCOPE_DEVICE */
#define KAT_PRIV_DEV_RAWIO      (1ULL << 40) /* Direct /dev/mem or disk access */

/* --- Data Structures --- */

struct kat_rule {
    uid_t    uid;                /* Target User ID */
    uint64_t privileges;         /* Bitmask of allowed KAT_PRIV_* */
    char     path[KAT_MAX_PATH]; /* Primary binary or target path */
int      type;
    int      active;             /* Rule status toggle */
};

/* --- IOCTL Interface --- */

/* Add a rule to the kernel table */
#define KATIOC_ADD_RULE      _IOW('K', 1, struct kat_rule)

/* Clear all rules from the kernel table */
#define KATIOC_CLEAR_RULES   _IO('K', 2)

/* Retrieve the current number of active rules */
#define KATIOC_GET_COUNT     _IOR('K', 3, int)

/* Retrieve a specific rule by index for auditing */
#define KATIOC_GET_RULE      _IOWR('K', 4, struct kat_rule)

#endif /* _KAT_H_ */
