/*
 * Copyright (C) 2021 Mihai Renea <mihai.renea@fu-berlin.de>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @file
 * @brief ConDaLF record serializer */

#ifndef INC_REC_SERIAL_H_
#define INC_REC_SERIAL_H_

#include "record.h"
#include "UsefulBuf.h"
#include "senml_enc.h"

typedef struct peekcb {
    record_t *a;
    size_t len;
    size_t ri;
    size_t wi;
} peekcb_t;

/** Record serializer parameters */
typedef struct recser_init {
    /** Buffer for the encoding */
    UsefulBuf buf;
    /** How many records to maximally encode in a buffer. MUST be power of 2.
     *  Note that the serializer will internally allocate
     *  len_limit * sizeof(record_t) bytes. The current size of record_t is
     *  18 Bytes on 32-Bit systems. */
    size_t len_limit;
    /** Pointer to a base to be used for all the encodings. Leave NULL if not
     *  used. Copied internally, can be destroyed after \ref recser_init()
     *  returns. */
    record_base_t const *base;
} recser_init_t;

typedef struct recser {
    UsefulBuf buf;
    peekcb_t cb;
    senml_enc_t enc;
    size_t fit_cnt;
    record_base_t base;
} recser_t;

/**
 * @brief Init the record serializer.
 *
 * @param rs pointer record serializer
 * @param init pointer to init structure
 *
 * @return 0 on success, -ENOSPC if the supplied buffer is too small, negative
 *  error otherwise */
int recser_init(recser_t *rs, recser_init_t const *init);
/**
 * @brief Add a record to be serialized.
 *
 * @param rs pointer to the record serializer
 * @param rec record to be appended
 *
 * @return
 *   0 on success
 *  -EAGAIN if the output buffer needs to be swapped. The record was
 *   nevertheless acknowledged and ownership was taken over its data.
 *  -ENOSPC if the record queue is full, and the serialized buffer must be
 *    swapped.
 *  -ENOBUFS if the supplied buffer is too small to be useful for any encoding.
 *    Call recser_swap with a bigger buffer.
 *  -EINVAL otherwise
 *
 *  @note Ownership over record data is taken only on return value 0 or -EINVAL,
 *   otherwise the ownership remains with the caller */
int recser_put(recser_t *rs, record_t *rec);
/**
 * @brief Retrieve the buffer with the serialized data and place in a new one.
 *
 * @param rs pointer to the record serializer
 * @param out buffer structure with the new buffer properties. If the ptr member
 *  of the buffer is NULL, then the serializer will be invalidated, and any data
 *  (including unflushed records) associated with it will be freed. Further
 *  calls to this function or recser_put() will fail with EINVAL. On success
 *  (return value is 0 or -EAGAIN), this structure will be filled with the
 *  encoded buffer and the encoding length, otherwise will remain untouched.
 *
 * @return 0 on success, -EAGAIN if there are unflushed buffered records, other
 *  negative error otherwise
 *
 * @note -EAGAIN only signals the existence of unflushed records, not that they
 *  must be immediately flushed. It is normal operation to add further records
 *  until recser_put returns -EAGAIN or -ENOSPC, then swap the buffer. */
int recser_swap(recser_t *rs, UsefulBuf *out);

#endif /* INC_REC_SERIAL_H_ */
