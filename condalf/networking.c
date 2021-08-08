/*
 * Copyright (C) 2021 Onur Demir <onud92@zedat.fu-berlin.de>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

#if CONDALF_USE_PUBLISHER == 1

#include "networking.h"
#include "remote_res.h"
#include <errno.h>
#include <stdio.h>
#include <vfs.h>
#include <fcntl.h>
#include <string.h>
#include <malloc.h>
#include <mutex.h>
#include "fmt.h"
#include "od.h"
#include "net/gcoap.h"
#include "assert.h"
#include "cond.h"
#include "condalf_config.h"

#define DLOG_LEVEL DLOG_INF
#include "dlog.h"

#define LENGHT_OF_SEND_PAYLOAD (1 << CDF_BLOCK_SIZE_EXP)

static const vfs_file_ops_t network_impl;

static void _resp_handler(const gcoap_request_memo_t *memo, coap_pkt_t* pdu,
                          const sock_udp_ep_t *remote);

typedef struct network_privdata {
	sock_udp_ep_t remote;
	char * rem_path;
	coap_pkt_t pdu;
	uint8_t buf[CONFIG_GCOAP_PDU_BUF_SIZE]; /* Defined in Makefile*/
	uint8_t buf_to_send[1024];
	uint16_t number_of_bytes;
	uint8_t err;
	coap_block1_t block1_init;
	cond_t send_cond;
	mutex_t lock;
} network_privdata_t;

#if DLOG_LEVEL >= DLOG_DBG

#include "hexout.h"

static int _print_payload(rem_res_t const *res, int fd)
{
    if (!res) return -EINVAL;

    int outfd = hexout_open(res->res_location);
    if (outfd < 0) return outfd;

    char buf[16];
    int retval;
    int sent = 0;

    while ((retval = vfs_read(fd, buf, sizeof(buf))) > 0) {
        int written = vfs_write(outfd, buf, retval);
        if (written != retval) {
            break;
        }
        sent += written;
    }

    vfs_lseek(fd, 0, SEEK_SET);

    vfs_close(outfd);

    if (retval < 0) return retval;
    return sent;
}
#else
#define _print_payload(...)
#endif

/* Return 1 on success, 0 on failure */
static ssize_t _init_remote(sock_udp_ep_t *remote, char *addr_str, uint16_t port_str)
{
    ipv6_addr_t addr;
    remote->family = AF_INET6;

    /* parse for interface */
    char *iface = ipv6_addr_split_iface(addr_str);
    if (!iface) {
        if (gnrc_netif_numof() == 1) {
            /* assign the single interface found in gnrc_netif_numof() */
            remote->netif = (uint16_t)gnrc_netif_iter(NULL)->pid;
        }
        else {
            remote->netif = SOCK_ADDR_ANY_NETIF;
        }
    }
    else {
        int pid = atoi(iface);
        if (gnrc_netif_get_by_pid(pid) == NULL) {
            puts("gcoap_cli: interface not valid");
            return 0;
        }
        remote->netif = pid;
    }

    /* parse destination address */
    if (ipv6_addr_from_str(&addr, addr_str) == NULL) {
        puts("client: unable to parse destination address");
        return 0;
    }
    if ((remote->netif == SOCK_ADDR_ANY_NETIF) && ipv6_addr_is_link_local(&addr)) {
        puts("client: must specify interface for link local target");
        return 0;
    }
    memcpy(&remote->addr.ipv6[0], &addr.u8[0], sizeof(addr.u8));

    remote->port = port_str;
    if (remote->port == 0) {
        puts("client: unable to parse destination port");
        return 0;
    }

    return 1;
}

/* Writes and sends next block for COAP resource request. */
static int _do_block_put(network_privdata_t* privdata)
{
    gcoap_req_init(&privdata->pdu, (uint8_t *)privdata->pdu.hdr, CONFIG_GCOAP_PDU_BUF_SIZE,
                   COAP_METHOD_PUT, privdata->rem_path);
    coap_opt_add_format(&privdata->pdu, COAP_FORMAT_SENML_CBOR);
    coap_opt_add_block1_control(&privdata->pdu, &privdata->block1_init);
    int len = coap_opt_finish(&privdata->pdu, COAP_OPT_FINISH_PAYLOAD);

    len += coap_payload_put_bytes(&privdata->pdu, &privdata->buf_to_send,
                                    privdata->number_of_bytes);

    ssize_t res = gcoap_req_send((uint8_t *)privdata->pdu.hdr, len, &privdata->remote, _resp_handler, privdata);
    if (res < 0) {
        printf("client: msg send failed: %d\n", (int)res);
        return 1;
    }
    return 0;
}



/* Response handler for client request to COAP resource. */
static void _resp_handler(const gcoap_request_memo_t *memo, coap_pkt_t* pdu,
                          const sock_udp_ep_t *remote)
{
    network_privdata_t *privdata = (network_privdata_t *) memo->context;

	if (memo->state == GCOAP_MEMO_TIMEOUT) {
		privdata->err = 1;
        printf("\nCoAP timeout.. Sending to Server failed.. -> This record will be dropped.. send next one\n\n");
        goto end;
    }
    else if (memo->state == GCOAP_MEMO_ERR) {
        printf("gcoap: error in response\n");
        goto end;
    }
    /* send next block if present */
    if (coap_get_code_raw(pdu) == COAP_CODE_CONTINUE) {
        privdata->block1_init.blknum++;
        printf("\n------- %u. Block containing %u bytes sent -------", privdata->block1_init.blknum, privdata->number_of_bytes);
    }
    /* if server got last block*/
    else if (coap_get_code_raw(pdu) == COAP_CODE_CHANGED) {
        printf("\n------- Last Block containing %u bytes sent -------", privdata->number_of_bytes);
    	printf("\n ------ SUCCESS: SERVER GOT ALL THE MESSAGES-------\n\n ");
    } else {
        privdata->err = 1;
    }

    end:
		mutex_lock(&privdata->lock);
		cond_signal(&privdata->send_cond);
		mutex_unlock(&privdata->lock);
}


int net_subsys_init(net_subsys_init_t *init)
{
    return 0;
}

int remstr_open(rem_res_t const *init)
{
    if (!init) return -EINVAL;

    //static_assert(CONFIG_GCOAP_PDU_BUF_SIZE == 512 && CONFIG_NANOCOAP_BLOCK_SIZE_EXP_MAX == 8);

    network_privdata_t *privdata = calloc(1, sizeof(*privdata));
    if (!privdata) return -ENOMEM;

    /*	set path of remote server */
    if (init->res_location) {
    	privdata->rem_path = strdup(init->res_location);
        if (!privdata->rem_path){
    		free(privdata->rem_path);
    		free(privdata);
    		return -ENOMEM;
        }
    }

    /* parse endpoint */
    if (!_init_remote(&privdata->remote, init->address, init->port)) {
		free(privdata->rem_path);
		free(privdata);
		return -EDESTADDRREQ;
	}

    /* reference buf to pdu and blockcount to first block*/
    privdata->pdu.hdr = (coap_hdr_t *) privdata->buf;
    privdata->number_of_bytes=0;
    privdata->err=0;

    /* Init Block Object*/
    coap_block_object_init(&privdata->block1_init,0,LENGHT_OF_SEND_PAYLOAD,1);


    int fd = vfs_bind(VFS_ANY_FD, O_WRONLY, &network_impl, privdata);
    if (fd < 0) {
		free(privdata->rem_path);
    	free(privdata);
	    return fd;
    }

    return fd;
}

static int _close(vfs_file_t *filp)
{

    network_privdata_t *privdata = (network_privdata_t *)filp->private_data.ptr;

    free(privdata->rem_path);
    free(privdata);

    return 0;
}


static ssize_t _write(vfs_file_t *filp, const void *src, size_t nbytes)
{
	network_privdata_t *privdata = (network_privdata_t *)filp->private_data.ptr;

    /* copy from Transferbuffer to Network Privdata Buffer*/
	memcpy(privdata->buf_to_send, src, nbytes);
    privdata->number_of_bytes = nbytes;

    /*	if its the last block, set "more" to 0*/
    if(nbytes<LENGHT_OF_SEND_PAYLOAD){
    	privdata->block1_init.more=0;
    }

    /* Do the actual sending*/
	_do_block_put(privdata);

	/* Wait for response from CoAP Server and continue afterwards*/
    mutex_lock(&privdata->lock);
    cond_wait(&privdata->send_cond, &privdata->lock);
    mutex_unlock(&privdata->lock);

    /* if send to server failed */
    if(privdata->err == 1){
    	return -1;
    }
    return nbytes;
}



int net_send(rem_res_t const *res, int fd){

	/* Buffer for read/write transfer*/
	char snd_buff[LENGHT_OF_SEND_PAYLOAD];
	int remfd, re;

	vfs_lseek(fd, 0, SEEK_SET);

	_print_payload(res, fd);

	/* Bind file descriptor for CoAP networking*/
	remfd = remstr_open(res);

	/* Read from file and send to CoAP Remote Server*/
	while ((re = vfs_read(fd, snd_buff, LENGHT_OF_SEND_PAYLOAD)) > 0) {
		int const cnt = re;
		re = vfs_write(remfd, snd_buff, cnt);
		if (re == -1) break;
	    }

	/* Close file descriptor for CoAP networking*/
	vfs_close(remfd);
	return re < 0 ? re : 0;
}

static const vfs_file_ops_t network_impl = {
	.close = _close,
    .write = _write
};

#endif /* CONDALF_USE_PUBLISHER == 1 */
