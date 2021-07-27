/*
 * Copyright (C) 2021 Onur Demir <onud92@zedat.fu-berlin.de>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @file
 * @brief Performs CoAP transfers for ConDaLF
 */

#if CONDALF_USE_PUBLISHER == 1

#ifndef INC_NETWORKING_H_
#define INC_NETWORKING_H_

#include "remote_res.h"
#include <stdint.h>


typedef struct net_subsys_init {
    /* Parameters for the global net subsys */
    int dummy;
} net_subsys_init_t;

/**
 * @brief init the global state of the networking subsystem
 *
 * @param init parameters
 *
 * @return 0 on success, negative error otherwise
 */
int net_subsys_init(net_subsys_init_t *init);
/**
 * @brief Send data from a file descriptor to a CoAP resource.
 * The function blocks until the transfer is complete, or an error happens.
 *
 * @param res pointer to rem_res_t structure describing the CoAP ressource
 * @param fd VFS file descriptor to read from
 *
 * @return 0 on success, negative error otherwise */
int net_send(rem_res_t const *res, int fd);
/**
 * @brief Receive data from a CoAP ressource into a file descriptor.
 * The function blocks until the transfer is complete, or an error happens.
 *
 * @param res pointer to rem_res_t structure describing the CoAP ressource
 * @param fd VFS file descriptor to write to
 *
 * @return 0 on success, negative error otherwise */
int net_recv(rem_res_t const *res, int fd);

#endif /* CONDALF_USE_PUBLISHER == 1 */

#endif /* INC_NETWORKING_H_ */
