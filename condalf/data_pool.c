/*
 * Copyright (C) 2021 Mihai Renea <mihai.renea@fu-berlin.de>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

#if CONDALF_USE_LTB == 1

#include "data_pool.h"
#include "vfs.h"
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include "malloc.h"

#define DLOG_LEVEL DLOG_ERR
#include "dlog.h"

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

static void _abs_fname(
    char const *fname,
    char const *pooldir,
    char *buf)
{
    strcpy(buf, pooldir);
    strcat(buf, "/");
    strcat(buf, fname);
}

static int _find_file(
    char const *poolpath,
    uint32_t *fidp,
    bool (*cmpf)(uint32_t, uint32_t))
{
    vfs_DIR pool_dir = { 0 };
    vfs_dirent_t dirent = { 0 };

    int res = vfs_opendir(&pool_dir, poolpath);
    if (res) return res;


    bool found = false;
    char *endptr;

    while ((res = vfs_readdir(&pool_dir, &dirent)) == 1) {
        char const * const fname = (*dirent.d_name == '/') ? dirent.d_name + 1 : dirent.d_name;

        uint32_t fid = strtoul(fname, &endptr, 16);

        if (*endptr != '\0') continue; // illegal file name

        DDBG("found %s, %u\n", fname, fid);

        if (cmpf(fid, *fidp)) {
            DDBG("%s best so far\n", fname);

            found = true;
            *fidp = fid;
        }
    }

    vfs_closedir(&pool_dir);

    if (!res && !found) res = -ENOENT;
    return res;
}

static bool _cmpf_older(uint32_t a, uint32_t b) { return a <= b; }
static bool _cmpf_newer(uint32_t a, uint32_t b) { return a >= b; }

static int _find_oldest(
    char const *pooldir,
    uint32_t *fid)
{
    *fid = 0xffffFFFF;
    return _find_file(pooldir, fid, _cmpf_older);
}

static int _find_newest(
    char const *pooldir,
    uint32_t *fid)
{
    *fid = 0;
    return _find_file(pooldir, fid, _cmpf_newer);
}

int dpool_drain(char const *pooldir)
{
    if (!pooldir) return -EINVAL;

    vfs_DIR dir = { 0 };
    vfs_dirent_t dirent = { 0 };

    int res = vfs_opendir(&dir, pooldir);

    if (res) return res;

    while ((res = vfs_readdir(&dir, &dirent)) == 1) {
        char *endptr;
        char const * const fname = (*dirent.d_name == '/') ? dirent.d_name + 1 : dirent.d_name;
        strtoul(fname, &endptr, 16);
        if (*endptr != '\0') continue; // illegal file name

        char abs_fname[strlen(pooldir) + 1 + strlen(fname) + 1];
        _abs_fname(fname, pooldir, abs_fname);

        res = vfs_unlink(abs_fname);
        if (res) break;
    }

    vfs_closedir(&dir);
    return res;
}

int dpool_move_file(char const *pooldir, char const *name)
{
    if (!pooldir || !name) return -EINVAL;

    char abs_fname[strlen(pooldir) + 1 + POOL_FNAME_MAX + 1];

    uint32_t newest;
    int res = _find_newest(pooldir, &newest);
    if (res && res != -ENOENT) return res;

    DDBG("newest: %0"TOSTRING(POOL_FNAME_MAX)"x\n", newest);

    newest++;

    snprintf(abs_fname, sizeof(abs_fname), "%s/%0"TOSTRING(POOL_FNAME_MAX)"x",
        pooldir, newest);

    DDBG("add %s\n", abs_fname);

    res = vfs_rename(name, abs_fname);

    return res;
}

int dpool_get_oldest_file(char const *pooldir, char *namebuf, size_t buflen)
{
    if (!pooldir || !namebuf) return -EINVAL;

    uint32_t oldest;
    int res = _find_oldest(pooldir, &oldest);
    if (res) return res;

    res = snprintf(namebuf, buflen, "%s/%0"TOSTRING(POOL_FNAME_MAX)"x",
        pooldir, oldest);

    if (res < 0) return -EINVAL;
    if ((unsigned)res >= buflen) return -ENOSPC;

    return 0;
}

int dpool_size(char const *pooldir)
{
    if (!pooldir) return -EINVAL;

    vfs_DIR dir = { 0 };
    vfs_dirent_t dirent = { 0 };

    int res = vfs_opendir(&dir, pooldir);

    if (res) return res;

    int cnt = 0;

    while ((res = vfs_readdir(&dir, &dirent)) == 1) {
        char *endptr;
        char const * const fname = (*dirent.d_name == '/') ? dirent.d_name + 1 : dirent.d_name;
        strtoul(fname, &endptr, 16);
        if (*endptr != '\0') continue; // illegal file name

        cnt++;
    }

    vfs_closedir(&dir);

    if (res) return res;
    return cnt;
}

#if DLOG_LEVEL >= DLOG_DBG
void dpool_print(char const *pooldir)
{
    if (!pooldir) return;

    vfs_DIR dir = { 0 };
    vfs_dirent_t dirent = { 0 };

    int res = vfs_opendir(&dir, pooldir);

    if (res) {
        DERR("cannot open %s: %d\n", pooldir, res);
        return;
    }

    DDBG("=== pool %s start ===\n", pooldir);

    while ((res = vfs_readdir(&dir, &dirent)) == 1) {
        char const * const fname = (*dirent.d_name == '/') ? dirent.d_name + 1 : dirent.d_name;
        char abs_fname[strlen(pooldir) + 1 + strlen(fname) + 1];
        _abs_fname(fname, pooldir, abs_fname);

        DDBG("\t%s\n", abs_fname);
    }

    DDBG("=== pool %s end: %d ===\n", pooldir, res);

    vfs_closedir(&dir);
}
#else
void dpool_print(char const *pooldir) { (void)pooldir; }
#endif

#endif /* CONDALF_USE_LTB */
