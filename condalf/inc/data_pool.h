/*
 * Copyright (C) 2021 Mihai Renea <mihai.renea@fu-berlin.de>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @file
 * @brief ConDaLF data pools - file directory used by each LTB instance */

#ifndef INC_DATA_POOL_H_
#define INC_DATA_POOL_H_

#if CONDALF_USE_LTB == 1

#include <stdint.h>
#include <stddef.h>

#define POOL_FNAME_MAX 8

/**
 * @brief Move an existing file to a pool.
 *
 * @pre the file and pool directory must exist and be closed.
 *
 * @param pooldir path to the pool directory
 * @param name path to the existing file
 *
 * @return 0 on success, negative error otherwise */
int dpool_move_file(char const *pooldir, char const *name);
/**
 * @brief Retrieve the path to the oldest file in a pool.
 *
 * @pre the pool directory must exist and be closed
 *
 * @param pooldir path to the pool directory
 * @param namebuf buffer to hold the path to the oldest file
 * @param buflen length of the buffer
 *
 * @return 0 on success, -ENOSPC if the buffer is to small to hold the path,
 *  other negative error otherwise */
int dpool_get_oldest_file(char const *pooldir, char *namebuf, size_t buflen);
/**
 * @brief Erase all the files in a pool.
 *
 * @pre the pool directory must exist and be closed
 *
 * @param pooldir path to the pool directory
 *
 * @return 0 on success,  negative error otherwise
 *
 * @note Files that don't follow in the pool's naming schema will not be deleted.
 *  The pool file names are hex numbers from 00000000 to ffffffff. */
int dpool_drain(char const *pooldir);
/**
 * Get the number of files in the pool.
 *
 * @param pooldir path to the pool directory
 *
 * @return number of files in the pool, negative error otherwise */
int dpool_size(char const *pooldir);

/**
 * Print the contents of the pool. For debug purposes only.
 *
 * @param pooldir path to the pool directory */
void dpool_print(char const *pooldir);

#endif /* CONDALF_USE_LTB == 1 */

#endif /* INC_DATA_POOL_H_ */

