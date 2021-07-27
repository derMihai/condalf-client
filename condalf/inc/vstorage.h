/*
 * Copyright (C) 2021 Mihai Renea <mihai.renea@fu-berlin.de>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @file
 * @brief VFS file descriptor wrapper around a buffer. */

#ifndef INC_VSTORAGE_H_
#define INC_VSTORAGE_H_

#include <stdint.h>

/**
 * On close, the file buffer will be automatically freed using free() */
#define VSTORF_OWNS_BUF  0x1 << 1
/**
 * Assumes buffer is filled with data. Effectively sets EOF at the buffer
 *  end. */
#define VSTORF_BUF_HAS_DATA 0x1 << 2

/**
 * @brief init structure for virtual storage file
 */
typedef struct vstorfile_init {
    char *buf; /**< Buffer to use as virtual storage */
    uint32_t bufsiz; /**< size of the buffer */
    int32_t flags; /**< flags, as value of VSTORF_* */
} vstorfile_init_t;

/**
 * @brief Create a RAM storage file.
 *
 * @param init see \ref vstorfile_init_t
 *
 * @return flie descriptor on success, negative error otherwise */
int vstorfile_open(vstorfile_init_t *init);

#endif /* INC_VSTORAGE_H_ */
