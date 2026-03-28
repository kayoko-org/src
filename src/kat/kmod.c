#include <sys/param.h>
#include <sys/module.h>
#include <sys/kauth.h>
#include <sys/conf.h>
#include <sys/systm.h>
#include <sys/ioccom.h>
#include <sys/syslog.h>
#include <sys/mutex.h>
#include <sys/vnode.h>
#include <sys/kmem.h>

#include "kat.h"

/* --- Globals --- */
static struct kat_rule kat_table[KAT_MAX_RULES];
static int kat_rule_count = 0;
static kmutex_t kat_mtx;

/* Listener handles for unregistration */
static kauth_listener_t kat_sys_lnr;
static kauth_listener_t kat_vfs_lnr;
static kauth_listener_t kat_net_lnr;
static kauth_listener_t kat_prc_lnr;

/* --- Helpers --- */

static void
kat_log(uid_t uid, const char *scope, uint64_t priv, int result)
{
    log(LOG_AUTHPRIV | LOG_NOTICE, 
        "KAT: [UID %d] Scope: %s, Priv: 0x%llx -> %s\n",
        uid, scope, (unsigned long long)priv, 
        (result == KAUTH_RESULT_ALLOW) ? "ALLOWED" : "DEFERRED");
}

static int
kat_check(uid_t uid, uint64_t required_priv, const char *current_path)
{
    int allowed = 0;

    mutex_enter(&kat_mtx);
    for (int i = 0; i < kat_rule_count; i++) {
        if (!kat_table[i].active || kat_table[i].uid != uid)
            continue;

        /* Check if the privilege bit matches */
        if (kat_table[i].privileges & required_priv) {
            
            /* Path validation (Global or Match) */
            if (kat_table[i].path[0] == '\0' || 
               (current_path && strcmp(kat_table[i].path, current_path) == 0)) {
                
                /* EXPLICIT RESTRICTION: Immediate exit with DEFER/DENY */
                if (kat_table[i].type == KAT_TYPE_RESTRICT) {
                    mutex_exit(&kat_mtx);
                    return KAUTH_RESULT_DENY; 
                }

                if (kat_table[i].type == KAT_TYPE_ALLOW) {
                    allowed = 1;
                }
            }
        }
    }
    mutex_exit(&kat_mtx);

    return (allowed) ? KAUTH_RESULT_ALLOW : KAUTH_RESULT_DENY;
}

/* --- Scope Callbacks --- */

static int
kat_sys_cb(kauth_cred_t cred, kauth_action_t action, void *cookie,
    void *arg0, void *arg1, void *arg2, void *arg3)
{
    uint64_t priv = 0;
    switch (action) {
        case KAUTH_SYSTEM_TIME:   priv = KAT_PRIV_SYS_TIME; break;
        case KAUTH_SYSTEM_MODULE: priv = KAT_PRIV_SYS_MODULE; break;
        case KAUTH_SYSTEM_REBOOT: priv = KAT_PRIV_SYS_REBOOT; break;
    }

    if (priv == 0) return KAUTH_RESULT_DEFER;
    int res = kat_check(kauth_cred_getuid(cred), priv, NULL);
    if (res == KAUTH_RESULT_ALLOW) kat_log(kauth_cred_getuid(cred), "SYSTEM", priv, res);
    return res;
}

static int
kat_vfs_cb(kauth_cred_t cred, kauth_action_t action, void *cookie,
    void *arg0, void *arg1, void *arg2, void *arg3)
{
    uint64_t priv = 0;
    uid_t uid = kauth_cred_getuid(cred);
    struct proc *p = curproc;
    const char *path = (p != NULL) ? p->p_path : NULL;

    /* Catch Write/Create/Delete/Rename */
    if (action & (KAUTH_VNODE_WRITE_DATA | KAUTH_VNODE_ADD_FILE | 
                  KAUTH_VNODE_DELETE | KAUTH_VNODE_RENAME |
                  KAUTH_VNODE_ADD_SUBDIRECTORY)) {
        priv = KAT_PRIV_VFS_WRITE;
    }
    /* Catch Read/Search/Attributes */
    else if (action & (KAUTH_VNODE_READ_DATA | KAUTH_VNODE_LIST_DIRECTORY | 
                       KAUTH_VNODE_READ_ATTRIBUTES | KAUTH_VNODE_ACCESS)) {
        priv = KAT_PRIV_VFS_READ;
    }

    if (priv == 0) return KAUTH_RESULT_DEFER;

    int res = kat_check(uid, priv, path);
    if (res == KAUTH_RESULT_ALLOW) {
        // kat_log(uid, "VNODE", priv, res); // Commented out to reduce dmesg spam
        return KAUTH_RESULT_ALLOW;
    }

    return KAUTH_RESULT_DEFER;
}

static int
kat_net_cb(kauth_cred_t cred, kauth_action_t action, void *cookie,
    void *arg0, void *arg1, void *arg2, void *arg3)
{
    uint64_t priv = 0;
    uid_t uid = kauth_cred_getuid(cred);
    struct proc *p = curproc;

    switch (action) {
        case KAUTH_NETWORK_BIND:       priv = KAT_PRIV_NET_BIND; break;
        case KAUTH_NETWORK_SOCKET:  priv = KAT_PRIV_NET_RAW; break;
    }

    if (priv == 0) return KAUTH_RESULT_DEFER;

    // Use curproc->p_path here for the check
    int res = kat_check(uid, priv, p ? p->p_path : NULL);
    if (res == KAUTH_RESULT_ALLOW) return KAUTH_RESULT_ALLOW;

    return KAUTH_RESULT_DEFER;
}

/* --- Devsw Logic --- */

dev_type_open(katopen);
dev_type_close(katclose);
dev_type_ioctl(katioctl);

const struct cdevsw kat_cdevsw = {
    .d_open = katopen, .d_close = katclose,
    .d_read = noread, .d_write = nowrite,
    .d_ioctl = katioctl, .d_stop = nostop,
    .d_tty = notty, .d_poll = nopoll,
    .d_mmap = nommap, .d_kqfilter = nokqfilter,
    .d_discard = nodiscard, .d_flag = D_OTHER
};

int katopen(dev_t dev, int flags, int fmt, struct lwp *l) { return 0; }
int katclose(dev_t dev, int flags, int fmt, struct lwp *l) { return 0; }

int
katioctl(dev_t dev, u_long cmd, void *data, int flag, struct lwp *l)
{
    int error = 0;
    uid_t caller_uid = kauth_cred_getuid(l->l_cred);

    switch (cmd) {
    case KATIOC_ADD_RULE:
        /* --- BOOTSTRAP BYPASS --- 
         * If the caller is root (UID 0), we allow the IOCTL even if
         * the KAT table doesn't have an explicit rule yet. 
         */
        if (caller_uid != 0) {
            if (kauth_authorize_system(l->l_cred, KAUTH_SYSTEM_MODULE, 0, NULL, NULL, NULL) != 0)
                return EPERM;
        }

        mutex_enter(&kat_mtx);
        if (kat_rule_count < KAT_MAX_RULES) {
            memcpy(&kat_table[kat_rule_count++], data, sizeof(struct kat_rule));
        } else {
            error = ENOSPC;
        }
        mutex_exit(&kat_mtx);
        break;

    case KATIOC_CLEAR_RULES:
        /* Same bypass for clearing rules so you don't lock yourself out */
        if (caller_uid != 0) {
            if (kauth_authorize_system(l->l_cred, KAUTH_SYSTEM_MODULE, 0, NULL, NULL, NULL) != 0)
                return EPERM;
        }

        mutex_enter(&kat_mtx);
        kat_rule_count = 0;
        mutex_exit(&kat_mtx);
        break;

    default: 
        error = ENOTTY;
    }
    return error;
}

/* --- Module Lifecycle --- */

MODULE(MODULE_CLASS_MISC, kat, NULL);

static int
kat_modcmd(modcmd_t cmd, void *arg)
{
    int bmajor = -1, cmajor = -1;

    switch (cmd) {
    case MODULE_CMD_INIT:
        mutex_init(&kat_mtx, MUTEX_DEFAULT, IPL_NONE);
        if (devsw_attach(KAT_DEV_NAME, NULL, &bmajor, &kat_cdevsw, &cmajor))
            return ENXIO;

        kat_sys_lnr = kauth_listen_scope(KAUTH_SCOPE_SYSTEM, kat_sys_cb, NULL);
        kat_vfs_lnr = kauth_listen_scope(KAUTH_SCOPE_VNODE, kat_vfs_cb, NULL);
        kat_net_lnr = kauth_listen_scope(KAUTH_SCOPE_NETWORK, kat_net_cb, NULL);

        printf("Kernel Authorization Table (KAT RBAC) initialized... %d\n", cmajor);
        return 0;

    case MODULE_CMD_FINI:
        kauth_unlisten_scope(kat_sys_lnr);
        kauth_unlisten_scope(kat_vfs_lnr);
        kauth_unlisten_scope(kat_net_lnr);
        devsw_detach(NULL, &kat_cdevsw);
        mutex_destroy(&kat_mtx);
        return 0;
    default: return ENOTTY;
    }
}
