#include <sys/param.h>
#include <sys/module.h>
#include <sys/kauth.h>
#include <sys/conf.h>
#include <sys/systm.h>
#include <sys/ioccom.h>
#include <sys/kmem.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/namei.h>

#include "kat.h"

/* --- Globals --- */
static kauth_listener_t kat_listener;
static struct kat_rule kat_table[KAT_MAX_RULES];
static int kat_rule_count = 0;
static kmutex_t kat_mtx;

/* --- Prototypes --- */
dev_type_open(katopen);
dev_type_close(katclose);
dev_type_ioctl(katioctl);

const struct cdevsw kat_cdevsw = {
    .d_open = katopen,
    .d_close = katclose,
    .d_read = noread,
    .d_write = nowrite,
    .d_ioctl = katioctl,
    .d_stop = nostop,
    .d_tty = notty,
    .d_poll = nopoll,
    .d_mmap = nommap,
    .d_kqfilter = nokqfilter,
    .d_discard = nodiscard,
    .d_flag = D_OTHER
};

/* --- Character Device Logic --- */

int
katopen(dev_t dev, int flags, int fmt, struct lwp *l)
{
    return 0;
}

int
katclose(dev_t dev, int flags, int fmt, struct lwp *l)
{
    return 0;
}

int
katioctl(dev_t dev, u_long cmd, void *data, int flag, struct lwp *l)
{
    int error = 0;

    mutex_enter(&kat_mtx);
    switch (cmd) {
    case KATIOC_ADD_RULE:
        if (kat_rule_count >= KAT_MAX_RULES) {
            error = ENOMEM;
            break;
        }
        memcpy(&kat_table[kat_rule_count], data, sizeof(struct kat_rule));
        printf("KAT: Added rule for UID %d, path: %s\n", 
               kat_table[kat_rule_count].uid, kat_table[kat_rule_count].path);
        kat_rule_count++;
        break;

    case KATIOC_CLEAR_RULES:
        kat_rule_count = 0;
        printf("KAT: Access table cleared.\n");
        break;

    case KATIOC_GET_COUNT:
        *(int *)data = kat_rule_count;
        break;

    default:
        error = ENOTTY;
        break;
    }
    mutex_exit(&kat_mtx);

    return error;
}

/* --- Kauth Listener --- */

static int
kat_scope_system_cb(kauth_cred_t cred, kauth_action_t action,
    void *cookie, void *arg0, void *arg1, void *arg2, void *arg3)
{
    uid_t uid = kauth_cred_getuid(cred);
    int result = KAUTH_RESULT_DEFER;

    /* * Example: If a user tries to change the time, 
     * check if their UID has KAT_PRIV_SYS_TIME in our table.
     */
    if (action == KAUTH_SYSTEM_TIME) {
        mutex_enter(&kat_mtx);
        for (int i = 0; i < kat_rule_count; i++) {
            if (kat_table[i].uid == uid && 
               (kat_table[i].privileges & KAT_PRIV_SYS_TIME)) {
                result = KAUTH_RESULT_ALLOW;
                break;
            }
        }
        mutex_exit(&kat_mtx);
    }

    return result;
}

/* --- Module Glue --- */

MODULE(MODULE_CLASS_MISC, kat, NULL);

static int
kat_modcmd(modcmd_t cmd, void *arg)
{
    int bmajor = -1, cmajor = -1;

    switch (cmd) {
    case MODULE_CMD_INIT:
        mutex_init(&kat_mtx, MUTEX_DEFAULT, IPL_NONE);
        
        if (devsw_attach("kat", NULL, &bmajor, &kat_cdevsw, &cmajor)) {
            mutex_destroy(&kat_mtx);
            return ENXIO;
        }

        kat_listener = kauth_listen_scope(KAUTH_SCOPE_SYSTEM,
            kat_scope_system_cb, NULL);
        
        printf("KAT: Kernel Access Table Loaded (major is %d)\n", cmajor);
        return 0;

    case MODULE_CMD_FINI:
        kauth_unlisten_scope(kat_listener);
        devsw_detach(NULL, &kat_cdevsw);
        mutex_destroy(&kat_mtx);
        return 0;

    default:
        return ENOTTY;
    }
}
