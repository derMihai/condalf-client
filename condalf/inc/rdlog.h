/*
 * Copyright (C) 2021 Mihai Renea <mihai.renea@fu-berlin.de>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @file
 * @brief ConDaLF Remote Diagnostics level-based logging functions.
 *
 * Enabled with \ref CONDALF_USE_RDLOG == 1. If not enabled, the logging functions
 * can still be used, but will only behave like the ones in \ref log.h.
 *
 * This can be included in a source file to allow level-based printf-like
 * debugging, which will also be sent to the ConDaLF backend.
 * The user must then define RDLOG_LEVEL before the inclusion the logging level
 * (see \ref RDLOG_LEVEL). A logging call will only be compiled if it's level is
 * less than \ref RDLOG_LEVEL.
 *
 * @note This implementation also uses the functions in \ref dlog.h to print
 * locally. Thus, the user also has to define the requirements for \ref dlog.h
 * before inclusion (see \ref dlog.h). Also, the user MUST NOT include \ref
 * dlog.h.
 *
 * @see \ref dlog.h */

#ifndef INC_RDLOG_H_
#define INC_RDLOG_H_

#include "dlog.h"
#include "transfer_driv.h"
#include "timex.h"
/**
 * Maximum length of a logged string.
 *
 * @see \ref logg_init_t */
#ifndef RDLOG_LOG_MAXLEN
#define RDLOG_LOG_MAXLEN 64
#endif
/**
 * Record queue length of the internally used logger
 *
 * @note MUST be power of 2!
 *
 * @see \ref logg_init_t  */
#ifndef RDLOG_REC_QUEUE_LEN
#define RDLOG_REC_QUEUE_LEN 8
#endif

#define RDLOG_ERR DLOG_ERR /**< Print error messages  */
#define RDLOG_WRN DLOG_WRN /**< Print error, warning messages  */
#define RDLOG_INF DLOG_INF /**< Print error, warning, info messages  */
#define RDLOG_DBG DLOG_DBG /**< Print error, warning, info, debug messages  */
/**
 * Define this before including this file to set the minimum log level
 *
 * The following example will print error, warning and info messages, but not
 * debug messages:
 *
 * #define RDLOG_LEVEL DLOG_INF */
#ifndef RDLOG_LEVEL
#define RDLOG_LEVEL 0
#endif

#if CONDALF_USE_RDLOG == 1
extern void _rdlog(unsigned level, char const *fmt, ...);
/**
 * Enable the remote diagnostics. Can be called multiple times. If never called,
 * it will only print locally.
 *
 * @param transfer_driv the transfer driver to be used
 * @param timef a function returning the current timestamp whenever a logging
 *  function is called. If the returned \ref timex_t::seconds of this function
 *  is 0, the the logged string will not be sent.
 * @param base_name prefix for the record names. See \ref logg_init_t::base_name.
 *  Will be copied internally.
 *
 * @return 0 on success, negative error otherwise */
int RDLOG_enable(transdrv_t *transfer_driv, timex_t (*timef)(void), char const *base_name);
/**
 * Flush the log buffer. */
void RDLOG_flush(void);
/**
 * Disable the remote diagnostics. Further calls to log functions will only print
 * locally. */
void RDLOG_disable(void);
#else
#define _rdlog(...)
#endif
/**
 * Debug logging */
#if (RDLOG_LEVEL >= RDLOG_DBG)
#define RDDBG(fmt, ...) do {                \
    _rdlog(RDLOG_DBG, fmt, ##__VA_ARGS__);  \
    DDBG(fmt"\n", ##__VA_ARGS__);           \
} while (0)
#else
#define RDDBG(...) DDBG(__VA_ARGS__)
#endif
/**
 * Info logging */
#if (RDLOG_LEVEL >= RDLOG_INF)
#define RDINF(fmt, ...) do {                \
    _rdlog(RDLOG_INF, fmt, ##__VA_ARGS__);  \
    DINF(fmt"\n", ##__VA_ARGS__);           \
} while (0)
#else
#define RDINF(...) DINF(__VA_ARGS__)
#endif
/**
 * Warning logging */
#if (RDLOG_LEVEL >= RDLOG_WRN)
#define RDWRN(fmt, ...) do {                \
    _rdlog(RDLOG_WRN, fmt, ##__VA_ARGS__);  \
    DWRN(fmt"\n", ##__VA_ARGS__);           \
} while (0)
#else
#define RDWRN(...) DWRN(__VA_ARGS__)
#endif
/**
 * Error logging */
#if (RDLOG_LEVEL >= RDLOG_ERR)
#define RDERR(fmt, ...) do {                \
    _rdlog(RDLOG_ERR, fmt, ##__VA_ARGS__);  \
    DERR(fmt"\n", ##__VA_ARGS__);           \
} while (0)
#else
#define RDERR(...) DERR(__VA_ARGS__)
#endif


#endif /* INC_RDLOG_H_ */
