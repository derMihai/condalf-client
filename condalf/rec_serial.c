/*
 * Copyright (C) 2021 Mihai Renea <mihai.renea@fu-berlin.de>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

#include "rec_serial.h"
#include "malloc.h"
#include <errno.h>
#include <sys/types.h>

#define DLOG_LEVEL DLOG_INF
#include "dlog.h"

#if DLOG_LEVEL >= DLOG_DBG
#include <assert.h>
#define _assert(expr) assert(expr)
#define _check_inv(_rs) __check_inv(_rs)
#else
#define _check_inv(_rs)
#define _assert(expr) (void)(expr)
#endif /* DEVHELP */

#define ARRAY_MAX_BYTES 4 /**< maximum number of bytes to describe an array */

static void peekcb_init(peekcb_t *pcb, record_t *a, size_t len)
{
    pcb->ri     = 0;
    pcb->wi     = 0;
    pcb->a      = a;
    pcb->len    = len;
}

static size_t peekcb_fill(peekcb_t *pcb)
{
    return pcb->wi - pcb->ri;
}

static size_t peekcb_put(peekcb_t *pcb, record_t const *a, size_t len)
{
    size_t wi = pcb->wi;
    size_t const msk = pcb->len - 1;
    size_t const empty = pcb->len - (wi - pcb->ri);
    size_t const to_write = len > empty ? empty : len;

    for (size_t i = 0; i < to_write; i++) {
        pcb->a[wi++ & msk] = *a++;
    }

    pcb->wi = wi;

    return to_write;
}

static size_t peekcb_get(peekcb_t *pcb, record_t *a, size_t len)
{
    size_t ri = pcb->ri;
    size_t const msk = pcb->len - 1;
    size_t const fill = pcb->wi - ri;
    size_t const to_read = len > fill ? fill : len;

    for (size_t i = 0; i < to_read; i++) {
        /* Only truncate on array access */
        *a++ = pcb->a[ri++ & msk];
    }

    pcb->ri = ri;

    return to_read;
}

static int peekcb_next(peekcb_t *pcb, record_t *r, size_t *itp)
{
    size_t const msk = pcb->len - 1;
    size_t it = *itp + 1;


    ssize_t diff = (ssize_t)(pcb->wi - it);
    if (diff == 0) return -ENODATA;
    if (diff < 0 || (size_t)diff > peekcb_fill(pcb)) return -EINVAL;

    *r = pcb->a[it & msk];
    *itp = it;

    return 0;
}

static int peekcb_peek(peekcb_t *pcb, record_t *r, size_t *itp)
{
    if (peekcb_fill(pcb) == 0) return -ENODATA;

    size_t const msk = pcb->len - 1;
    *r = pcb->a[pcb->ri & msk];

    if (itp) *itp = pcb->ri;
    return 0;
}

#if DLOG_LEVEL >= DLOG_DBG
static inline void __check_inv(recser_t *rs)
{
    _assert(rs);
    _assert(rs->fit_cnt <= peekcb_fill(&rs->cb));
}
#endif

int recser_init(recser_t *rs, recser_init_t const *init)
{
    if (!rs || !init || !init->buf.ptr)  return -EINVAL;
    if (init->len_limit == 0)            return -EINVAL;
    if (init->buf.len < ARRAY_MAX_BYTES) return -ENOSPC;

    size_t len = init->len_limit;
    while (!(len & 0x1)) len >>= 1;
    if (len != 1) return -EINVAL;

    memset(rs, 0, sizeof(*rs));

    if (init->base) {
        if (record_base_copy(&rs->base, init->base)) return -ENOMEM;
    } else {
        DDBG("no base\n");
    }

    record_t *const a = malloc(sizeof(*a) * init->len_limit);
    if (!a) {
        record_base_freedata(&rs->base);
        return -ENOMEM;
    }

    rs->buf = init->buf;
    rs->fit_cnt = 0;
    peekcb_init(&rs->cb, a, init->len_limit);
    /* Init encoder in simulation mode.
     * Even if n records fit in the buffer, closing the array will require up
     * to ARRAY_MAX_BYTES extra bytes, so we subtract that from the buffer
     * length. */
    senml_enc_init(&rs->enc, NULL, rs->buf.len - ARRAY_MAX_BYTES, &rs->base);

    _check_inv(rs);

    return 0;
}

int recser_put(recser_t *rs, record_t *rec)
{
    if (!rs || !rec) return -EINVAL;
    if (!rs->buf.ptr) {
        DERR("invalid instance!\n");
        return -EINVAL;
    }

    _check_inv(rs);

    record_t nrec;
    record_move(&nrec, rec);

    if (peekcb_fill(&rs->cb) == rs->cb.len) {
        record_move(rec, &nrec);
        return -ENOSPC;
    }

    int ret = senml_enc_put(&rs->enc, &nrec);
    if (ret == -ENOSPC) {
        if (rs->fit_cnt == 0) {
            /* Buffer cannot fit even one record */
            record_move(rec, &nrec);
            return -ENOBUFS;
        }

        _assert(peekcb_put(&rs->cb, &nrec, 1) == 1);
        return -EAGAIN;
    }

    if (ret) {
        DERR("enc_put failed: %d!\n", ret);
        record_move(rec, &nrec);
        return -EINVAL;
    }

    _assert(peekcb_put(&rs->cb, &nrec, 1) == 1);
    rs->fit_cnt++;

    _check_inv(rs);

    return 0;
}

static ssize_t _recser_flush_simulate(recser_t *rs, size_t cnt)
{
    if (cnt == 0) return 0;

    record_t rec = { 0 };
    int flushed = 0;
    size_t it;

    int res = peekcb_peek(&rs->cb, &rec, &it);
    if (res == -ENODATA) return flushed;

    do {
        res = senml_enc_put(&rs->enc, &rec);
        if (res == -ENOSPC) break;
        if (res) return res;

        flushed++;

        res = peekcb_next(&rs->cb, &rec, &it);
        if (res == -ENODATA) break;

    } while (--cnt);

    return flushed;
}

static ssize_t _recser_flush(recser_t *rs, size_t cnt)
{
    record_t rec = { 0 };
    int flushed = 0;

    while (cnt--) {
        int res = peekcb_get(&rs->cb, &rec, 1);

        _assert(res == 1);

        res = senml_enc_put(&rs->enc, &rec);
        if (rec.type == RECORDTYPE_STRING) free(rec.str);
        if (res == -ENOSPC) break;
        if (res) return res;

        flushed++;
    }

    return flushed;
}

int recser_swap(recser_t *rs, UsefulBuf *out)
{
    if (!rs || !out) return -EINVAL;
    if (!rs->buf.ptr) {
        DERR("invalid instance!\n");
        return -EINVAL;
    }

    DDBG("out->ptr=0x%X\n", (unsigned)out->ptr);

    _check_inv(rs);

    size_t enc_len;

    if (rs->fit_cnt > 0) {
        senml_enc_init(&rs->enc, rs->buf.ptr, rs->buf.len, &rs->base);
        int const fit_cnt = rs->fit_cnt;
        _assert(_recser_flush(rs, fit_cnt) == fit_cnt);
        rs->fit_cnt = 0;
        _assert(senml_enc_close(&rs->enc, &enc_len) == 0);

    } else {
        enc_len = 0;
    }

    UsefulBuf tmp = rs->buf;
    rs->buf = *out;
    tmp.len = enc_len;
    *out = tmp;

    /* Input buffer NULL invalidates the serializer */
    if (rs->buf.ptr == NULL) {
        /* flush remaining records */
        DDBG("invalidating...\n");
        senml_enc_init(&rs->enc, NULL, 0xFFFF, &rs->base);
        _recser_flush(rs, peekcb_fill(&rs->cb));
        rs->fit_cnt = 0;

        _assert(rs->fit_cnt == 0);
        _assert(peekcb_fill(&rs->cb) == 0);

        _check_inv(rs);

        free(rs->cb.a);
        record_base_freedata(&rs->base);

        return 0;
    }

    _assert(rs->fit_cnt == 0);
    /* We prepare the encoder for the next buffer */
    senml_enc_init(&rs->enc, NULL, rs->buf.len - ARRAY_MAX_BYTES, &rs->base);

    _check_inv(rs);

    if (peekcb_fill(&rs->cb)) {
        /* simulate how many of the remainig records fit in the new buffer */
        int res = _recser_flush_simulate(rs, peekcb_fill(&rs->cb));
        _assert(res >= 0);

        rs->fit_cnt = res;

        _check_inv(rs);

        return -EAGAIN;
    }

    return 0;
}
