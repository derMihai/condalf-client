/*
 * Copyright (C) 2021 Mihai Renea <mihai.renea@fu-berlin.de>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

#include "vstorage.h"
#include <errno.h>
#include <malloc.h>
#include <string.h>
#include <vfs.h>
#include <stdio.h>
#include <fcntl.h>

#define DLOG_LEVEL DLOG_ERR
#include "dlog.h"

#if DLOG_LEVEL >= DLOG_DBG
#include <assert.h>
#define _check_inv(_fp) __check_inv(_fp)
#else
#define _check_inv(_fp)
#endif

typedef struct vstor_privdata {
    char *buf;
    uint32_t bufsiz;
    uint32_t fend;
    int32_t flags;
} vstor_privdata_t;

static const vfs_file_ops_t vstor_impl;

#if DLOG_LEVEL >= DLOG_DBG
static void __check_inv(vfs_file_t *filp)
{
    assert(filp);
    vstor_privdata_t *privdata = (vstor_privdata_t *)filp->private_data.ptr;
    assert(privdata->fend >= (uint32_t)filp->pos);
    assert(privdata->fend <= privdata->bufsiz);
}
#endif

int vstorfile_open(vstorfile_init_t *init)
{
    if (!init || !init->buf || init->bufsiz == 0) return -EINVAL;

    vstor_privdata_t *privdata = calloc(1, sizeof(*privdata));
    if (!privdata) return -ENOMEM;

    privdata->flags     = init->flags;
    privdata->bufsiz    = init->bufsiz;
    privdata->buf       = init->buf;
    if (init->flags & VSTORF_BUF_HAS_DATA) privdata->fend = init->bufsiz;

    int fd = vfs_bind(VFS_ANY_FD, O_RDWR, &vstor_impl, privdata);
    if (fd < 0) free(privdata);

    return fd;
}

static int _close(vfs_file_t *filp)
{
    _check_inv(filp);

    vstor_privdata_t *privdata = (vstor_privdata_t *)filp->private_data.ptr;

    if (privdata->flags & VSTORF_OWNS_BUF) free(privdata->buf);
    free(privdata);

    return 0;
}

static off_t _lseek (vfs_file_t *filp, off_t off, int whence)
{
    _check_inv(filp);

    vstor_privdata_t *privdata = (vstor_privdata_t *)filp->private_data.ptr;

    switch (whence) {
    case SEEK_SET:
        break;
    case SEEK_CUR:
        off += filp->pos;
        break;
    case SEEK_END:
        off += privdata->fend;
        break;
    default:
        return -EINVAL;
    }

    if (off < 0) return -EINVAL;
    if ((uint32_t)off > privdata->bufsiz) return -ENOSPC;
    if ((uint32_t)off > privdata->fend) privdata->fend = off;
    filp->pos = off;

    _check_inv(filp);

    return 0;
}

static ssize_t _read(vfs_file_t *filp, void *dest, size_t nbytes)
{
    _check_inv(filp);

    vstor_privdata_t *privdata = (vstor_privdata_t *)filp->private_data.ptr;

    if (!dest) return -EINVAL;

    uint32_t left = privdata->fend - filp->pos;
    size_t read = nbytes > left ? left : nbytes;

    memcpy(dest, privdata->buf + filp->pos, read);

    filp->pos += read;

    _check_inv(filp);

    return read;
}

static ssize_t _write(vfs_file_t *filp, const void *src, size_t nbytes)
{
    _check_inv(filp);

    vstor_privdata_t *privdata = (vstor_privdata_t *)filp->private_data.ptr;

    if (!src) return -EINVAL;

    uint32_t left = privdata->bufsiz - filp->pos;
    int written = nbytes > left ? left : nbytes;

    memcpy(privdata->buf + filp->pos, src, written);

    filp->pos += written;
    if ((uint32_t)filp->pos > privdata->fend) privdata->fend = filp->pos;

    _check_inv(filp);

    return written;
}

static const vfs_file_ops_t vstor_impl = {
    .close = _close,
    .lseek = _lseek,
    .read  = _read,
    .write = _write
};
