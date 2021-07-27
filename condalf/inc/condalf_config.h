/*
 * Copyright (C) 2021 Mihai Renea <mihai.renea@fu-berlin.de>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @file
 * @brief ConDaLF configuration file
 */

#ifndef INC_CONDALF_CONFIG_H_
#define INC_CONDALF_CONFIG_H_

/**
 * COAP block size used to transfer data. MUST be less or equal than \ref
 * CONFIG_NANOCOAP_BLOCK_SIZE_EXP_MAX */
#ifndef CDF_BLOCK_SIZE_EXP
#define CDF_BLOCK_SIZE_EXP      CONFIG_NANOCOAP_BLOCK_SIZE_EXP_MAX
#endif
/**
 * The Long Term Buffering implementation uses a dispatch queue to synchronize
 * accesses to the global LTB subsystem data structures (including file system).
 * While seemingly complicated, in this special case it actually simplifies the
 * implementation a lot, by removing the need for complex synchronization
 * mechanisms: everything that touches global objects happens serially, in the
 * same thread.
 *
 * This implementation also handles some time-intensive file system operations
 * that need mutual exclusion, so by asynchronously dispatching we can delay
 * execution without blocking other threads.
 *
 * This is inspired by Apple's Grand Central Dispatch.
 * @see https://en.wikipedia.org/wiki/Grand_Central_Dispatch
 *
 * This is the priority of the queue dispatcher thread. */
#ifndef LTB_QUEUE_PRIO
#define LTB_QUEUE_PRIO (THREAD_PRIORITY_MAIN - 2)
#endif
/**
 * When using the asynchronous family of transfer functions, the publisher
 * executes the jobs on a serial queue. This is the priority of the thread
 * that processes this queue. */
#ifndef PUBLISHER_QUEUE_PRIO
#define PUBLISHER_QUEUE_PRIO (THREAD_PRIORITY_MAIN - 1)
#endif

#endif /* INC_CONDALF_CONFIG_H_ */
