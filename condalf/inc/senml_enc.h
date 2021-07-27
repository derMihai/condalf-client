/*
 * Copyright (C) 2021 Mihai Renea <mihai.renea@fu-berlin.de>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @file
 * @brief Quick and easy SenML CBOR encoder for ConDaLF records. Does not support
 *  any compression yet.
 */

#ifndef SRC_INC_SENML_ENC_H_
#define SRC_INC_SENML_ENC_H_

#include "record.h"
#include "qcbor.h"
#include <stddef.h>

typedef struct senml_enc {
    UsefulBuf buf;
    QCBOREncodeContext cbor_ctx;
} senml_enc_t;

/**
 * @brief init SenML encoder
 *
 * @param enc pointer to encoder
 * @param buf pointer to destination buffer. May be NULL.
 * @param size size of the buffer
 * @param base bases for the whole encoding. Copied internally, can be destroyed
 *  after this funtion returns
 *
 * @return 0 on success, negative error otherwise
 *
 * @note Passing a NULL pointer for the buffer is permitted. This way, the
 *  encoder simulates the space required, without actually encoding anything.
 *  see @ref senml_enc_put @ref senml_enc_close */
int senml_enc_init(senml_enc_t *enc, char *buf, size_t size, record_base_t const *base);
/**
 * Put a record in the buffer.
 *
 * @param enc pointer to encoder
 * @param rec record to be added
 *
 * @return 0 on success, -ENOSPC if we ran out of space in the output buffer,
 * -EINVAL otherwise
 *
 * @note If a NULL pointer was passed in \ref senml_inc_init as buffer, then
 *  this call will merely simulate if the record could have been added to the
 *  buffer of the specified size, without actually encoding anything. */
int senml_enc_put(senml_enc_t *enc, record_t const *rec);
/**
 * Close the encoder and the SenML packet associated with the buffer.
 *
 * @param enc pointer to encoder
 * @param enc_len on success, total bytes written in the buffer. May be null.
 *
 * @return 0 on success, -ENOSPC if we ran out of space in the output buffer
 *  before successfully closing the SenML packet, -EINVAL otherwise. */
int senml_enc_close(senml_enc_t *enc, size_t *enc_len);

#endif /* SRC_INC_SENML_ENC_H_ */
