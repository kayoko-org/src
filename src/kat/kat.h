#ifndef _KAT_H_
#define _KAT_H_

#include <sys/types.h>
#include <sys/ioccom.h>

/* Limits */
#define KAT_MAX_PATH    256
#define KAT_MAX_USER    32
#define KAT_MAX_RULES   512

/* * Privileges bitmask (Solaris-style)
 * We can map these to kauth scopes later.
 */
#define KAT_PRIV_NET_BIND     (1 << 0)  /* Bind to privileged ports */
#define KAT_PRIV_SYS_TIME     (1 << 1)  /* Change system clock */
#define KAT_PRIV_PROC_EXEC     (1 << 2)  /* Execute specific binaries */
#define KAT_PRIV_VFS_WRITE    (1 << 3)  /* Bypass certain file perms */

/* The core rule structure */
struct kat_rule {
    uid_t   uid;                    /* User this rule applies to */
    char    path[KAT_MAX_PATH];     /* Path to the authorized binary */
    uint32_t privileges;            /* Bitmask of allowed KAT_PRIV_* */
};

/* IOCTL Definitions */
#define KATIOC_ADD_RULE    _IOW('K', 1, struct kat_rule)
#define KATIOC_CLEAR_RULES _IO('K', 2)
#define KATIOC_GET_COUNT   _IOR('K', 3, int)

#endif /* _KAT_H_ */
