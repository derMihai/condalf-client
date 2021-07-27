/*
 * Copyright (C) 2021 Mihai Renea <mihai.renea@fu-berlin.de>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @file
 * @brief ConDaLF Long Term Buffering.
 *
 * Enabled with CONDALF_USE_LTB == 1
 *
 * With the aid of a file system, the LTB subsystem can be used to buffer logger
 * data for a long time. This is useful if the application should/can only
 * publish in bursts, e.g. network connectivity isn't permanently available, or
 * for power efficiency reasons. */

#ifndef INC_LTB_H_
#define INC_LTB_H_

#if CONDALF_USE_LTB == 1

#include "transfer_driv.h"
#include <stddef.h>
#include <stdbool.h>

#define LTB_NAME_LEN_MAX 8

/** Arguments for the LTB subsystem */
typedef struct {
    /**
     * Every time a LTB instance adds a file, the total number of files over
     * all instances is added up and, if greater or equal than this number,
     * the subsystem will attempt to publish all the files. */
    size_t nb_files_lim;
    /**
     * If provided, the return value of this function is logically &&ed with
     * the internal condition (number of files, see \ref nb_files_lim). If set
     * to NULL, it is ignored and considered true.
     *
     * @return true, if the subsystem should publish, false otherwise */
    bool (*ext_cond)(void);
} ltb_subsys_init_t;

/** Arguments for the creation of a LTB instance */
typedef struct {
    /**
     *  Path to the file directory that will serve as storage pool for the LTB
     *  instance. MUST be unique for every instance.
     *
     *  @pre the folder MUST exist and SHOULD not contain any user data
     *
     *  @post the user MUST NOT modify the contents of this folder directly */
    char *pool_path;
    /**
     * Pointer to a transfer driver that will be used to publish the files.
     * Set NULL if you only want to store locally. */
    transdrv_t *sender;
    /**
     * Name of the LTB instance. MUST be provided. Will be truncated to
     * \ref LTB_NAME_LEN_MAX. */
    char *name;
} ltb_init_t;

/**
 * Init the LTB subsystem.
 *
 * @param init see \ref ltb_subsys_init_t
 *
 * @return 0 on success, negative error otherwise */
int ltb_subsys_init(ltb_subsys_init_t const *init);
/**
 * Create a LTB instance.
 *
 * @param drvpp pointer to a pointer to a LTB instance, that will be set to the
 *  newly created instance on success.
 *
 * @param init see \ref ltb_init_t
 *
 * @return 0 on success, negative error otherwise
 *
 * @pre The subsystem was initialized with \ref ltb_subsys_init */
int ltb_create(transdrv_t **drvpp, ltb_init_t const *init);
/**
 * Force the publishing of files, no matter the conditions.
 *
 * @param cb callback to be called on completion. \p res is the completion
 *  status, 0 on success, negative error otherwise.
 *
 * @return 0 if the publishing reques to the subsystem was successfully enqueued,
 *  negative error otherwise.
 *
 * @note this function does not block, so a success return value does not
 * necessarily mean the files were successfully published. */
int ltb_force_publish(void (*cb)(int res));

#endif /* CONDALF_USE_LTB == 1 */

#endif /* INC_LTB_H_ */
