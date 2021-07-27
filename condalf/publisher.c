/*
 * Copyright (C) 2021 Mihai Renea <mihai.renea@fu-berlin.de>
 *                    Onur Demir  <onud92@zedat.fu-berlin.de>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

#if CONDALF_USE_PUBLISHER == 1

#include "publisher.h"
#include "condalf_config.h"
#include "malloc.h"
#include "thread.h"
#include "cond.h"
#include "networking.h"
#include <errno.h>

#define DLOG_LEVEL DLOG_INF
#include "dlog.h"

typedef struct {
    transdrv_t driv;
    rem_res_t rem_res;
    uint32_t nb_jobs_snd; /**< # sending jobs */
    cond_t close_cond;
    mutex_t lock;
    unsigned retry_cnt;
} publ_t;

static transdrv_itf_t const sender_impl;

static kernel_pid_t _sender_pid = KERNEL_PID_UNDEF;
#define PUBLISHER_QUEUE_MSGQUEUE_LEN 4

static void _pub_exec_snd_job(transfer_job_t *job)
{
    if (!job) return;
    publ_t *snd = (publ_t *)job->_drv_priv;

    int res;
    unsigned retry = snd->retry_cnt;

    do {
        res = net_send(&snd->rem_res, job->fd);
        if (res < 0 && retry) { DWRN("failed: %d, retrying...\n", res) };
    } while (res < 0 && retry--);

    if (res < 0) { DERR("failed: %d\n", res) };

    if (job->cb) job->cb(job, res > 0 ? 0 : res);
}

static void *_pub_thread(void *arg)
{
    static msg_t msg_queue[PUBLISHER_QUEUE_MSGQUEUE_LEN];
    msg_t msg;
    msg_init_queue(msg_queue, PUBLISHER_QUEUE_MSGQUEUE_LEN);

    while (1) {
        msg_receive(&msg);
        publ_t *snd = (publ_t *)((transfer_job_t *)msg.content.ptr)->_drv_priv;

        _pub_exec_snd_job((transfer_job_t *)msg.content.ptr);

        mutex_lock(&snd->lock);
        if (--snd->nb_jobs_snd == 0) cond_signal(&snd->close_cond);
        mutex_unlock(&snd->lock);
    }

    return NULL;
}

static int _pub_init_subsys(void)
{
    static char sender_stack[THREAD_STACKSIZE_MAIN];

    _sender_pid = thread_create(
        sender_stack,
        sizeof(sender_stack),
        PUBLISHER_QUEUE_PRIO,
        0,
        _pub_thread,
        NULL,
        "sender");

    if (_sender_pid < 0) {
        return _sender_pid;
    }

    return 0;
}

int publisher_init(transdrv_t **drvpp, rem_res_t const *rem_res, unsigned retry_cnt)
{
    int res;

    if (_sender_pid == KERNEL_PID_UNDEF) {
        res = _pub_init_subsys();
        if (res) return res;
    }

    publ_t *snd = calloc(1, sizeof(*snd));
    if (!snd) return -ENOMEM;

    res = rem_res_cpy(&snd->rem_res, rem_res);
    if (res) goto sender_init_err;

    snd->driv.itf = &sender_impl;
    snd->retry_cnt = retry_cnt;

    mutex_init(&snd->lock);
    cond_init(&snd->close_cond);

    *drvpp = (transdrv_t *)snd;

    return 0;

sender_init_err:
    if (snd) {
        rem_res_freedata(&snd->rem_res);
        free(snd);
    }

    return res;
}

static int _pub_try_send(transdrv_t *drv, transfer_job_t *job)
{
    publ_t *snd = (publ_t *)drv;

    job->_drv_priv = snd;

    msg_t msg = {
        .content.ptr = job
    };

    mutex_lock(&snd->lock);

    snd->nb_jobs_snd++;

    int res = msg_try_send(&msg, _sender_pid);

    if (res != 1) {
        if (--snd->nb_jobs_snd == 0) cond_signal(&snd->close_cond);

        mutex_unlock(&snd->lock);

        if (res == 0) {
            DERR("sender queue full!\n")
            return -EWOULDBLOCK;
        } else {
            return -ESRCH;
        }
    }

    mutex_unlock(&snd->lock);

    return 0;
}

static int _pub_send(transdrv_t *drv, transfer_job_t *job)
{
    publ_t *snd = (publ_t *)drv;

    int res;
    unsigned retry = snd->retry_cnt;

    do {
        res = net_send(&snd->rem_res, job->fd);
        if (res < 0 && retry) { DWRN("failed: %d, retrying...\n", res) };
    } while (res < 0 && retry--);

    if (res < 0) { DERR("failed: %d\n", res) };

    if (res >= 0 && job->cb) job->cb(job, res);

    return res > 0 ? 0 : res;
}

static void _pub_delete(transdrv_t **drv)
{
    publ_t **sndpp = (publ_t **)drv;
    publ_t *sndp = *sndpp;

    /* Wait for the enqueued jobs to finish... */
    mutex_lock(&sndp->lock);
    while (sndp->nb_jobs_snd) cond_wait(&sndp->close_cond, &sndp->lock);
    mutex_unlock(&sndp->lock);

    rem_res_freedata(&sndp->rem_res);
    free(sndp);
    *sndpp = NULL;
}

static transdrv_itf_t const sender_impl = {
    .trysend = _pub_try_send,
    .send    = _pub_send,
    .delete  = _pub_delete
};

#endif /* CONDALF_USE_PUBLISHER == 1 */
