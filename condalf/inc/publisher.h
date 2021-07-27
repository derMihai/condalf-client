/*
 * Copyright (C) 2021 Mihai Renea <mihai.renea@fu-berlin.de>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @file
 * @brief ConDaLF CoAP publisher
 *
 * Enabled with \ref CONDALF_USE_PUBLISHER == 1
 */

#ifndef INC_PUBLISHER_H_
#define INC_PUBLISHER_H_

#if CONDALF_USE_PUBLISHER == 1

#include "transfer_driv.h"
#include "remote_res.h"

/**
 * @brief Init a ConDaLF CoAP publisher instance
 *
 * @param drvpp  pointer to a pointer to a transdrv_t. Will be set to the newly
 *  created instance on success.
 * @param rem_res pointer to the remote CoAP resource. Will be copied internally
 * upon return.
 * @param retry_cnt how many times to retry on sending failure.
 *
 * @note \p retry_cnt may have impact on responsiveness. Values of 0 - 3 should
 * suffice.
 *
 * @return 0 on success, negative error otherwise */
int publisher_init(transdrv_t **drvpp, rem_res_t const *rem_res, unsigned retry_cnt);

#endif /* CONDALF_USE_PUBLISHER == 1 */

#endif /* INC_PUBLISHER_H_ */
