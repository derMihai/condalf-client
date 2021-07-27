/*
 * Copyright (C) 2021 Mihai Renea <mihai.renea@fu-berlin.de>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @file
 * @brief ConDaLF record, the basic logging data type.
 */

#ifndef INC_RECORD_H_
#define INC_RECORD_H_

#include "timex.h"
#include "malloc.h"
#include <stdint.h>
#include <string.h>
#include <errno.h>

/**
 * Record type.
 */
enum {
    RECORDTYPE_EMPTY,  /**< RECORDTYPE_EMPTY */
    RECORDTYPE_U32,    /**< RECORDTYPE_U32 */
    RECORDTYPE_I32,    /**< RECORDTYPE_I32 */
    RECORDTYPE_STRING, /**< RECORDTYPE_STRING */

    RECORDTYPE_ENUMSIZE/**< RECORDTYPE_ENUMSIZE */
};

/**
 * Record unit. Taken from SenML specification.
 */
enum {
    RECORDUNIT_NONE,        /**< RECORDUNIT_NONE */
    RECORDUNIT_m,           /**< RECORDUNIT_m */
    RECORDUNIT_kg,          /**< RECORDUNIT_kg */
    RECORDUNIT_g,           /**< RECORDUNIT_g */
    RECORDUNIT_s,           /**< RECORDUNIT_s */
    RECORDUNIT_A,           /**< RECORDUNIT_A */
    RECORDUNIT_K,           /**< RECORDUNIT_K */
    RECORDUNIT_cd,          /**< RECORDUNIT_cd */
    RECORDUNIT_mol,         /**< RECORDUNIT_mol */
    RECORDUNIT_Hz,          /**< RECORDUNIT_Hz */
    RECORDUNIT_rad,         /**< RECORDUNIT_rad */
    RECORDUNIT_sr,          /**< RECORDUNIT_sr */
    RECORDUNIT_N,           /**< RECORDUNIT_N */
    RECORDUNIT_Pa,          /**< RECORDUNIT_Pa */
    RECORDUNIT_J,           /**< RECORDUNIT_J */
    RECORDUNIT_W,           /**< RECORDUNIT_W */
    RECORDUNIT_C,           /**< RECORDUNIT_C */
    RECORDUNIT_V,           /**< RECORDUNIT_V */
    RECORDUNIT_F,           /**< RECORDUNIT_F */
    RECORDUNIT_Ohm,         /**< RECORDUNIT_Ohm */
    RECORDUNIT_S,           /**< RECORDUNIT_S */
    RECORDUNIT_Wb,          /**< RECORDUNIT_Wb */
    RECORDUNIT_T,           /**< RECORDUNIT_T */
    RECORDUNIT_H,           /**< RECORDUNIT_H */
    RECORDUNIT_Cel,         /**< RECORDUNIT_Cel */
    RECORDUNIT_lm,          /**< RECORDUNIT_lm */
    RECORDUNIT_lx,          /**< RECORDUNIT_lx */
    RECORDUNIT_Bq,          /**< RECORDUNIT_Bq */
    RECORDUNIT_Gy,          /**< RECORDUNIT_Gy */
    RECORDUNIT_Sv,          /**< RECORDUNIT_Sv */
    RECORDUNIT_kat,         /**< RECORDUNIT_kat */
    RECORDUNIT_m2,          /**< RECORDUNIT_m2 */
    RECORDUNIT_m3,          /**< RECORDUNIT_m3 */
    RECORDUNIT_l,           /**< RECORDUNIT_l */
    RECORDUNIT_m_per_s,     /**< RECORDUNIT_m_per_s */
    RECORDUNIT_m_per_s2,    /**< RECORDUNIT_m_per_s2 */
    RECORDUNIT_m3_per_s,    /**< RECORDUNIT_m3_per_s */
    RECORDUNIT_l_per_s,     /**< RECORDUNIT_l_per_s */
    RECORDUNIT_W_per_m2,    /**< RECORDUNIT_W_per_m2 */
    RECORDUNIT_cd_per_m2,   /**< RECORDUNIT_cd_per_m2 */
    RECORDUNIT_bit,         /**< RECORDUNIT_bit */
    RECORDUNIT_bit_per_s,   /**< RECORDUNIT_bit_per_s */
    RECORDUNIT_lat,         /**< RECORDUNIT_lat */
    RECORDUNIT_lon,         /**< RECORDUNIT_lon */
    RECORDUNIT_pH,          /**< RECORDUNIT_pH */
    RECORDUNIT_dB,          /**< RECORDUNIT_dB */
    RECORDUNIT_dBW,         /**< RECORDUNIT_dBW */
    RECORDUNIT_Bspl,        /**< RECORDUNIT_Bspl */
    RECORDUNIT_count,       /**< RECORDUNIT_count */
    RECORDUNIT_ratio,       /**< RECORDUNIT_ratio */
    RECORDUNIT_percent,     /**< RECORDUNIT_percent */
    RECORDUNIT_percent_RH,  /**< RECORDUNIT_percent_RH */
    RECORDUNIT_percent_EL,  /**< RECORDUNIT_percent_EL */
    RECORDUNIT_EL,          /**< RECORDUNIT_EL */
    RECORDUNIT_1_per_s,     /**< RECORDUNIT_1_per_s */
    RECORDUNIT_1_per_min,   /**< RECORDUNIT_1_per_min */
    RECORDUNIT_beat_per_min,/**< RECORDUNIT_beat_per_min */
    RECORDUNIT_beats,       /**< RECORDUNIT_beats */
    RECORDUNIT_S_per_m,     /**< RECORDUNIT_S_per_m */

    RECORDUNIT_ENUMSIZE     /**< RECORDUNIT_ENUMSIZE */
};

typedef struct record {
    /** name is assumed to remain owned by the creator of the record, but allowed
     *  to be referenced more than once. Thus, it is the responsibility of the
     *  creator to free this string, if required. Also, the creator of the record
     *  (i.e. owner of the string) must ensure that the owner of the record's
     *  data is finished before freeing/modifying this string.
     *
     *  rationale: the name usually represents a node and/or a measurement, so
     *  it is expected to be long-lived. Conversely, records are meant to be
     *  allocated and deallocated very often.*/
    char const  *name;
    timex_t     timestamp; /**< Timestamp of the record */

    union {
        uint32_t    u32;
        int32_t     i32;
        float       f;
        /** str is owned by the record's data owner, and only one reference to
         *  the record's data is allowed (the owner itself). The creator of the
         *  record must make sure this string can be released with free(), should
         *  ownership of the record be passed further. Consequently, the (new)
         *  owner of the record must release this string with free() when the
         *  record is not used anymore. */
        char        *str;
    };

    uint8_t type; /**< Value of RECORDTYPE_* */
    uint8_t unit; /**< Value of RECORDUNIT_* */
} __attribute__((__packed__)) record_t;

typedef struct {
    /**
     * Leave NULL if not used */
    char *name;
} record_base_t;

static void record_move(record_t *to, record_t *from)
{
    *to = *from;
    if (from->type == RECORDTYPE_STRING) from->str = NULL;
}

static int record_copy(record_t *to, record_t const *from)
{
    *to = *from;

    if (from->type == RECORDTYPE_STRING) {
        to->str = strdup(from->str);
        if (!to->str) return -ENOMEM;
    }

    return 0;
}

static void record_freedata(record_t *rec)
{
    if (rec->type == RECORDTYPE_STRING) free(rec->str);
    rec->str = NULL;
}

static int record_base_copy(record_base_t *to, record_base_t const *from)
{
    memset(to, 0, sizeof(*to));
    if (from->name) {
        to->name = strdup(from->name);
        if (!to->name) return -ENOMEM;
    }

    return 0;
}

static void record_base_freedata(record_base_t *base)
{
    free(base->name);
}

#endif /* INC_RECORD_H_ */
