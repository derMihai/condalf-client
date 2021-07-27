/*
 * Copyright (C) 2021 Mihai Renea <mihai.renea@fu-berlin.de>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * The Long Term Buffering implementation uses a dispatch queue to synchronize
 * accesses to the global LTB subsystem data structures (including file system).
 * While seemingly complicated, in this special case it actually simplifies the
 * implementation a lot, by removing the need for complex synchronization
 * mechanisms: everything that touches global objects happens serially, in the
 * same thread.
 *
 * This implementation also handles some time-intensive file system operations
 * that need mutual exclusion, so by asynchronously dispatching we can delay
 * execution without blocking other threads.
 *
 * This is inspired by Apple's Grand Central Dispatch.
 * @see https://en.wikipedia.org/wiki/Grand_Central_Dispatch */

#if CONDALF_USE_LTB == 1

#include "condalf_config.h"
#include "ltb.h"
#include "thread.h"
#include "cond.h"
#include "vfs.h"
#include "data_pool.h"
#include "malloc.h"
#include <fcntl.h>
#include <stdbool.h>
#include <string.h>

#define DLOG_LEVEL DLOG_INF
#include "dlog.h"

typedef struct ltb ltb_t;

struct ltb {
    transdrv_t driv;
    ltb_t *next;
    char *pooldir;
    transdrv_t *sender;
    union {
        struct {
            char _padding[2];
            char name[LTB_NAME_LEN_MAX + 1];
        };
        char tmpfil_name[2 + LTB_NAME_LEN_MAX + 1];
    };
};

static transdrv_itf_t const ltb_impl;

static kernel_pid_t _ltb_queue = KERNEL_PID_UNDEF;
static size_t       _nb_files_lim;
static ltb_t        *_ltb_lhead = NULL;
static ssize_t      _nb_files_total;
static bool         _publishing;
static bool        (*_ext_cond)(void) = NULL;

#define LTB_QUEUE_MSGQUEUE_LEN 4

#define DISPATCH_TYPE_ASYNC 0
#define DISPATCH_TYPE_SYNC  1

typedef void (*dispatch_cb_t)(void *);
typedef struct dispatch_unit dispatch_unit_t;
struct dispatch_unit {
    dispatch_cb_t cb;
    void *arg;
};

typedef void *(*dispatch_sync_cb_t)(void *);
typedef struct dispatch_sync_unit dispatch_sync_unit_t;
struct dispatch_sync_unit {
    dispatch_sync_cb_t cb;
    void *arg;
    void *retval;
    mutex_t lock;
    cond_t cond;
};

static int _ltb_dispatch(
    dispatch_cb_t cb,
    void *arg)
{
    dispatch_unit_t *unit = malloc(sizeof(*unit));
    if (!unit) return -ENOMEM;

    unit->cb = cb;
    unit->arg = arg;

    msg_t msg = {
        .type = DISPATCH_TYPE_ASYNC,
        .content.ptr = unit
    };

    int res = msg_try_send(&msg, _ltb_queue);
    if (res != 1) {
        DERR("cannot dispatch: %d", res);
        free(unit);
        return -EWOULDBLOCK;
    }
    return 0;
}

static void *_ltb_dispatch_sync(
    dispatch_sync_cb_t cb,
    void *arg)
{
    dispatch_sync_unit_t unit = {
        .cb = cb,
        .arg = arg,
    };

    mutex_init(&unit.lock);
    cond_init(&unit.cond);

    msg_t msg = {
        .type = DISPATCH_TYPE_SYNC,
        .content.ptr = &unit
    };

    mutex_lock(&unit.lock);

    msg_send(&msg, _ltb_queue);

    cond_wait(&unit.cond, &unit.lock);
    mutex_unlock(&unit.lock);

    return unit.retval;
}

static int _ltb_get_first_file(char fname[static 64], ltb_t **ltb)
{
    ltb_t *currltb = _ltb_lhead;
    int res = -ENOENT;

    while (currltb) {
        if (!currltb->sender) {
            DDBG("LTB %s skipped: has no publisher\n", currltb->name);
            currltb = currltb->next;
            continue;
        }

        res = dpool_get_oldest_file(currltb->pooldir, fname, 64);

        if (res < 0) {
            currltb = currltb->next;

        } else {
            DDBG("found %s\n", fname);
            *ltb = currltb;
            break;
        }
    }

    return res;
}

static void *_ltb_dispatcher(void *arg)
{
    static msg_t msg_queue[LTB_QUEUE_MSGQUEUE_LEN];
    msg_init_queue(msg_queue, LTB_QUEUE_MSGQUEUE_LEN);
    msg_t msg;

    while (1) {
        msg_receive(&msg);

        switch (msg.type) {
        case DISPATCH_TYPE_ASYNC:
        {
            dispatch_unit_t *unit = (dispatch_unit_t *)msg.content.ptr;
            unit->cb(unit->arg);

            free(unit);

            break;
        }
        case DISPATCH_TYPE_SYNC:
        {
            dispatch_sync_unit_t *unit = (dispatch_sync_unit_t *)msg.content.ptr;
            unit->retval = unit->cb(unit->arg);

            mutex_lock(&unit->lock);
            cond_signal(&unit->cond);
            mutex_unlock(&unit->lock);

            break;
        }
        default:
            assert(0);
        }
    }

    return NULL;
}

static int _ltb_publish(void *arg)
{
    _publishing = true;

    static char fname[64];
    ltb_t *ltb = NULL;

    int res = _ltb_get_first_file(fname, &ltb);
    if (res < 0) {
        if (res == -ENOENT) {
            DDBG("nothing to publish!\n");
            res = 0;
        }
        goto _publish_end;
    }

    DINF("publishing file %s ...\n", fname);

    res = vfs_open(fname, O_RDONLY, 0);
    if (res < 0) goto _publish_end;
    int fd = res;

    transfer_job_t job = {
        .cb = NULL,
        .fd = fd
    };
    res = transdrv_send(ltb->sender, &job);
    vfs_close(fd);
    if (res < 0) {
        DERR("transfer_send err: %d", res);
        goto _publish_end;
    }

    res = vfs_unlink(fname);
    if (res < 0){
        DERR("unlink fail: %d\n", res);
    } else {
        _nb_files_total--;
    }

    res = _ltb_dispatch((dispatch_cb_t)_ltb_publish, NULL);
    if (res < 0) goto _publish_end;

    return 0;

_publish_end:
    if (arg) {
        ((void (*)(int))arg)(res);
    }
    _publishing = false;
    return res;
}

static void _ltb_upd_pub_cond(ltb_t *ltb)
{
    if (!_publishing) {
        bool ext_cond = _ext_cond ? (_ext_cond()) : true;

        if (((size_t)_nb_files_total >= _nb_files_lim) && ext_cond) {

            DINF("cond met, publishing...\n");

            _ltb_publish(NULL);
        } else {
            DDBG("cond unmet: # files=%d, limit=%d, ext=%d\n",
                _nb_files_total, _nb_files_lim, ext_cond);
        }
    }
}

int ltb_subsys_init(ltb_subsys_init_t const *init)
{
    if (!init) return -EINVAL;

    static char sender_stack[THREAD_STACKSIZE_MAIN];

    _ltb_queue = thread_create(
        sender_stack,
        sizeof(sender_stack),
        LTB_QUEUE_PRIO,
        0,
        _ltb_dispatcher,
        NULL,
        "ltb_dispatcher");

    if (_ltb_queue < 0) {
        return _ltb_queue;
    }

    _nb_files_lim = init->nb_files_lim;
    _ext_cond     = init->ext_cond;

    DDBG("done!\n");

    return 0;
}

void _force_publish(void *arg)
{
    if (!_publishing) {
        DINF("publishing...\n");
        _ltb_publish(arg);

    } else {
        DDBG("already publishing\n");
    }
}

int ltb_force_publish(void (*cb)(int res))
{
    return _ltb_dispatch(_force_publish, cb);
}

static void _add_ltb(ltb_t *ltb)
{
    int res = dpool_size(ltb->pooldir);
    if (res < 0) {
        DERR("dpool_size err: %d\n", res);
        res = 0;
    }

    _nb_files_total += res;

    DDBG("poolsize=%d, total=%d\n", res, _nb_files_total);

    ltb->next = _ltb_lhead;
    _ltb_lhead = ltb;
}

static void _remove_ltb(ltb_t *ltb)
{
    ltb_t **ltbpp = &_ltb_lhead;
    while (*ltbpp != ltb) {
        assert(*ltbpp);
        ltbpp = &(*ltbpp)->next;
    }

    *ltbpp = ltb->next;
    ltb->next = NULL;

    int res = dpool_size(ltb->pooldir);
    if (res < 0) res = 0;

    _nb_files_total -= res;

    assert(_nb_files_total >= 0);
}

int ltb_create(transdrv_t **drvpp, ltb_init_t const *init)
{
    if (!drvpp || !init) return -EINVAL;
    if (!init->pool_path ||
        !init->name ) return -EINVAL;

    int res;

    ltb_t *nltb = calloc(1, sizeof(*nltb));
    if (!nltb) return -ENOMEM;

    nltb->pooldir = strdup(init->pool_path);
    if (!nltb->pooldir) {
        res = -ENOMEM;
        goto _ltb_create_err;
    }


    strcpy(nltb->tmpfil_name, "/.");
    strncpy(nltb->name, init->name, LTB_NAME_LEN_MAX);
    nltb->name[LTB_NAME_LEN_MAX] = '\0';

    nltb->sender = init->sender;
    nltb->driv.itf = &ltb_impl;
    *drvpp = (transdrv_t *)nltb;

    _ltb_dispatch_sync((dispatch_sync_cb_t)_add_ltb, nltb);

    DINF("created: pooldir=%s, tmpf=%s\n", nltb->pooldir, nltb->tmpfil_name);

    return 0;

_ltb_create_err:
    if (nltb) {
        free(nltb->pooldir);
        free(nltb);
    }

    return res;
}

static void _ltb_try_send_disp(void *arg)
{
    transfer_job_t *job = (transfer_job_t *)arg;
    ltb_t *ltb = (ltb_t *)job->_drv_priv;

    static char buf[64];

    char tmp_path[strlen(ltb->pooldir) + strlen(ltb->tmpfil_name) + 1];
    strcpy(tmp_path, ltb->pooldir);
    strcat(tmp_path, ltb->tmpfil_name);

    DDBG("vfs_open(%s)\n", tmp_path);

    int res = vfs_open(tmp_path, O_CREAT | O_TRUNC | O_WRONLY, 0);
    if (res < 0) {
        goto _try_send_cb_end;
    }

    int dest_fd = res;
    vfs_lseek(job->fd, 0, SEEK_SET);

    while ((res = vfs_read(job->fd, buf, sizeof(buf))) > 0) {
        int written = vfs_write(dest_fd, buf, res);
        if (written != res) {
            if (written < 0) res = written;
            else res = -ENOSPC;

            break;
        }
    }

    vfs_close(dest_fd);

    if (res) {
        DEBUG_PRINT("%s: error transfer: %d\n", __func__, res);
        goto _try_send_cb_end;
    }

    res = dpool_move_file(ltb->pooldir, tmp_path);

    if (res) {
        DEBUG_PRINT("%s: error moving to pool: %d\n", __func__, res);
        goto _try_send_cb_end;
    }

    _nb_files_total++;

_try_send_cb_end:
    _ltb_upd_pub_cond(ltb);
    if (job->cb) job->cb(job, res);
}

static int _ltb_try_send(transdrv_t *drv, transfer_job_t *job)
{
    job->_drv_priv = (ltb_t *)drv;

    DDBG("disp\n");

    int res = _ltb_dispatch(_ltb_try_send_disp, job);
    if (res) {
        DERR("disp err: %d\n", res);
    }

    return res;
}

static void _ltb_delete(transdrv_t **drv)
{
    ltb_t **ltbpp = (ltb_t **)drv;
    ltb_t *ltbp = *ltbpp;

    _ltb_dispatch_sync((dispatch_sync_cb_t)_remove_ltb, ltbp);

    free(ltbp->pooldir);
    free(ltbp);
    *ltbpp = NULL;
}

static transdrv_itf_t const ltb_impl = {
    .trysend = _ltb_try_send,
    .delete  = _ltb_delete
};

#endif /* CONDALF_USE_LTB == 1 */
