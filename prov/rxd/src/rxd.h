/*
 * Copyright (c) 2015-2018 Intel Corporation, Inc.  All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#if HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include <pthread.h>
#include <rdma/fabric.h>
#include <rdma/fi_atomic.h>
#include <rdma/fi_cm.h>
#include <rdma/fi_domain.h>
#include <rdma/fi_endpoint.h>
#include <rdma/fi_eq.h>
#include <rdma/fi_errno.h>
#include <rdma/fi_rma.h>
#include <rdma/fi_tagged.h>
#include <rdma/fi_trigger.h>

#include <ofi.h>
#include <ofi_proto.h>
#include <ofi_enosys.h>
#include <ofi_rbuf.h>
#include <ofi_list.h>
#include <ofi_util.h>
#include <ofi_tree.h>

#ifndef _RXD_H_
#define _RXD_H_

#define RXD_MAJOR_VERSION 	(1)
#define RXD_MINOR_VERSION 	(0)
#define RXD_PROTOCOL_VERSION 	(1)
#define RXD_FI_VERSION 		FI_VERSION(1,6)

#define RXD_IOV_LIMIT		4
#define RXD_NAME_LENGTH		64
#define RXD_INJECT_SIZE		4096
#define RXD_MAX_MTU_SIZE	4096

#define RXD_MAX_TX_BITS 	10
#define RXD_MAX_RX_BITS 	10

#define RXD_BUF_POOL_ALIGNMENT	16
#define RXD_TX_POOL_CHUNK_CNT	1024
#define RXD_RX_POOL_CHUNK_CNT	1024
#define RXD_MAX_UNACKED		128
#define RXD_MAX_PENDING		128
#define RXD_MAX_PKT_RETRY	50

#define RXD_REMOTE_CQ_DATA	(1 << 0)
#define RXD_NO_COMPLETION	(1 << 1)
#define RXD_INJECT		(1 << 2)
#define RXD_RETRY		(1 << 3)
#define RXD_LAST		(1 << 4)

struct rxd_env {
	int spin_count;
	int retry;
	int max_peers;
};

extern struct rxd_env rxd_env;
extern struct fi_provider rxd_prov;
extern struct fi_info rxd_info;
extern struct fi_fabric_attr rxd_fabric_attr;
extern struct util_prov rxd_util_prov;

struct rxd_fabric {
	struct util_fabric util_fabric;
	struct fid_fabric *dg_fabric;
};

struct rxd_domain {
	struct util_domain util_domain;
	struct fid_domain *dg_domain;

	ssize_t max_mtu_sz;
	ssize_t max_inline_sz;
	ssize_t max_seg_sz;
	int mr_mode;
	struct ofi_mr_map mr_map;//TODO use util_domain mr_map instead
};

struct rxd_peer {
	struct dlist_entry entry;
	fi_addr_t peer_addr;
	uint32_t tx_seq_no;
	uint32_t rx_seq_no;
	uint32_t last_ack_seq_no;
	uint32_t tx_msg_id;
	uint32_t rx_msg_id;
	uint16_t rx_window;//constant at MAX_UNACKED for now
	uint16_t tx_window;//unused for now, will be used for slow start

	uint16_t unacked_cnt;
	int pending_cnt;

	uint32_t curr_rx_id;
	uint32_t curr_tx_id;

	uint8_t blocking;
	struct dlist_entry tx_list;
	struct dlist_entry rx_list;
	struct dlist_entry unacked;
	struct dlist_entry pending;
	struct dlist_entry buf_ops;
	struct dlist_entry buf_cq;
};

struct rxd_av {
	struct util_av util_av;
	struct fid_av *dg_av;
	struct ofi_rbmap rbmap;
	int tx_idx;

	int dg_av_used;
	size_t dg_addrlen;
	fi_addr_t tx_map[];
};

struct rxd_cq;
typedef int (*rxd_cq_write_fn)(struct rxd_cq *cq,
			       struct fi_cq_tagged_entry *cq_entry);
struct rxd_cq {
	struct util_cq util_cq;
	rxd_cq_write_fn write_fn;
};

struct rxd_ep {
	struct util_ep util_ep;
	struct fid_ep *dg_ep;
	struct fid_cq *dg_cq;

	size_t rx_size;
	size_t tx_size;
	size_t prefix_size;
	uint32_t posted_bufs;
	int do_local_mr;

	struct util_buf_pool *tx_pkt_pool;
	struct util_buf_pool *rx_pkt_pool;
	struct slist rx_pkt_list;

	struct rxd_x_fs *tx_fs;
	struct rxd_x_fs *rx_fs;

	struct dlist_entry unexp_list;
	struct dlist_entry unexp_tag_list;
	struct dlist_entry rx_list;
	struct dlist_entry rx_tag_list;
	struct dlist_entry active_peers;

	struct rxd_peer peers[];
};

static inline struct rxd_domain *rxd_ep_domain(struct rxd_ep *ep)
{
	return container_of(ep->util_ep.domain, struct rxd_domain, util_domain);
}

static inline struct rxd_av *rxd_ep_av(struct rxd_ep *ep)
{
	return container_of(ep->util_ep.av, struct rxd_av, util_av);
}

static inline struct rxd_cq *rxd_ep_tx_cq(struct rxd_ep *ep)
{
	return container_of(ep->util_ep.tx_cq, struct rxd_cq, util_cq);
}

static inline struct rxd_cq *rxd_ep_rx_cq(struct rxd_ep *ep)
{
	return container_of(ep->util_ep.rx_cq, struct rxd_cq, util_cq);
}

struct rxd_x_entry {
	fi_addr_t peer;
	uint32_t tx_id;
	uint32_t rx_id;
	uint32_t msg_id;
	uint64_t bytes_done;
	uint32_t next_seg_no;
	uint32_t start_seq;
	uint32_t window;
	uint32_t num_segs;
	uint32_t op;

	uint32_t flags;
	uint64_t ignore;
	uint8_t iov_count;
	struct iovec iov[RXD_IOV_LIMIT];

	struct fi_cq_tagged_entry cq_entry;

	struct dlist_entry entry;
};
DECLARE_FREESTACK(struct rxd_x_entry, rxd_x_fs);

#define rxd_ep_rx_flags(rxd_ep) ((rxd_ep)->util_ep.rx_op_flags)
#define rxd_ep_tx_flags(rxd_ep) ((rxd_ep)->util_ep.tx_op_flags)


enum rxd_msg_type {
	RXD_MSG			= ofi_op_msg,
	RXD_TAGGED		= ofi_op_tagged,
	RXD_READ_REQ		= ofi_op_read_req,
	RXD_READ		= ofi_op_read_rsp,
	RXD_WRITE		= ofi_op_write,
	RXD_ATOMIC		= ofi_op_atomic,
	RXD_ATOMIC_FETCH	= ofi_op_atomic_fetch,
	RXD_ATOMIC_COMPARE	= ofi_op_atomic_compare,
	RXD_RTS,
	RXD_CTS,
	RXD_ACK,
	RXD_DATA,
	RXD_NO_OP,
};

struct rxd_base_hdr {
	uint32_t version;
	uint32_t type;
};

struct rxd_pkt_hdr {
	uint32_t	flags;
	uint32_t	tx_id;
	uint32_t	rx_id;
	uint32_t	msg_id;
	uint32_t	seg_no;
	uint32_t	seq_no;
	fi_addr_t	peer;
};

struct rxd_rts_pkt {
	struct rxd_base_hdr	base_hdr;
	uint64_t		dg_addr;
	uint8_t			source[RXD_NAME_LENGTH];
};

struct rxd_cts_pkt {
	struct	rxd_base_hdr	base_hdr;
	uint64_t		dg_addr;
	uint64_t		peer_addr;
};

struct rxd_ack_pkt {
	struct rxd_base_hdr	base_hdr;
	struct rxd_pkt_hdr	pkt_hdr;
	//TODO fill in more fields? Selective ack?
};

struct rxd_op_pkt {
	struct rxd_base_hdr	base_hdr;
	struct rxd_pkt_hdr	pkt_hdr;

	uint64_t		num_segs;
	uint64_t		tag;
	uint64_t		cq_data;
	uint64_t		size;

	char			msg[];
}; 

struct rxd_data_pkt {
	struct rxd_base_hdr	base_hdr;
	struct rxd_pkt_hdr	pkt_hdr;

	char			msg[];
};

struct rxd_pkt_entry {
	struct dlist_entry d_entry;
	struct slist_entry s_entry;//TODO - keep both or make separate tx/rx pkt structs
	size_t pkt_size;
	uint64_t retry_time;
	uint8_t retry_cnt;
	struct fi_context context;
	struct fid_mr *mr;
	fi_addr_t peer;
	void *pkt;
};

static inline int rxd_pkt_type(struct rxd_pkt_entry *pkt_entry)
{
	return ((struct rxd_base_hdr *) (pkt_entry->pkt))->type;
}

static inline struct rxd_pkt_hdr *rxd_get_pkt_hdr(struct rxd_pkt_entry *pkt_entry)
{
	return &((struct rxd_ack_pkt *) (pkt_entry->pkt))->pkt_hdr;
}

static inline void rxd_set_pkt(struct rxd_ep *ep, struct rxd_pkt_entry *pkt_entry)
{
	pkt_entry->pkt = (void *) ((char *) pkt_entry +
			  sizeof(*pkt_entry) + ep->prefix_size);
}

static inline void *rxd_pkt_start(struct rxd_pkt_entry *pkt_entry)
{
	return (void *) ((char *) pkt_entry + sizeof(*pkt_entry));
}

static inline int rxd_match_addr(fi_addr_t addr, fi_addr_t match_addr)
{
	return (addr == FI_ADDR_UNSPEC || addr == match_addr);
}

static inline int rxd_match_tag(uint64_t tag, uint64_t ignore, uint64_t match_tag)
{
	return ((tag | ignore ) == (match_tag | ignore));
}

int rxd_info_to_core(uint32_t version, const struct fi_info *rxd_info,
		     struct fi_info *core_info);
int rxd_info_to_rxd(uint32_t version, const struct fi_info *core_info,
		    struct fi_info *info);

int rxd_fabric(struct fi_fabric_attr *attr,
	       struct fid_fabric **fabric, void *context);
int rxd_domain_open(struct fid_fabric *fabric, struct fi_info *info,
		    struct fid_domain **dom, void *context);
int rxd_av_create(struct fid_domain *domain_fid, struct fi_av_attr *attr,
		  struct fid_av **av, void *context);
int rxd_endpoint(struct fid_domain *domain, struct fi_info *info,
		 struct fid_ep **ep, void *context);
int rxd_cq_open(struct fid_domain *domain, struct fi_cq_attr *attr,
		struct fid_cq **cq_fid, void *context);
int rxd_cntr_open(struct fid_domain *domain, struct fi_cntr_attr *attr,
		  struct fid_cntr **cntr_fid, void *context);

/* AV sub-functions */
int rxd_av_insert_dg_addr(struct rxd_av *av, const void *addr,
			  fi_addr_t *dg_fiaddr, uint64_t flags,
			  void *context);
fi_addr_t rxd_av_dg_addr(struct rxd_av *av, fi_addr_t fi_addr);
fi_addr_t rxd_av_fi_addr(struct rxd_av *av, fi_addr_t dg_fiaddr);

/* Pkt resource functions */
int rxd_ep_post_buf(struct rxd_ep *ep);
void rxd_release_repost_rx(struct rxd_ep *ep, struct rxd_pkt_entry *pkt_entry);
void rxd_ep_send_ack(struct rxd_ep *rxd_ep, fi_addr_t peer);
struct rxd_pkt_entry *rxd_get_tx_pkt(struct rxd_ep *ep);
void rxd_release_rx_pkt(struct rxd_ep *ep, struct rxd_pkt_entry *pkt);
void rxd_release_tx_pkt(struct rxd_ep *ep, struct rxd_pkt_entry *pkt);
int rxd_ep_retry_pkt(struct rxd_ep *ep, struct rxd_pkt_entry *pkt_entry);
ssize_t rxd_ep_post_data_pkts(struct rxd_ep *ep, struct rxd_x_entry *tx_entry);

/* Tx/Rx entry sub-functions */
void rxd_tx_entry_free(struct rxd_ep *ep, struct rxd_x_entry *tx_entry);
void rxd_rx_entry_free(struct rxd_ep *ep, struct rxd_x_entry *rx_entry);
void rxd_set_timeout(struct rxd_pkt_entry *pkt_entry);

/* Progress functions */
void rxd_tx_entry_progress(struct rxd_ep *ep, struct rxd_x_entry *tx_entry,
			   int try_send);
void rxd_handle_send_comp(struct rxd_ep *ep, struct fi_cq_msg_entry *comp);
void rxd_handle_recv_comp(struct rxd_ep *ep, struct fi_cq_msg_entry *comp);
void rxd_progress_op(struct rxd_ep *ep, struct rxd_pkt_entry *pkt_entry,
		     struct rxd_x_entry *rx_entry);

/* CQ sub-functions */
void rxd_cq_report_error(struct rxd_cq *cq, struct fi_cq_err_entry *err_entry);
void rxd_cq_report_tx_comp(struct rxd_cq *cq, struct rxd_x_entry *tx_entry);
void rxd_cntr_report_tx_comp(struct rxd_ep *ep, struct rxd_x_entry *tx_entry);

#endif
