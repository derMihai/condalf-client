/*
 * Copyright (C) 2021 Mihai Renea <mihai.renea@fu-berlin.de>
 *                    Onur Demir  <onud92@zedat.fu-berlin.de>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @file
 * @brief ConDaLF usecase example
 * */

/* ConDaLF */
#include "logging.h"

/* RIOT */
#include "periph/adc.h"
#include "periph/rtc.h"
#include "xtimer.h"
#include "net/sntp.h"
#include "periph/pm.h"
#include "net/sock/udp.h"
#include "arpa/inet.h"

/* STD */
#include <assert.h>
#include <stdbool.h>

#if CONDALF_USE_PUBLISHER == 1
/* ConDaLF */
#include "publisher.h"
#endif

#if CONDALF_USE_LTB == 1
/* ConDaLF */
#include "ltb.h"

/* RIOT */
#include "fs/littlefs2_fs.h"
#include "board.h"
#endif

/* Define the time-stamp macro for the DLOG calls */
#define DLOG_TIME (sntp_get_unix_usec() / US_PER_SEC)
/* Define the logging level for the DLOG calls. DLOG_DBG enables all of them. */
#define DLOG_LEVEL DLOG_DBG
/* Define the logging level for the RDLOG calls. Only info, warning and error
 * logs will be sent, but not debugging. */
#define RDLOG_LEVEL DLOG_INF
#include "rdlog.h"

#define SNTP_ADDR "2610:20:6f96:96::4"
#define SNTP_PORT   123

#define LIGHT_ADC_LINE 10 // ESP32 gpio 32
#define TEMP_ADC_LINE 11 // ESP32 gpio 33

#define TIME_UPDATE_PRIO (THREAD_PRIORITY_MAIN + 1)
static char time_update_stack[THREAD_STACKSIZE_MAIN];

static bool volatile time_is_set = false;
static bool volatile must_stop = false;

#define ENCODING_BUFSIZE  2048
#define ENCODING_QUEUELEN 64
#define PROBING_PERIOD (5 * US_PER_SEC)

#if CONDALF_USE_LTB == 1

#define FS_MOUNT_POINT "/fs"
#define LTB_FILECNT_TRESHOLD 4
#define LTB_DATA_POOLDIR (FS_MOUNT_POINT "/datpool")

static littlefs2_desc_t fs_desc = {
    .lock = MUTEX_INIT,
};

static vfs_mount_t flash_mount = {
    .fs = &littlefs2_file_system,
    .mount_point = FS_MOUNT_POINT,
    .private_data = &fs_desc,
};

static int fs_setup(void)
{
    fs_desc.dev = MTD_0;

//    DINF("formatting..\n");
//    vfs_format(&flash_mount);
//    DINF("formatted!\n");

    int res = vfs_mount(&flash_mount);
    if (res < 0) {
        DERR("mount failed: %d, %x\n", res, (unsigned)fs_desc.dev);

        DINF("formatting..\n");
        vfs_format(&flash_mount);
        DINF("formatted!\n");

        res = vfs_mount(&flash_mount);
        if (res < 0) {
            DERR("mount failed again: %d, %x\n", res, (unsigned)fs_desc.dev);
            return res;
        }
    }
    DINF("mounted!\n");
    return res;
}

#endif

/* Provides time-stamps for the RDLOG calls */
timex_t rdlog_timef(void)
{
    uint64_t unixtime = time_is_set ? sntp_get_unix_usec() : 0;
    timex_t ts = {
        .microseconds = unixtime % US_PER_SEC,
        .seconds      = unixtime / US_PER_SEC
    };
    return ts;
}

/* Handles time updates asynchronously */
void *time_update(void *arg)
{
    sock_udp_t sntp_sock;
    sock_udp_ep_t sntp_ep = SOCK_IPV6_EP_ANY;
    sntp_ep.port = SNTP_PORT;
    inet_pton(AF_INET6, SNTP_ADDR, &sntp_ep.addr.ipv6);

    while (1) {
        int res;
        unsigned try = 4;

        do {
            res = sock_udp_create(&sntp_sock, &sntp_ep, NULL, 0);
            if (res < 0) {
                DERR("SNTP: cannot create UDP sock: %d\n", res);

            } else {
                res = sntp_sync(&sntp_ep, 10 * US_PER_SEC);
                sock_udp_close(&sntp_sock);

                if (res < 0) xtimer_sleep(5);
            }
        } while (res < 0 && --try);

        if (res < 0) {
            RDERR("SNTP: cannot sync: %d", res);

            /* Unfortunately, after repeatedly trying, it still doesn't
             * work, only a system restart settles it... */
            must_stop = true;

            xtimer_sleep(1 * 60);

        } else {
            time_is_set = true;

            uint64_t unixtime = sntp_get_unix_usec() / US_PER_SEC;
            RDINF("UNIX time updated: %u", (uint32_t)unixtime);
#if CONDALF_USE_RDLOG == 1
            RDLOG_flush();
#endif

            xtimer_sleep(1 * 60 * 60);
        }
    }

    return NULL;
}

int usecase(void)
{
    /* Nothing much can be done with none of these modules */
    static_assert(CONDALF_USE_PUBLISHER == 1 || CONDALF_USE_LTB == 1,
        "Please enable at least the ConDaLF publisher or LTB.");

    DINF("ConDaLF, running on %s.\n", RIOT_BOARD);
    DINF("\n\tCONDALF_USE_PUBLISHER=%d\n"
           "\tCONDALF_USE_LTB=%d\n"
           "\tCONDALF_USE_RDLOG=%d\n",
         CONDALF_USE_PUBLISHER,
         CONDALF_USE_LTB,
         CONDALF_USE_RDLOG);

    int res = thread_create(
        time_update_stack,
        sizeof(time_update_stack),
        TIME_UPDATE_PRIO,
        0,
        time_update,
        NULL,
        "timupd");


    if (res < 0) {
        DERR("cannot start time update thread: %d\n", res);
        return -1;
    }

#if CONDALF_USE_PUBLISHER == 1
    /* if data publishing is enabled, configure the publisher. We use a common
     * CoAP server and resource for data and RDLOG diagnostics. */
    static rem_res_t const rem_res = {
        .address      = USECASE_BACKEND_ADDR,
        .port         = USECASE_BACKEND_PORT,
        .res_location = USECASE_BACKEND_RESSOURCE
    };

    transdrv_t *sender = NULL;
    res = publisher_init(&sender, &rem_res, 1);
    if (res) {
        DERR("cannot init sender: %s\n", strerror(res));
        return -1;
    }
#endif

#if CONDALF_USE_LTB == 1
    /* If LTB enabled, we have to set up the file system */
    res = fs_setup();
    if (res) {
        DERR("cannot init FS: %d\n", res);
        return -1;
    }

    /* Init the LTB subsystem to publish (if enabled) whenever the number or
     * files (SenML packs) ist greater or equal LTB_FILECNT_TRESHOLD. */
    static ltb_subsys_init_t ltb_subsys_param = {
        .nb_files_lim = LTB_FILECNT_TRESHOLD
    };

    res = ltb_subsys_init(&ltb_subsys_param);
    if (res) {
        DERR("cannot init LTB subsys: %s\n", strerror(res));
        return -1;
    }
    /* Create the pool directory for the only LTB that we use. */
    res = vfs_mkdir(LTB_DATA_POOLDIR, 0);
    if (res && res != -EEXIST) {
        DERR("cannot make pooldir: %d\n", res);
        return -1;
    }

    ltb_init_t ltb_param = {
        .name = "datltb",
#if CONDALF_USE_PUBLISHER == 1
        /* If we want to send, we bind the previously created publisher to the
         * LTB instance... */
        .sender = sender,
#else
        /* ...otherwise we set it to NULL to disable sending. The LTB then only
         * stores locally. */
        .sender = NULL,
#endif
        .pool_path = LTB_DATA_POOLDIR
    };

    transdrv_t *ltb = NULL;
    res = ltb_create(&ltb, &ltb_param);
    if (res) {
        DERR("cannot init LTB: %s\n", strerror(res));
        return -1;
    }
#if CONDALF_USE_RDLOG == 1
    /* Configure the RDLOG to use the previously created LTB */
    res = RDLOG_enable(
        ltb, // bind the transfer driver. If NULL, it will only print locally.
        rdlog_timef, // bind the time-stamp function; mandatory
        USECASE_INFLUXDB":"USECASE_INSTANCE":" // set the prefix for record names
        );
    if (res) {
        DERR("cannot init RDLOG: %d\n", res);
    }
#endif
#elif CONDALF_USE_PUBLISHER == 1 && CONDALF_USE_RDLOG == 1
    /* Configure the RDLOG to use the previously created publisher directly */
    res = RDLOG_enable(sender, rdlog_timef, USECASE_INFLUXDB":"USECASE_INSTANCE":");
    if (res) {
        DERR("cannot init RDLOG: %d\n", res);
    }
#endif

    DINF("%u\n", __LINE__);

    /* Finally, we create the logger. */
    logg_init_t logg_init = {
        .name = "data",
        .record_queue_size = ENCODING_QUEUELEN,
        .encoding_buf_size = ENCODING_BUFSIZE,
        .base_name = USECASE_INFLUXDB":"USECASE_INSTANCE":", // set the prefix for record names
#if CONDALF_USE_LTB == 1
        /* If using LTB, bind it... */
        .driv = ltb
#else
        /* ...otherwise bind the publisher directly.  */
        .driv = sender
#endif
    };

    recstr_t *logger = NULL;
    res = logg_create(&logg_init, &logger);
    if (res) {
        DERR("cannot init logger instance: %s\n", strerror(res));
        return -1;
    }

    if (adc_init(LIGHT_ADC_LINE)) {
        DERR("cannot init light adc\n");
        return -1;
    }

    if (adc_init(TEMP_ADC_LINE)) {
        DERR("cannot init temp adc\n");
        return -1;
    }

    while (!must_stop) {
        int32_t light_sample = adc_sample(LIGHT_ADC_LINE, ADC_RES_10BIT);
        int32_t temp_sample  = adc_sample(TEMP_ADC_LINE, ADC_RES_10BIT);

        /* percentile light */
        light_sample = light_sample * 100 / 1024;
        /* voltage in mV */
        temp_sample = temp_sample * 3300 / 1024;
        /* LM35: 10mv / degree Celsius */
        temp_sample = temp_sample / 10;
        /* for some mysterious reason, the LM35 is outputting exactly half the
         * real temperature.
         * The ADC measures 0 for GND and 1023 for 3.3V */
        temp_sample *= 2;

        static int32_t sum_light = 0;
        static int32_t sum_temp  = 0;

        sum_light = sum_light - (sum_light / 10) + light_sample;
        sum_temp  = sum_temp - (sum_temp / 10) + temp_sample;

        DDBG("ADC: light=%i%%(%i), temp=%i°C(%i)\n",
            light_sample, sum_light / 10, temp_sample, sum_temp / 10);

        uint64_t unixtime = sntp_get_unix_usec();

        /* Create a sample of integer type */
        record_t sample = {
            .type = RECORDTYPE_I32,
        };
        /* Set the time-stamp */
        sample.timestamp.microseconds = unixtime % US_PER_SEC;
        sample.timestamp.seconds      = unixtime / US_PER_SEC;

        if (time_is_set) {
            /* no way our light diode gives us any accurate values, so we'll go
             * for percentile light. Anyway, InfluxDB ignores it. */
            sample.unit = RECORDUNIT_percent;
            sample.name = "light";
            sample.i32  = light_sample;

            /* Log the sample using the created logger. */
            res = recstr_put(logger, &sample);

            if (res) {
                DERR("restr_put() failed: %s\n", strerror(res));
                return -1;
            }

            sample.unit = RECORDUNIT_Cel;
            sample.name = "temp";
            sample.i32  = temp_sample;

            res = recstr_put(logger, &sample);

            if (res) {
                DERR("restr_put() failed: %s\n", strerror(res));
                return -1;
            }

        } else {
            DWRN("Time invalid, record skipped.\n");
        }

        xtimer_usleep(PROBING_PERIOD);
    }


    RDWRN("Client must restart...");

    /* First, close any loggers using transfer drivers. */
#if CONDALF_USE_RDLOG == 1
    RDLOG_disable();
#endif
    recstr_close(&logger);

#if CONDALF_USE_LTB == 1
    /* Close the LTB using the sender */
    transdrv_delete(&ltb);
#endif

#if CONDALF_USE_PUBLISHER == 1
    /* Finally, close the sender */
    transdrv_delete(&sender);
#endif

    DWRN("=====================\n");
    DWRN("      RESTARTING\n");
    DWRN("=====================\n");

    xtimer_usleep(100 * US_PER_MS);

    pm_reboot();

    return 0;
}
