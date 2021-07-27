/*
 * Copyright (C) 2021 Mihai Renea <mihai.renea@fu-berlin.de>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @file
 * @brief Hex byte-wise printer. Wrapper around printf with a VFS file descriptor
 * interface.
 */

#ifndef HEXOUT_H_
#define HEXOUT_H_
/**
 * Create a hex byte-wyse printer. Useful for debugging.
 *
 * @param name name of the printer
 *
 * @return VFS file descriptor on success, negative error otherwise */
int hexout_open(char const *name);

#endif /* HEXOUT_H_ */
