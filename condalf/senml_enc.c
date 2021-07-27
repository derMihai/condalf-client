/*
 * Copyright (C) 2021 Mihai Renea <mihai.renea@fu-berlin.de>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

#include "senml_enc.h"
#include "malloc.h"
#include <errno.h>
#include <timex.h>

#define DLOG_LEVEL DLOG_ERR
#include "dlog.h"

enum {
    SENMLKEY_bs   = -6,
    SENMLKEY_bv   = -5,
    SENMLKEY_bu   = -4,
    SENMLKEY_bt   = -3,
    SENMLKEY_bn   = -2,
    SENMLKEY_bver = -1,
    SENMLKEY_n    =  0,
    SENMLKEY_u    =  1,
    SENMLKEY_v    =  2,
    SENMLKEY_vs   =  3,
    SENMLKEY_vb   =  4,
    SENMLKEY_s    =  5,
    SENMLKEY_t    =  6,
    SENMLKEY_ut   =  7,
    SENMLKEY_vd   =  8
};

static char const *const senml_units[RECORDUNIT_ENUMSIZE] = {
    [RECORDUNIT_NONE] =                   NULL,
    [RECORDUNIT_m] =                      "m",
    [RECORDUNIT_kg] =                     "kg",
    [RECORDUNIT_g] =                      "g",
    [RECORDUNIT_s] =                      "s",
    [RECORDUNIT_A] =                      "A",
    [RECORDUNIT_K] =                      "K",
    [RECORDUNIT_cd] =                     "cd",
    [RECORDUNIT_mol] =                    "mol",
    [RECORDUNIT_Hz] =                     "Hz",
    [RECORDUNIT_rad] =                    "rad",
    [RECORDUNIT_sr] =                     "sr",
    [RECORDUNIT_N] =                      "N",
    [RECORDUNIT_Pa] =                     "Pa",
    [RECORDUNIT_J] =                      "J",
    [RECORDUNIT_W] =                      "W",
    [RECORDUNIT_C] =                      "C",
    [RECORDUNIT_V] =                      "V",
    [RECORDUNIT_F] =                      "F",
    [RECORDUNIT_Ohm] =                    "Ohm",
    [RECORDUNIT_S] =                      "S",
    [RECORDUNIT_Wb] =                     "Wb",
    [RECORDUNIT_T] =                      "T",
    [RECORDUNIT_H] =                      "H",
    [RECORDUNIT_Cel] =                    "Cel",
    [RECORDUNIT_lm] =                     "lm",
    [RECORDUNIT_lx] =                     "lx",
    [RECORDUNIT_Bq] =                     "Bq",
    [RECORDUNIT_Gy] =                     "Gy",
    [RECORDUNIT_Sv] =                     "Sv",
    [RECORDUNIT_kat] =                    "kat",
    [RECORDUNIT_m2] =                     "m2",
    [RECORDUNIT_m3] =                     "m3",
    [RECORDUNIT_l] =                      "l",
    [RECORDUNIT_m_per_s] =                "m/s",
    [RECORDUNIT_m_per_s2] =               "m/s2",
    [RECORDUNIT_m3_per_s] =               "m3/s",
    [RECORDUNIT_l_per_s] =                "l/s",
    [RECORDUNIT_W_per_m2] =               "W/m2",
    [RECORDUNIT_cd_per_m2] =              "cd/m2",
    [RECORDUNIT_bit] =                    "bit",
    [RECORDUNIT_bit_per_s] =              "bit/s",
    [RECORDUNIT_lat] =                    "lat",
    [RECORDUNIT_lon] =                    "lon",
    [RECORDUNIT_pH] =                     "pH",
    [RECORDUNIT_dB] =                     "dB",
    [RECORDUNIT_dBW] =                    "dBW",
    [RECORDUNIT_Bspl] =                   "Bspl",
    [RECORDUNIT_count] =                  "count",
    [RECORDUNIT_ratio] =                  "/",
    [RECORDUNIT_percent] =                "%",
    [RECORDUNIT_percent_RH] =             "%RH",
    [RECORDUNIT_percent_EL] =             "%EL",
    [RECORDUNIT_EL] =                     "EL",
    [RECORDUNIT_1_per_s] =                "1/s",
    [RECORDUNIT_1_per_min] =              "1/min",
    [RECORDUNIT_beat_per_min] =           "beat/min",
    [RECORDUNIT_beats] =                  "beats",
    [RECORDUNIT_S_per_m] =                "S/m"
};

int senml_enc_init(senml_enc_t *enc, char *buf, size_t size, record_base_t const *base)
{
    if (!enc) return -EINVAL;

    memset(enc, 0, sizeof(*enc));

    enc->buf.ptr = buf;
    enc->buf.len = size;

    QCBOREncodeContext *const qenc = &enc->cbor_ctx;

    QCBOREncode_Init(qenc, enc->buf);
    QCBOREncode_OpenArray(qenc);

    if (base && base->name) {
        DDBG("base name: %s\n", base->name);

        QCBOREncode_OpenMap(qenc);

        UsefulBufC const _bname = {
            .ptr = base->name,
            .len = strlen(base->name)
        };
        QCBOREncode_AddTextToMapN(qenc, SENMLKEY_bn, _bname);

        QCBOREncode_CloseMap(qenc);

        switch (QCBOREncode_GetErrorState(qenc)) {
        case QCBOR_SUCCESS:
            break;

        case QCBOR_ERR_BUFFER_TOO_SMALL:
            return -ENOSPC;

        default:
            DERR("qbenc fail: %d!\n", QCBOREncode_GetErrorState(qenc));
            return -EINVAL;
        }
    } else {
        DDBG("no base / base name\n");
    }

    return 0;
}

int senml_enc_put(senml_enc_t *enc, record_t const *rec)
{
    if (!enc || !rec) {
        DERR("invalid arguments!\n");
        return -EINVAL;
    }

    QCBOREncodeContext *const qenc = &enc->cbor_ctx;
    QCBOREncode_OpenMap(qenc);

    UsefulBufC const name = {.ptr = rec->name, .len = strlen(rec->name)};
    QCBOREncode_AddTextToMapN(qenc, SENMLKEY_n, name);

    double const ts = timex_uint64(rec->timestamp) / (double)US_PER_SEC;
    QCBOREncode_AddDoubleToMapN(qenc, SENMLKEY_t, ts);

    if (rec->unit != RECORDUNIT_NONE) {
        if (rec->unit >= RECORDUNIT_ENUMSIZE) {
            DERR("unit invalid: %u\n", rec->unit);
            return -EINVAL;
        }

        UsefulBufC const unit = {
            .ptr = senml_units[rec->unit],
            .len = strlen(senml_units[rec->unit])
        };

        QCBOREncode_AddTextToMapN(qenc, SENMLKEY_u, unit);
    }

    switch (rec->type) {
    case RECORDTYPE_EMPTY:
    default:
        DERR("rectype invalid: %u!\n", rec->type);
        return -EINVAL;

    case RECORDTYPE_U32:
        QCBOREncode_AddUInt64ToMapN(qenc, SENMLKEY_v, rec->u32);
        break;

    case RECORDTYPE_I32:
        QCBOREncode_AddInt64ToMapN(qenc, SENMLKEY_v, rec->i32);
        break;

    case RECORDTYPE_STRING:
    {
        UsefulBufC const val = {.ptr = rec->str, .len = strlen(rec->str)};
        QCBOREncode_AddTextToMapN(qenc, SENMLKEY_v, val);
    }
    }

    QCBOREncode_CloseMap(qenc);

    switch (QCBOREncode_GetErrorState(qenc)) {
    case QCBOR_SUCCESS:
        return 0;

    case QCBOR_ERR_BUFFER_TOO_SMALL:
        return -ENOSPC;

    default:
        DERR("qbenc fail: %d!\n", QCBOREncode_GetErrorState(qenc));
        return -EINVAL;
    }
}

int senml_enc_close(senml_enc_t *enc, size_t *enc_len)
{
    if (!enc) return -EINVAL;

    QCBOREncodeContext *qenc = &enc->cbor_ctx;
    UsefulBufC outb;
    int retval = 0;

    QCBOREncode_CloseArray(qenc);
    switch (QCBOREncode_Finish(qenc, &outb)) {
    case QCBOR_SUCCESS:
        break;

    case QCBOR_ERR_BUFFER_TOO_SMALL:
        retval = -ENOSPC;
        break;

    default:
        return -EINVAL;
    }

    if (enc_len) *enc_len = outb.len;
    return retval;
}
