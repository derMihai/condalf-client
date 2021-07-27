/*
 * Copyright (C) 2021 Mihai Renea <mihai.renea@fu-berlin.de>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

#include <stdio.h>
#include <vfs.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <malloc.h>
#include <mutex.h>

static const vfs_file_ops_t hexout_impl;

int hexout_open(char const *name)
{
    char *namecpy = NULL;
    if (name) {
        namecpy = strdup(name);
        if (!namecpy) return -ENOMEM;
    }

    int fd = vfs_bind(VFS_ANY_FD, O_WRONLY, &hexout_impl, namecpy);
    if (fd < 0) {
        free(namecpy);
        return fd;
    }

    printf("\n======== %s begin ========\n",
        namecpy ? namecpy : "Hexout");

    return fd;
}

static int _close(vfs_file_t *filp)
{
    char *name = (char *)filp->private_data.ptr;
    printf("\n======== %s end ========== \n",
        name ? name : "Hexout");

    free(name);
    return 0;
}

static ssize_t _write(vfs_file_t *filp, const void *src, size_t nbytes)
{
    char const *buf = (char const *)src;

    for (size_t i = 0; i < nbytes;) {
        printf("0x%02X, ", buf[i]);
        if (!(++i & 0xF)) printf("\n");
    }

    return nbytes;
}

static const vfs_file_ops_t hexout_impl = {
    .close = _close,
    .write = _write
};
