/*
 * Copyright (C) 2021 Mihai Renea <mihai.renea@fu-berlin.de>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @file
 * @brief Transfer driver interface
 */

#ifndef INC_TRANSFER_DRIV_H_
#define INC_TRANSFER_DRIV_H_

#include <errno.h>

typedef struct transdrv transdrv_t;
typedef struct transfer_job transfer_job_t;

typedef struct {
    int  (*trysend)(transdrv_t *, transfer_job_t *);
    int  (*tryrecv)(transdrv_t *, transfer_job_t *);
    int  (*send)   (transdrv_t *, transfer_job_t *);
    int  (*recv)   (transdrv_t *, transfer_job_t *);
    void (*delete) (transdrv_t **);
} transdrv_itf_t;

struct transdrv {
    transdrv_itf_t const *itf;
};
/**
 * @brief Object describing a transfer.
 */
struct transfer_job {
    /** File descriptor used to write to / read from */
    int fd;
    /** Callback to be called on completion. May be NULL.
     *  @param job the completed job
     *  @param status the completion status of the job, 0 on success, negative
     *   error otherwise
     *  - for async transfers: if the transfer cannot be enqueued (i.e. the
     *    transdrv_try*() function returns an error), this callback will NOT be
     *    called. However, if successfully enqueued, the callback will be called
     *    on transfer completion, no matter if successful or not.
     *  - for synchronous transfers: if the transfer function returns an error,
     *    this callback will NOT be called.
     *
     *   Either way, the user MUST make sure that eventual cleanup is done EITHER
     *   in the callback on success, OR after the transfer call successfully
     *   returns. */
    void (*cb)(transfer_job_t *job, int status);
    /** Private data for the driver implementation to use. Do NOT use this
     * externally! Add custom fields below, if necessary. */
    void *_drv_priv;
};
/**
 * @brief Start a send transfer asynchronously. If the transfer cannot be
 *  enqueued immediately, it will return. Thread safe.
 *
 * @param drv pointer to transfer driver
 * @param job pointer to transfer job
 *
 * @return 0 on success, -EWOULDBLOCK if the transfer cannot be enqueued,
 *  other negative error otherwise.
 *
 * @note in case of error, the callback supplied in the job will NOT be called.
 */
static int transdrv_trysend(transdrv_t *drv, transfer_job_t *job)
{
    if (!drv || !job) return -EINVAL;
    if (!drv->itf->trysend) return -ENOSYS;
    return drv->itf->trysend(drv, job);
}
/**
 * @brief Start a send transfer synchronously. Thread safe.
 *
 * @param drv pointer to transfer driver
 * @param job pointer to transfer job
 *
 * @return 0 on success, negative error otherwise.
 *
 * @note in case of error, the callback supplied in the job will NOT be called.
 */
static int transdrv_send(transdrv_t *drv, transfer_job_t *job)
{
    if (!drv || !job) return -EINVAL;
    if (!drv->itf->send) return -ENOSYS;
    return drv->itf->send(drv, job);
}
/**
 * Start a receive transfer asynchronously. If the transfer cannot be
 * enqueued immediately, it will return. Thread safe.
 *
 * @param drv pointer to transfer driver
 * @param job pointer to transfer job
 *
 * @return 0 on success, -EWOULDBLOCK if the transfer cannot be enqueued, other
 * negative error otherwise.
 *
 * @note in case of error, the callback supplied in the job will NOT be called.
 */
static int transdrv_tryrecv(transdrv_t *drv, transfer_job_t *job)
{
    if (!drv || !job) return -EINVAL;
    if (!drv->itf->tryrecv) return -ENOSYS;
    return drv->itf->tryrecv(drv, job);
}
/**
 * Start a receive transfer synchronously. Thread safe.
 *
 * @param drv pointer to transfer driver
 * @param job pointer to transfer job
 *
 * @return 0 on success, negative error otherwise.
 *
 * @note in case of error, the callback supplied in the job will NOT be called.
 */
static int transdrv_recv(transdrv_t *drv, transfer_job_t *job)
{
    if (!drv || !job) return -EINVAL;
    if (!drv->itf->recv) return -ENOSYS;
    return drv->itf->recv(drv, job);
}
/**
 * Delete and deallocated a transfer driver.
 *
 * @param drv pointer to pointer to transfer driver
 *
 * @note depending on implementation, this might block
 */
static void transdrv_delete(transdrv_t **drv)
{
    if (!drv || !*drv) return;
    (*drv)->itf->delete(drv);
}

#endif /* INC_TRANSFER_DRIV_H_ */
