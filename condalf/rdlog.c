/*
 * Copyright (C) 2021 Mihai Renea <mihai.renea@fu-berlin.de>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

#if CONDALF_USE_RDLOG == 1

#include "errno.h"
#include "mutex.h"
#include "logging.h"
#include <stdarg.h>
#include <stdio.h>

#define DLOG_LEVEL DLOG_ERR
#include "rdlog.h"

#define RDLOG_ENC_BUF_LEN (RDLOG_REC_QUEUE_LEN * RDLOG_LOG_MAXLEN)

mutex_t _lock = MUTEX_INIT;
recstr_t *_logger = NULL;
timex_t (*_timef)(void) = NULL;

void _rdlog(unsigned level, char const *fmt, ...)
{
    static timex_t const time_zero = { 0 };
    static char const * const level_map[] = {
        [RDLOG_ERR] = "ERR",
        [RDLOG_WRN] = "WRN",
        [RDLOG_INF] = "INF",
        [RDLOG_DBG] = "DBG"
    };

    if (!fmt) return;
    if (level == 0 || level > RDLOG_DBG) return;

    char *buf = malloc(RDLOG_LOG_MAXLEN);
    if (!buf) return;

    va_list args;
    va_start(args, fmt);

    vsnprintf(buf, RDLOG_LOG_MAXLEN, fmt, args);

    va_end(args);

    DDBG("%s", buf);

    record_t rec = {
        .type = RECORDTYPE_STRING,
        .str = buf,
        .timestamp = _timef ? _timef() : time_zero,
        .name = level_map[level]
    };

    mutex_lock(&_lock);

    int res;
    if (!_logger || rec.timestamp.seconds == 0) {
        DDBG("disabled!\n");
        res = -1;
    } else {
        res = recstr_put(_logger, &rec);
    }

    mutex_unlock(&_lock);

    if (res) free(rec.str);
}

int RDLOG_enable(
    transdrv_t *transfer_driv,
    timex_t (*timef)(void),
    char const *base_name)
{
    if (!transfer_driv) return -EINVAL;

    logg_init_t logg_ini = {
        .base_name = base_name,
        .name = "RDLOG",
        .driv = transfer_driv,
        .record_queue_size = RDLOG_REC_QUEUE_LEN,
        .encoding_buf_size = RDLOG_ENC_BUF_LEN
    };

    recstr_t *logg;
    int res = logg_create(&logg_ini, &logg);

    if (res) {
        DERR("cannot create logger!\n");
        return res;
    }

    mutex_lock(&_lock);

    if (_logger) recstr_close(&_logger);
    _logger = logg;
    _timef = timef;

    mutex_unlock(&_lock);

    return 0;
}

void RDLOG_disable(void)
{
    mutex_lock(&_lock);
    if (_logger) recstr_close(&_logger);
    mutex_unlock(&_lock);
}

void RDLOG_flush(void)
{
    mutex_lock(&_lock);
    if (_logger) recstr_put(_logger, NULL);
    mutex_unlock(&_lock);
}

#endif /* CONDALF_USE_RDLOG == 1 */
