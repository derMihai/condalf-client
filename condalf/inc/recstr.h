/*
 * Copyright (C) 2021 Mihai Renea <mihai.renea@fu-berlin.de>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @file
 * @brief ConDaLF record stream interface
 */

#ifndef INC_RECSTR_H_
#define INC_RECSTR_H_

#include "record.h"
#include "mutex.h"
#include <errno.h>

#define RECORDSTREAM_MAX_STR_LEN 15

typedef struct recstr recstr_t;

typedef struct recstr_itf {
    int (*put)(recstr_t *, record_t *);
    int (*get)(recstr_t *, record_t *);
    int (*close)(recstr_t **);
} recstr_itf_t;

struct recstr {
    recstr_itf_t const *itf;
    mutex_t lock;
    char name[RECORDSTREAM_MAX_STR_LEN + 1];
};

/**
 * @brief Append a record to the stream. Thread safe.
 *
 * @param rs pointer to a recstr_t
 * @param rec pointer to a Record_t. If NULL, the stream will be flushed (if
 *  supported). On success, it will take ownership over the record's data,
 *  otherwise ownership remains with the caller.
 *
 * @return 0 on success, negative error otherwise
 *
 * @note As specified in \ref record_t, the record's name will be referenced
 *  but not copied. The implementation may cache this reference for indefinite
 *  time. If the caller wishes to modify/free the name, it MUST flush or drain
 *  the stream beforehand (e.g. by calling recstr_put() with \p rec == NULL).
 *
 * @note It is implementation defined if this function blocks. Thus, it is also
 *  implementation-defined what the success of this function means. */
static int recstr_put(recstr_t *rs, record_t *rec)
{
    if (!rs) return -EINVAL;
    if (!rs->itf->put) return -ENOSYS;

    mutex_lock(&rs->lock);
    int ret = rs->itf->put(rs, rec);
    mutex_unlock(&rs->lock);

    return ret;
}
/**
 * @brief Retrieve a Record_t from a stream, blocking. Thread safe.
 *
 * @param rs pointer to a recstr_t
 * @param rec pointer to a record to be filled on success.
 *
 * @return 0 on success, negative error otherwise
 *
 * @pre the record pointed by \p rec must be in a valid state */
static int recstr_get(recstr_t *rs, record_t *rec)
{
    if (!rs || !rec) return -EINVAL;
    if (!rs->itf->get) return -ENOSYS;

    mutex_lock(&rs->lock);
    int ret = rs->itf->get(rs, rec);
    mutex_unlock(&rs->lock);

    return ret;
}
/**
 * @brief Close a stream. This will flush the stream (if applicable) and
 * deallocate any resources allocated with the stream.
 *
 * @param rs pointer to pointer to stream. Set to NULL on return.
 *
 * @return 0 on success, negative error otherwise. */
static int recstr_close(recstr_t **rs)
{
    if (!rs) return -EINVAL;
    if (!*rs) return 0;
    if (!(*rs)->itf->close) return -ENOSYS;
    return (*rs)->itf->close(rs);
}

#endif /* INC_RECSTR_H_ */
