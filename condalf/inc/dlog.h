/*
 * Copyright (C) 2021 Mihai Renea <mihai.renea@fu-berlin.de>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @file
 * @brief ConDaLF Diagnostics level-based logging functions.
 *
 * This can be included in a source file to allow level-based printf-like debugging.
 * The user must then define DLOG_LEVEL before the inclusion the logging level
 * (see \ref DLOG_LEVEL). A logging call will only be compiled if it's level is
 * less than \ref DLOG_LEVEL.
 *
 * The debugging function can also print an UNIX timestamp, if the user defines
 * the macro DLOG_TIME before inclusion.
 * This macro should resolve to an UNIX timestamp, in seconds.
 *
 * Example:
 *
 * #define DLOG_TIME (sntp_get_unix_usec() / US_PER_SEC)
 */

#ifndef INC_DLOG_H_
#define INC_DLOG_H_

#define DLOG_ERR 1 /**< Print error messages  */
#define DLOG_WRN 2 /**< Print error, warning messages  */
#define DLOG_INF 3 /**< Print error, warning, info messages  */
#define DLOG_DBG 4 /**< Print error, warning, info, debug messages  */

#define COLOR_RED  "\x1B[31m"
#define COLOR_NRM  "\x1B[0m"
#define COLOR_BLU  "\x1B[34m"
#define COLOR_YEL  "\x1B[33m"

/**
 * Define this before including this file to set the minimum log level
 *
 * The following example will print error, warning and info messages, but not
 * debug messages:
 *
 * #define DLOG_LEVEL DLOG_INF
 */
#ifndef DLOG_LEVEL
#define DLOG_LEVEL 0
#endif

#if DLOG_LEVEL > 0
#define ENABLE_DEBUG 1
#else
#define ENABLE_DEBUG 0
#endif

#include "debug.h"

#ifdef DLOG_TIME
#define DLOG(level, fmt, ...) DEBUG(level" %u %s: "fmt, (unsigned)(DLOG_TIME), __func__, ##__VA_ARGS__)
#else
#define DLOG(level, fmt, ...) DEBUG(level" %s: "fmt, __func__, ##__VA_ARGS__)
#endif

/**
 * Debug logging
 */
#if (DLOG_LEVEL >= DLOG_DBG)
#define DDBG(fmt, ...) DLOG(COLOR_BLU"DBG"COLOR_NRM, fmt, ##__VA_ARGS__)
#else
#define DDBG(...)
#endif
/**
 * Info logging
 */
#if (DLOG_LEVEL >= DLOG_INF)
#define DINF(fmt, ...) DLOG("INF", fmt, ##__VA_ARGS__)
#else
#define DINF(...)
#endif
/**
 * Error logging
 */
#if (DLOG_LEVEL >= DLOG_WRN)
#define DWRN(fmt, ...) DLOG(COLOR_YEL"WRN"COLOR_NRM, fmt, ##__VA_ARGS__)
#else
#define DWRN(...)
#endif
/**
 * Error logging
 */
#if (DLOG_LEVEL >= DLOG_ERR)
#define DERR(fmt, ...) DLOG(COLOR_RED"ERR"COLOR_NRM, fmt, ##__VA_ARGS__)
#else
#define DERR(...)
#endif

#endif /* INC_DLOG_H_ */
