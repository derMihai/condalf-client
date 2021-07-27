/*
 * Copyright (C) 2021 Mihai Renea <mihai.renea@fu-berlin.de>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @file
 * @brief ConDaLF logger
 *
 * The logger is thread-safe and does not block on IO. */

#ifndef INC_LOGGING_H_
#define INC_LOGGING_H_

#include "transfer_driv.h"
#include "recstr.h"
#include <stddef.h>

typedef struct logg_init {
    /**
     * Pointer to an initialized transfer driver. It is allowed to share a
     * driver between multiple logging instances. */
    transdrv_t *driv;
    /**
     * Flags, value of LOGGERF_* */
    int flags;
    /**
     * The size of the queue that buffers the records before being encoded and
     * flushed. The memory footprint a logger instance requires is thus roughly
     * proportional to this number. The current size of a record is 18 Bytes.
     *
     * Depending on the encoder implementation, large buffering could mean
     * better compression (not yet the case).
     *
     * The size of the queue should also be weighed against the size
     * of the encoding buffer (see \ref encoding_buf_size ), e.g. a large queue
     * size with small encoding buffer doesn't make sense.
     *
     * @note !!! MUST be power of 2 !!!
     *
     * @warning Special care should be taken if the logger is used with records
     * of type \ref RECORDTYPE_STRING. Every record in the queue owns a copy of
     * its string. If the strings are long, the dynamic memory requirements might
     * increase considerably. Again, this must also be weighed against the
     * encoding buffer size. */
    size_t record_queue_size;
    /**
     * The size of the output buffer for the encoding.
     *
     * Given the current implementation, large buffers can reduce the overhead
     * introduced by long term buffering and transmission. However, large buffer
     * only makes sense with large enough queue size (see \ref record_queue_size),
     * otherwise the encoder can not make use of its size. */
    size_t encoding_buf_size;
    /**
     * Name of the logging instance, will be copied internally.*/
    char *name;
    /**
     * Base name used as prefix for all the encodings. \ref record_t::name of
     * every record added with \ref recser_put() will be appended to this string
     * at decoding time. Leave NULL if not used. Will be copied internally by
     * \ref logg_create().
     *
     * Usecase: For an InfluxDB named "swp", and a sensor "cdf1", the base name
     * can be set to "swp:cdf1:". Thus, by adding a record with name
     * "light", the resolved name at decoding will be "swp:cdf1:light". Thus,
     * data traffic can be reduced, as the prefix must be sent only once. */
    char  const *base_name;
} logg_init_t;
/**
 * @brief Allocate and initialize a logger instance
 *
 * @param init pointer to init structure
 * @param log pointer to a pointer to record stream instance. On success, will
 *  be set to the newly allocated instance.
 *
 * @return 0 on success, negative error otherwise
 */
int logg_create(logg_init_t const *init, recstr_t **log);

#endif /* INC_LOGGING_H_ */
