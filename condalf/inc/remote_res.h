/*
 * Copyright (C) 2021 Mihai Renea <mihai.renea@fu-berlin.de>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @file
 * @brief ConDaLF CoAP resource description
 */

#ifndef INC_REMOTE_RES_H_
#define INC_REMOTE_RES_H_

#include "malloc.h"
#include <string.h>
#include <stdint.h>
#include <errno.h>

typedef struct rem_res {
    /**
     * ConDaLF backend server address (IPv6)*/
	char *address;
	/**
	 * ConDaLF backend server port */
    uint16_t port;
    /**
     * CoAP ressource path */
    char *res_location;
} rem_res_t;

/**
 * Copy a resource description.
 *
 * @param dst destination
 * @param src source
 *
 * @return 0 on success, negative error otherwise */
static int rem_res_cpy(rem_res_t *dst, rem_res_t const *src)
{
    if (!dst || !src) return -EINVAL;

    memset(dst, 0, sizeof(*dst));

    dst->address = strdup(src->address);
    dst->res_location = strdup(src->res_location);
    dst->port = src->port;

    if (!dst->address || !dst->res_location) {
        free(dst->address);
        free(dst->res_location);
        return -ENOMEM;
    }

    return 0;
}

/**
 * Free the content of a ressource description.
 *
 * @param res pointer to the resource
 *
 * @pre the data inside this function can be freed with \ref free(), e.g. it was
 * copied by \ref rem_res_copy() or created with malloc. */
static void rem_res_freedata(rem_res_t *res)
{
    if (!res) return;

    free(res->address);
    free(res->res_location);
    memset(res, 0, sizeof(*res));
}

#endif /* INC_REMOTE_RES_H_ */
