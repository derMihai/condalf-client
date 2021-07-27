/*
 * Copyright (C) 2021 Mihai Renea <mihai.renea@fu-berlin.de>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

#include <string.h>
#include <errno.h>
#include <vfs.h>
#include <rec_serial.h>
#include <vstorage.h>
#include <assert.h>
#include "cond.h"
#include "malloc.h"
#include "logging.h"
#include "thread.h"
#include "condalf_config.h"
#include "networking.h"

#define DLOG_LEVEL DLOG_INF
#include "dlog.h"

typedef struct logg {
    recstr_t stream;
    recser_t ser;
    int flags;
    transdrv_t *driv;
    size_t encbuf_size;
} logg_t;

static recstr_itf_t const recstr_impl;

int logg_create(logg_init_t const *init, recstr_t **log)
{
    if (!init || !log) return -EINVAL;
    if (!init->driv) return -EINVAL;

    int res = 0;
    logg_t *logger = calloc(1, sizeof(*logger));
    if (!logger) return -ENOMEM;
    char *ser_buf = NULL;

    logger->stream.itf  = &recstr_impl;
    logger->flags       = init->flags;
    logger->driv        = init->driv;
    logger->encbuf_size = init->encoding_buf_size;

    mutex_init(&logger->stream.lock);

    ser_buf = malloc(logger->encbuf_size);
    if (!ser_buf) {
        res = -ENOMEM;
        goto logg_create_fail;
    }

    record_base_t base = {
        .name = (char *)init->base_name
    };

    recser_init_t const ser_init = {
        .len_limit = init->record_queue_size,
        .buf.len   = logger->encbuf_size,
        .buf.ptr   = ser_buf,
        .base      = &base
    };

    res = recser_init(&logger->ser, &ser_init);
    if (res < 0) goto logg_create_fail;

    strncpy(
        logger->stream.name,
        init->name ? init->name : "<none>",
        RECORDSTREAM_MAX_STR_LEN);

    logger->stream.name[RECORDSTREAM_MAX_STR_LEN] = '\0';

    *log = (recstr_t *)logger;
    return 0;

logg_create_fail:
    free(ser_buf);
    free(logger);

    return res;
}

static void _logg_snd_cb(transfer_job_t *job, int err)
{
    DDBG("job finished: %d\n", err);
    vfs_close(job->fd);
    free(job);
}

static int _logg_send_buffer(logg_t *logger, UsefulBuf *ub)
{
    if (ub->len == 0) return 0;

    vstorfile_init_t vf_init = {
        .buf    = ub->ptr,
        .bufsiz = ub->len,
        .flags  = VSTORF_OWNS_BUF | VSTORF_BUF_HAS_DATA
    };

    int fd = vstorfile_open(&vf_init);
    if (fd < 0) {
        free(ub->ptr);
        ub->ptr = NULL;
        return fd;
    }

    ub->ptr = NULL;

    transfer_job_t *job = calloc(1, sizeof(*job));
    if (!job) {
        vfs_close(fd);
        return -ENOMEM;
    }

    job->cb = _logg_snd_cb;
    job->fd = fd;

    int res = transdrv_trysend(logger->driv, job);

    if (res) {
        DERR("%s: trysend failed: %d\n", logger->stream.name, res);
        vfs_close(job->fd);
        free(job);
    } else {
        DINF("%s: trysend success!\n", logger->stream.name);
    }

    return res;
}

static int _logg_flush(logg_t *logger)
{
    int res = 0;
    UsefulBuf ub;

    do {
        /* Flush any remaining records in the serializer's queue */
        ub.len = logger->encbuf_size;
        ub.ptr = malloc(logger->encbuf_size);

        if (!ub.ptr) {
            DDBG("ENOMEM\n");
            res = ENOMEM;
            break;
        }

        res = recser_swap(&logger->ser, &ub);
        if (res == -EAGAIN || res == 0) {
            DDBG("more records to flush...");
            int res2;

            // TODO: retry?
            res2 = _logg_send_buffer(logger, &ub);
            if (res2) {
                DERR("failed: %s\n", strerror(res2));
                break;
            }
            DDBG("success!\n");
        } else {
            DERR("swap failed: %d\n", res);
            free(ub.ptr);
            ub.ptr = NULL;
            break;
        }

    } while (res == -EAGAIN);

    return res;
}

static int _logg_put(recstr_t *rstr, record_t *rec)
{
    logg_t *logger = (logg_t *)rstr;

    if (!rec) return _logg_flush(logger);

    record_t nrec = { 0 };
    UsefulBuf ub = { 0 };
    int retval = 0;
    /* Make a copy first, in case of error we promise not to touch the original
     * record data */
    int res = record_copy(&nrec, rec);
    if (res) return res;

    int put_res = recser_put(&logger->ser, &nrec);

    switch (put_res) {
    case -EAGAIN:
    case -ENOSPC:
    {
        DDBG("cannot add: %s, swapping...",
            put_res == -EAGAIN ? "EAGAIN" : "ENOSPC");

        ub.len = logger->encbuf_size;
        ub.ptr = malloc(logger->encbuf_size);

        if (!ub.ptr) {
            DERR("failed: ENOMEM\n");
            retval = -ENOMEM;
            break;
        }

        res = recser_swap(&logger->ser, &ub);

        if (res && res != -EAGAIN) {
            DERR("failed: %s\n", strerror(res));
            retval = res;
            break;
        }

        DDBG("done!\n");

        DINF("sending buffer...\n");
        res = _logg_send_buffer(logger, &ub);

        if (res) {
            DERR("_send_buffer err: %d\n", res);
        } else {
            DINF("buffer sent!\n");
        }

        if (put_res == -ENOSPC) {
            /* The queue was full -> the record wasn't added, try again */
            put_res = recser_put(&logger->ser, &nrec);

            if (put_res != -EAGAIN) {
                retval = put_res;
                break;
            }
        }
        break;
    }

    default:
        retval = put_res;
    }

    free(ub.ptr);
    record_freedata(&nrec);
    /* Only release the original record data on success */
    if (!retval) record_freedata(rec);
    return retval;
}

static int _logg_close(recstr_t **rstr)
{
    logg_t *logger = (logg_t *)*rstr;
    int res = 0;
    UsefulBuf ub;

    DDBG("closing...\n");

    res = _logg_flush(logger);

    /* Invalidate the serializer */
    ub.ptr = NULL;
    recser_swap(&logger->ser, &ub);
    free(ub.ptr);

    free(logger);
    *rstr = NULL;

    return res;
}

static recstr_itf_t const recstr_impl = {
    .put    = _logg_put,
    .close  = _logg_close
};
