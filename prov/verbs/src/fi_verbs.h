/*
 * Copyright (c) 2013-2018 Intel Corporation, Inc.  All rights reserved.
 * Copyright (c) 2016 Cisco Systems, Inc. All rights reserved.
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

#ifndef FI_VERBS_H
#define FI_VERBS_H

#include "config.h"

#include <asm/types.h>
#include <errno.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>
#include <sys/epoll.h>

#include <infiniband/ib.h>
#include <infiniband/verbs.h>
#include <rdma/rdma_cma.h>

#include <rdma/fabric.h>
#include <rdma/fi_cm.h>
#include <rdma/fi_domain.h>
#include <rdma/fi_endpoint.h>
#include <rdma/fi_rma.h>
#include <rdma/fi_errno.h>

#include "ofi.h"
#include "ofi_atomic.h"
#include "ofi_enosys.h"
#include <uthash.h>
#include "ofi_prov.h"
#include "ofi_list.h"
#include "ofi_signal.h"
#include "ofi_util.h"

#ifdef HAVE_VERBS_EXP_H
#include <infiniband/verbs_exp.h>
#endif /* HAVE_VERBS_EXP_H */

#ifndef AF_IB
#define AF_IB 27
#endif

#ifndef RAI_FAMILY
#define RAI_FAMILY              0x00000008
#endif

#define VERBS_PROV_NAME "verbs"
#define VERBS_PROV_VERS FI_VERSION(1,0)

#define VERBS_DBG(subsys, ...) FI_DBG(&fi_ibv_prov, subsys, __VA_ARGS__)
#define VERBS_INFO(subsys, ...) FI_INFO(&fi_ibv_prov, subsys, __VA_ARGS__)
#define VERBS_INFO_ERRNO(subsys, fn, errno) VERBS_INFO(subsys, fn ": %s(%d)\n",	\
		strerror(errno), errno)
#define VERBS_WARN(subsys, ...) FI_WARN(&fi_ibv_prov, subsys, __VA_ARGS__)


#define VERBS_INJECT_FLAGS(ep, len, flags) (((flags & FI_INJECT) || \
		len <= ep->info->tx_attr->inject_size) ? IBV_SEND_INLINE : 0)
#define VERBS_INJECT(ep, len) VERBS_INJECT_FLAGS(ep, len, ep->info->tx_attr->op_flags)

#define VERBS_SELECTIVE_COMP(ep) (ep->ep_flags & FI_SELECTIVE_COMPLETION)
#define VERBS_COMP_FLAGS(ep, flags) ((ep->util_ep.tx_op_flags | flags) &	\
				     FI_COMPLETION ?				\
				     IBV_SEND_SIGNALED : 0)
#define VERBS_COMP(ep) VERBS_COMP_FLAGS(ep, ep->info->tx_attr->op_flags)

#define VERBS_WCE_CNT 1024
#define VERBS_WRE_CNT 1024

#define VERBS_DEF_CQ_SIZE 1024
#define VERBS_MR_IOV_LIMIT 1

#define VERBS_INJECT_FLAG	((uint64_t)-1)

#define VERBS_DGRAM_MSG_PREFIX_SIZE	(40)

#define FI_IBV_EP_TYPE(info)						\
	((info && info->ep_attr) ? info->ep_attr->type : FI_EP_MSG)

#define FI_IBV_MEM_ALIGNMENT (64)
#define FI_IBV_BUF_ALIGNMENT (4096) /* TODO: Page or MTU size */
#define FI_IBV_POOL_BUF_CNT (100)

#define VERBS_ANY_DOMAIN "verbs_any_domain"
#define VERBS_ANY_FABRIC "verbs_any_fabric"

#define FI_IBV_MEMORY_HOOK_BEGIN(notifier)			\
{								\
	pthread_mutex_lock(&notifier->lock);			\
	ofi_set_mem_free_hook(notifier->prev_free_hook);	\
	ofi_set_mem_realloc_hook(notifier->prev_realloc_hook);	\

#define FI_IBV_MEMORY_HOOK_END(notifier)				\
	ofi_set_mem_realloc_hook(fi_ibv_mem_notifier_realloc_hook);	\
	ofi_set_mem_free_hook(fi_ibv_mem_notifier_free_hook);		\
	pthread_mutex_unlock(&notifier->lock);				\
}

extern struct fi_provider fi_ibv_prov;
extern struct util_prov fi_ibv_util_prov;

extern struct fi_ibv_gl_data {
	int	def_tx_size;
	int	def_rx_size;
	int	def_tx_iov_limit;
	int	def_rx_iov_limit;
	int	def_inline_size;
	int	min_rnr_timer;
	int	fork_unsafe;
	int	use_odp;
	int	cqread_bunch_size;
	char	*iface;
	int	mr_cache_enable;
	int	mr_max_cached_cnt;
	size_t	mr_max_cached_size;
	int	mr_cache_merge_regions;

	struct {
		int	buffer_num;
		int	buffer_size;
		int	rndv_seg_size;
		int	thread_timeout;
		char	*eager_send_opcode;
		char	*cm_thread_affinity;
	} rdm;

	struct {
		int	use_name_server;
		int	name_server_port;
	} dgram;
} fi_ibv_gl_data;

struct verbs_addr {
	struct dlist_entry entry;
	struct rdma_addrinfo *rai;
};

/*
 * fields of Infiniband packet headers that are used to
 * represent OFI EP address
 * - LRH (Local Route Header) - Link Layer:
 *   - LID - destination Local Identifier
 *   - SL - Service Level
 * - GRH (Global Route Header) - Network Layer:
 *   - GID - destination Global Identifier
 * - BTH (Base Transport Header) - Transport Layer:
 *   - QPN - destination Queue Pair number
 *   - P_key - Partition Key
 *
 * Note: DON'T change the placement of the fields in the structure.
 *       The placement is to keep structure size = 256 bits (32 byte).
 */
struct ofi_ib_ud_ep_name {
	union ibv_gid	gid;		/* 64-bit GUID + 64-bit EUI - GRH */

	uint32_t	qpn;		/* BTH */

	uint16_t	lid; 		/* LRH */
	uint16_t	pkey;		/* BTH */
	uint16_t	service;	/* for NS src addr, 0 means any */

	uint8_t 	sl;		/* LRH */
	uint8_t		padding[5];	/* forced padding to 256 bits (32 byte) */
}; /* 256 bits */

#define VERBS_IB_UD_NS_ANY_SERVICE	0

static inline
int fi_ibv_dgram_ns_is_service_wildcard(void *svc)
{
	return (*(int *)svc == VERBS_IB_UD_NS_ANY_SERVICE);
}

static inline
int fi_ibv_dgram_ns_service_cmp(void *svc1, void *svc2)
{
	int service1 = *(int *)svc1, service2 = *(int *)svc2;

	if (fi_ibv_dgram_ns_is_service_wildcard(svc1) ||
	    fi_ibv_dgram_ns_is_service_wildcard(svc2))
		return 0;
	return (service1 < service2) ? -1 : (service1 > service2);
}

struct verbs_dev_info {
	struct dlist_entry entry;
	char *name;
	struct dlist_entry addrs;
};

struct fi_ibv_fabric {
	struct util_fabric	util_fabric;
	const struct fi_info	*info;
	struct util_ns		name_server;
};

int fi_ibv_fabric(struct fi_fabric_attr *attr, struct fid_fabric **fabric,
		  void *context);
int fi_ibv_find_fabric(const struct fi_fabric_attr *attr);

struct fi_ibv_eq_entry {
	struct dlist_entry	item;
	uint32_t		event;
	size_t			len;
	char 			eq_entry[0];
};

typedef int (*fi_ibv_trywait_func)(struct fid *fid);

struct fi_ibv_eq {
	struct fid_eq		eq_fid;
	struct fi_ibv_fabric	*fab;
	fastlock_t		lock;
	struct dlistfd_head	list_head;
	struct rdma_event_channel *channel;
	uint64_t		flags;
	struct fi_eq_err_entry	err;
	int			epfd;
};

int fi_ibv_eq_open(struct fid_fabric *fabric, struct fi_eq_attr *attr,
		   struct fid_eq **eq, void *context);

struct fi_ibv_rdm_ep;

typedef struct fi_ibv_rdm_conn *
	(*fi_ibv_rdm_addr_to_conn_func)
	(struct fi_ibv_rdm_ep *ep, fi_addr_t addr);

typedef fi_addr_t
	(*fi_ibv_rdm_conn_to_addr_func)
	(struct fi_ibv_rdm_ep *ep, struct fi_ibv_rdm_conn *conn);

typedef struct fi_ibv_rdm_av_entry *
	(*fi_ibv_rdm_addr_to_av_entry_func)
	(struct fi_ibv_rdm_ep *ep, fi_addr_t addr);

typedef fi_addr_t
	(*fi_ibv_rdm_av_entry_to_addr_func)
	(struct fi_ibv_rdm_ep *ep, struct fi_ibv_rdm_av_entry *av_entry);

struct fi_ibv_av {
	struct fid_av		av_fid;
	struct fi_ibv_domain	*domain;
	struct fi_ibv_rdm_ep	*ep;
	struct fi_ibv_eq	*eq;
	size_t			count;
	size_t			used;
	uint64_t		flags;
	enum fi_av_type		type;
	fi_ibv_rdm_addr_to_conn_func addr_to_conn;
	fi_ibv_rdm_conn_to_addr_func conn_to_addr;
	fi_ibv_rdm_addr_to_av_entry_func addr_to_av_entry;
	fi_ibv_rdm_av_entry_to_addr_func av_entry_to_addr;
};

int fi_ibv_av_open(struct fid_domain *domain, struct fi_av_attr *attr,
		   struct fid_av **av, void *context);
struct fi_ops_av *fi_ibv_rdm_set_av_ops(void);

struct fi_ibv_pep {
	struct fid_pep		pep_fid;
	struct fi_ibv_eq	*eq;
	struct rdma_cm_id	*id;
	int			backlog;
	int			bound;
	size_t			src_addrlen;
	struct fi_info		*info;
};

struct fi_ops_cm *fi_ibv_pep_ops_cm(struct fi_ibv_pep *pep);
struct fi_ibv_rdm_cm;

struct fi_ibv_mem_desc;
typedef int(*fi_ibv_mr_reg_cb)(struct fi_ibv_domain *domain, void *buf,
			       size_t len, uint64_t access,
			       struct fi_ibv_mem_desc *md);
typedef int(*fi_ibv_mr_dereg_cb)(struct fi_ibv_mem_desc *md);

struct fi_ibv_mem_notifier;

struct fi_ibv_domain {
	struct util_domain		util_domain;
	struct ibv_context		*verbs;
	struct ibv_pd			*pd;
	/*
	 * TODO: Currently, only 1 rdm EP can be created per rdm domain!
	 *	 CM logic should be separated from EP,
	 *	 excluding naming/addressing
	 */
	enum fi_ep_type			ep_type;
	struct fi_ibv_rdm_cm		*rdm_cm;
	struct slist			ep_list;
	struct fi_info			*info;
	/* This EQ is utilized by verbs/RDM and verbs/DGRAM */
	struct fi_ibv_eq		*eq;
	uint64_t			eq_flags;

	/* MR stuff */
	int				use_odp;
	struct ofi_mr_cache		cache;
	struct ofi_mem_monitor		monitor;
	fi_ibv_mr_reg_cb		internal_mr_reg;
	fi_ibv_mr_dereg_cb		internal_mr_dereg;
	struct fi_ibv_mem_notifier	*notifier;
};

struct fi_ibv_cq;
typedef void (*fi_ibv_cq_read_entry)(struct ibv_wc *wc, void *buf);

struct fi_ibv_wce {
	struct slist_entry	entry;
	struct ibv_wc		wc;
};

struct fi_ibv_cq {
	struct util_cq		util_cq;
	struct ibv_comp_channel	*channel;
	struct ibv_cq		*cq;
	size_t			entry_size;
	uint64_t		flags;
	enum fi_cq_wait_cond	wait_cond;
	struct ibv_wc		wc;
	int			signal_fd[2];
	fi_ibv_cq_read_entry	read_entry;
	struct slist		wcq;
	fi_ibv_trywait_func	trywait;
	ofi_atomic32_t		nevents;
	struct util_buf_pool	*wce_pool;
};

struct fi_ibv_rdm_request;
typedef void (*fi_ibv_rdm_cq_read_entry)(struct fi_ibv_rdm_request *cq_entry,
					 int index, void *buf);

struct fi_ibv_rdm_cq {
	struct fid_cq			cq_fid;
	struct fi_ibv_domain		*domain;
	struct fi_ibv_rdm_ep		*ep;
	struct dlist_entry		request_cq;
	struct dlist_entry		request_errcq;
	uint64_t			flags;
	size_t				entry_size;
	fi_ibv_rdm_cq_read_entry	read_entry;
	int				read_bunch_size;
	enum fi_cq_wait_cond		wait_cond;
};

int fi_ibv_cq_open(struct fid_domain *domain, struct fi_cq_attr *attr,
		   struct fid_cq **cq, void *context);

struct fi_ibv_mem_desc {
	struct fid_mr		mr_fid;
	struct ibv_mr		*mr;
	struct fi_ibv_domain	*domain;
	size_t			len;
	/* this field is used only by MR cache operations */
	struct ofi_mr_entry	*entry;
};

int fi_ibv_rdm_alloc_and_reg(struct fi_ibv_rdm_ep *ep,
			     void **buf, size_t size,
			     struct fi_ibv_mem_desc *md);
ssize_t fi_ibv_rdm_dereg_and_free(struct fi_ibv_mem_desc *md,
				  char **buff);

static inline uint64_t
fi_ibv_mr_internal_rkey(struct fi_ibv_mem_desc *md)
{
	return md->mr->rkey;
}

static inline uint64_t
fi_ibv_mr_internal_lkey(struct fi_ibv_mem_desc *md)
{
	return md->mr->lkey;
}

struct fi_ibv_mr_internal_ops {
	struct fi_ops_mr	*fi_ops;
	fi_ibv_mr_reg_cb	internal_mr_reg;
	fi_ibv_mr_dereg_cb	internal_mr_dereg;
};

struct fi_ibv_mem_ptr_entry {
	struct dlist_entry	entry;
	void			*addr;
	struct ofi_subscription *subscription;
	UT_hash_handle		hh;
};

struct fi_ibv_mem_notifier {
	struct fi_ibv_mem_ptr_entry	*mem_ptrs_hash;
	struct util_buf_pool		*mem_ptrs_ent_pool;
	struct dlist_entry		event_list;
	ofi_mem_free_hook		prev_free_hook;
	ofi_mem_realloc_hook		prev_realloc_hook;
	int				ref_cnt;
	pthread_mutex_t			lock;
};

void fi_ibv_mem_notifier_free_hook(void *ptr, const void *caller);
void *fi_ibv_mem_notifier_realloc_hook(void *ptr, size_t size, const void *caller);

extern struct fi_ibv_mr_internal_ops fi_ibv_mr_internal_ops;
extern struct fi_ibv_mr_internal_ops fi_ibv_mr_internal_cache_ops;
extern struct fi_ibv_mr_internal_ops fi_ibv_mr_internal_ex_ops;

int fi_ibv_mr_cache_entry_reg(struct ofi_mr_cache *cache,
			      struct ofi_mr_entry *entry);
void fi_ibv_mr_cache_entry_dereg(struct ofi_mr_cache *cache,
				 struct ofi_mr_entry *entry);
int fi_ibv_monitor_subscribe(struct ofi_mem_monitor *notifier, void *addr,
			     size_t len, struct ofi_subscription *subscription);
void fi_ibv_monitor_unsubscribe(struct ofi_mem_monitor *notifier, void *addr,
				size_t len, struct ofi_subscription *subscription);
struct ofi_subscription *fi_ibv_monitor_get_event(struct ofi_mem_monitor *notifier);

struct fi_ibv_srq_ep {
	struct fid_ep		ep_fid;
	struct ibv_srq		*srq;
};

int fi_ibv_srq_context(struct fid_domain *domain, struct fi_rx_attr *attr,
		       struct fid_ep **rx_ep, void *context);

struct fi_ibv_ep {
	struct util_ep		util_ep;
	struct ibv_qp		*ibv_qp;
	union {
		struct rdma_cm_id	*id;
		struct {
			struct ofi_ib_ud_ep_name	ep_name;
			int				service;
		};
	};
	struct fi_ibv_eq	*eq;
	struct fi_ibv_srq_ep	*srq_ep;
	struct fi_info		*info;

	struct {
		struct ibv_send_wr	rma_wr;
		struct ibv_send_wr	msg_wr;
		struct ibv_sge		sge;	
	} *wrs;
};

int fi_ibv_open_ep(struct fid_domain *domain, struct fi_info *info,
		   struct fid_ep **ep, void *context);
int fi_ibv_passive_ep(struct fid_fabric *fabric, struct fi_info *info,
		      struct fid_pep **pep, void *context);
int fi_ibv_rdm_open_ep(struct fid_domain *domain, struct fi_info *info,
			struct fid_ep **ep, void *context);
int fi_ibv_create_ep(const char *node, const char *service,
		     uint64_t flags, const struct fi_info *hints,
		     struct rdma_addrinfo **rai, struct rdma_cm_id **id);
void fi_ibv_destroy_ep(struct rdma_addrinfo *rai, struct rdma_cm_id **id);
int fi_rbv_rdm_cntr_open(struct fid_domain *domain, struct fi_cntr_attr *attr,
			struct fid_cntr **cntr, void *context);
int fi_ibv_rdm_av_open(struct fid_domain *domain, struct fi_av_attr *attr,
		       struct fid_av **av_fid, void *context);
int fi_ibv_dgram_av_open(struct fid_domain *domain_fid, struct fi_av_attr *attr,
			 struct fid_av **av_fid, void *context);

struct fi_ops_atomic fi_ibv_msg_ep_atomic_ops;
struct fi_ops_cm fi_ibv_msg_ep_cm_ops;
struct fi_ops_msg fi_ibv_msg_ep_msg_ops_ts;
struct fi_ops_msg fi_ibv_msg_ep_msg_ops;
struct fi_ops_rma fi_ibv_msg_ep_rma_ops_ts;
struct fi_ops_rma fi_ibv_msg_ep_rma_ops;
struct fi_ops_msg fi_ibv_msg_srq_ep_msg_ops;

struct fi_ibv_connreq {
	struct fid		handle;
	struct rdma_cm_id	*id;
};

int fi_ibv_sockaddr_len(struct sockaddr *addr);


int fi_ibv_init_info(const struct fi_info **all_infos);
int fi_ibv_getinfo(uint32_t version, const char *node, const char *service,
		   uint64_t flags, const struct fi_info *hints,
		   struct fi_info **info);
const struct fi_info *fi_ibv_get_verbs_info(const struct fi_info *ilist,
					    const char *domain_name);
int fi_ibv_fi_to_rai(const struct fi_info *fi, uint64_t flags,
		     struct rdma_addrinfo *rai);
int fi_ibv_get_rdma_rai(const char *node, const char *service, uint64_t flags,
			const struct fi_info *hints, struct rdma_addrinfo **rai);
int fi_ibv_rdm_cm_bind_ep(struct fi_ibv_rdm_cm *cm, struct fi_ibv_rdm_ep *ep);

struct verbs_ep_domain {
	char			*suffix;
	enum fi_ep_type		type;
	uint64_t		caps;
};

extern const struct verbs_ep_domain verbs_rdm_domain;
extern const struct verbs_ep_domain verbs_dgram_domain;

int fi_ibv_check_ep_attr(const struct fi_info *hints,
			 const struct fi_info *info);
int fi_ibv_check_rx_attr(const struct fi_rx_attr *attr,
			 const struct fi_info *hints,
			 const struct fi_info *info);

int fi_ibv_cq_signal(struct fid_cq *cq);

ssize_t fi_ibv_eq_write_event(struct fi_ibv_eq *eq, uint32_t event,
		const void *buf, size_t len);

int fi_ibv_query_atomic(struct fid_domain *domain_fid, enum fi_datatype datatype,
			enum fi_op op, struct fi_atomic_attr *attr,
			uint64_t flags);
int fi_ibv_set_rnr_timer(struct ibv_qp *qp);
void fi_ibv_cleanup_cq(struct fi_ibv_ep *cur_ep);
int fi_ibv_find_max_inline(struct ibv_pd *pd, struct ibv_context *context,
                           enum ibv_qp_type qp_type);

struct fi_ibv_dgram_av {
	struct util_av util_av;
	struct dlist_entry av_entry_list;
};

struct fi_ibv_dgram_av_entry {
	struct dlist_entry list_entry;
	struct ofi_ib_ud_ep_name addr;
	struct ibv_ah *ah;
};

extern struct fi_ops_msg fi_ibv_dgram_msg_ops;
extern struct fi_ops_msg fi_ibv_dgram_msg_ops_ts;

static inline struct fi_ibv_dgram_av_entry*
fi_ibv_dgram_av_lookup_av_entry(fi_addr_t fi_addr)
{
	return (struct fi_ibv_dgram_av_entry *)fi_addr;
}

/* NOTE:
 * When ibv_post_send/recv returns '-1' it means the following:
 * Deal with non-compliant libibverbs drivers which set errno
 * instead of directly returning the error value
 */
static inline ssize_t fi_ibv_handle_post(ssize_t ret)
{
	switch (ret) {
		case ENOMEM:
			ret = -FI_EAGAIN;
			break;
		case -1:
			ret = (errno == ENOMEM) ? -FI_EAGAIN :
						  -errno;
			break;
		default:
			ret = -ret;
			break;
	}
	return ret;
}

/* Returns 0 if it processes WR entry for which user
 * doesn't request the completion */
static inline int
fi_ibv_process_wc(struct fi_ibv_cq *cq, struct ibv_wc *wc)
{
	return (wc->wr_id == VERBS_INJECT_FLAG) ? 0 : 1;
}

/* Returns 0 and tries read new completions if it processes
 * WR entry for which user doesn't request the completion */
static inline int
fi_ibv_process_wc_poll_new(struct fi_ibv_cq *cq, struct ibv_wc *wc)
{
	if (wc->wr_id == VERBS_INJECT_FLAG) {
		int ret;

		while ((ret = ibv_poll_cq(cq->cq, 1, wc)) > 0) {
			if (wc->wr_id != VERBS_INJECT_FLAG)
				return 1;
		}
		return ret;
	}
	return 1;
}

static inline int fi_ibv_wc_2_wce(struct fi_ibv_cq *cq,
				  struct ibv_wc *wc,
				  struct fi_ibv_wce **wce)

{
	*wce = util_buf_alloc(cq->wce_pool);
	if (OFI_UNLIKELY(!*wce))
		return -FI_ENOMEM;
	memset(*wce, 0, sizeof(**wce));
	(*wce)->wc = *wc;

	return FI_SUCCESS;
}

#define fi_ibv_init_sge(buf, len, desc) (struct ibv_sge)		\
	{ .addr = (uintptr_t)buf,					\
	  .length = (uint32_t)len,					\
	  .lkey = (uint32_t)(uintptr_t)desc }

#define fi_ibv_set_sge_iov(sg_list, iov, count, desc)	\
({							\
	size_t i;					\
	sg_list = alloca(sizeof(*sg_list) * count);	\
	for (i = 0; i < count; i++) {			\
		sg_list[i] = fi_ibv_init_sge(		\
				iov[i].iov_base,	\
				iov[i].iov_len,		\
				desc[i]);		\
	}						\
})

#define fi_ibv_set_sge_iov_count_len(sg_list, iov, count, desc, len)	\
({									\
	size_t i;							\
	sg_list = alloca(sizeof(*sg_list) * count);			\
	for (i = 0; i < count; i++) {					\
		sg_list[i] = fi_ibv_init_sge(				\
				iov[i].iov_base,			\
				iov[i].iov_len,				\
				desc[i]);				\
		len += iov[i].iov_len;					\
	}								\
})

#define fi_ibv_init_sge_inline(buf, len) fi_ibv_init_sge(buf, len, NULL)

#define fi_ibv_set_sge_iov_inline(sg_list, iov, count, len)	\
({								\
	size_t i;						\
	sg_list = alloca(sizeof(*sg_list) * count);		\
	for (i = 0; i < count; i++) {				\
		sg_list[i] = fi_ibv_init_sge_inline(		\
					iov[i].iov_base,	\
					iov[i].iov_len);	\
		len += iov[i].iov_len;				\
	}							\
})

#define fi_ibv_send_iov(ep, wr, iov, desc, count)		\
	fi_ibv_send_iov_flags(ep, wr, iov, desc, count,		\
			      (ep)->info->tx_attr->op_flags)

#define fi_ibv_send_msg(ep, wr, msg, flags)				\
	fi_ibv_send_iov_flags(ep, wr, (msg)->msg_iov, (msg)->desc,	\
			      (msg)->iov_count, flags)


static inline int fi_ibv_poll_reap_unsig_cq(struct fi_ibv_ep *ep)
{
	struct fi_ibv_wce *wce;
	struct ibv_wc wc[10];
	int ret, i;
	struct fi_ibv_cq *cq =
		container_of(ep->util_ep.tx_cq, struct fi_ibv_cq, util_cq);

	cq->util_cq.cq_fastlock_acquire(&cq->util_cq.cq_lock);
	/* TODO: retrieve WCs as much as possible in a single
	 * ibv_poll_cq call */
	while (1) {
		ret = ibv_poll_cq(cq->cq, 10, wc);
		if (ret <= 0) {
			cq->util_cq.cq_fastlock_release(&cq->util_cq.cq_lock);
			return ret;
		}

		for (i = 0; i < ret; i++) {
			if (!fi_ibv_process_wc(cq, &wc[i]) ||
			    OFI_UNLIKELY(wc[i].status == IBV_WC_WR_FLUSH_ERR))
				continue;
			if (OFI_LIKELY(!fi_ibv_wc_2_wce(cq, &wc[i], &wce)))
				slist_insert_tail(&wce->entry, &cq->wcq);
		}
	}

	cq->util_cq.cq_fastlock_release(&cq->util_cq.cq_lock);
	return FI_SUCCESS;
}

/* WR must be filled out by now except for context */
static inline ssize_t
fi_ibv_send_poll_cq_if_needed(struct fi_ibv_ep *ep, struct ibv_send_wr *wr)
{
	int ret;
	struct ibv_send_wr *bad_wr;

	ret = ibv_post_send(ep->ibv_qp, wr, &bad_wr);
	if (OFI_UNLIKELY(ret)) {
		ret = fi_ibv_handle_post(ret);
		if (OFI_LIKELY(ret == -FI_EAGAIN)) {
			ret = fi_ibv_poll_reap_unsig_cq(ep);
			if (OFI_UNLIKELY(ret))
				return -FI_EAGAIN;
			/* Try again and return control to a caller */
			ret = fi_ibv_handle_post(ibv_post_send(ep->ibv_qp, wr,
							       &bad_wr));
		}
	}
	return ret;
}

static inline ssize_t
fi_ibv_send_buf(struct fi_ibv_ep *ep, struct ibv_send_wr *wr,
		const void *buf, size_t len, void *desc)
{
	struct ibv_sge sge = fi_ibv_init_sge(buf, len, desc);

	assert(wr->wr_id != VERBS_INJECT_FLAG);

	wr->sg_list = &sge;
	wr->num_sge = 1;

	return fi_ibv_send_poll_cq_if_needed(ep, wr);
}

static inline ssize_t
fi_ibv_send_buf_inline(struct fi_ibv_ep *ep, struct ibv_send_wr *wr,
		       const void *buf, size_t len)
{
	struct ibv_sge sge = fi_ibv_init_sge_inline(buf, len);

	assert(wr->wr_id == VERBS_INJECT_FLAG);

	wr->sg_list = &sge;
	wr->num_sge = 1;

	return fi_ibv_send_poll_cq_if_needed(ep, wr);
}

static inline ssize_t
fi_ibv_send_iov_flags(struct fi_ibv_ep *ep, struct ibv_send_wr *wr,
		      const struct iovec *iov, void **desc, int count,
		      uint64_t flags)
{
	size_t len = 0;

	if (!desc)
		fi_ibv_set_sge_iov_inline(wr->sg_list, iov, count, len);
	else
		fi_ibv_set_sge_iov_count_len(wr->sg_list, iov, count, desc, len);

	wr->num_sge = count;
	wr->send_flags = VERBS_INJECT_FLAGS(ep, len, flags) | VERBS_COMP_FLAGS(ep, flags);

	if (flags & FI_FENCE)
		wr->send_flags |= IBV_SEND_FENCE;

	return fi_ibv_send_poll_cq_if_needed(ep, wr);
}

static inline const void *
fi_ibv_dgram_moderate_user_buf(const void *user_buf, size_t offset)
{
	return (const void *)((const char *)user_buf + offset);
}

static inline size_t
fi_ibv_dgram_moderate_user_buf_len(size_t user_buf_len, size_t offset)
{
	return user_buf_len - offset;
}

static inline int
fi_ibv_dgram_moderate_user_iov(const struct iovec *user_iov, void **desc,
			       int count, size_t offset, struct iovec *new_iov,
			       void **new_desc)
{
	int i, new_count = 0;

	for (i = 0; i < count; i++) {
		if (offset > user_iov[i].iov_len) {
			offset -= user_iov[i].iov_len;
			continue;
		}
		new_iov[new_count].iov_base = (char *)user_iov[i].iov_base + offset;
		new_iov[new_count].iov_len = user_iov[i].iov_len - offset;
		if (desc)
			new_desc[new_count] = desc[i];
		new_count++;

		offset = 0;
	}

	return new_count;
}

static inline ssize_t
fi_ibv_dgram_send_buf(struct fi_ibv_ep *ep, struct ibv_send_wr *wr,
		      const void *buf, size_t len, void *desc)
{
	return fi_ibv_send_buf(
			ep, wr, fi_ibv_dgram_moderate_user_buf(
					buf, ep->info->ep_attr->msg_prefix_size),
			fi_ibv_dgram_moderate_user_buf_len(
					len, ep->info->ep_attr->msg_prefix_size),
			desc);
}

static inline ssize_t
fi_ibv_dgram_send_buf_inline(struct fi_ibv_ep *ep, struct ibv_send_wr *wr,
			     const void *buf, size_t len)
{
	return fi_ibv_send_buf_inline(
			ep, wr, fi_ibv_dgram_moderate_user_buf(
					buf, ep->info->ep_attr->msg_prefix_size),
			fi_ibv_dgram_moderate_user_buf_len(
				len, ep->info->ep_attr->msg_prefix_size));
}

static inline ssize_t
fi_ibv_dgram_send_iov(struct fi_ibv_ep *ep, struct ibv_send_wr *wr,
		      const struct iovec *iov, void **desc, int count)
{
	struct iovec *new_iov = alloca(sizeof(*new_iov) * count);
	void **new_desc = alloca(sizeof(*new_desc) * count);

	count = fi_ibv_dgram_moderate_user_iov(iov, desc, count,
					       ep->info->ep_attr->msg_prefix_size,
					       new_iov, new_desc);
	return fi_ibv_send_iov(ep, wr, new_iov, new_desc, count);
}

static inline ssize_t
fi_ibv_dgram_send_msg(struct fi_ibv_ep *ep, struct ibv_send_wr *wr,
		      const struct fi_msg *msg, uint64_t flags)
{
	struct fi_msg new_msg = *msg;
	struct iovec *new_iov = alloca(sizeof(*new_iov) * msg->iov_count);
	void **new_desc = alloca(sizeof(*new_desc) * msg->iov_count);

	new_msg.iov_count = fi_ibv_dgram_moderate_user_iov(
				msg->msg_iov, msg->desc, msg->iov_count,
				ep->info->ep_attr->msg_prefix_size,
				new_iov, new_desc);
	new_msg.msg_iov = new_iov;
	new_msg.desc = new_desc;
	return fi_ibv_send_msg(ep, wr, &new_msg, flags);
}

#endif /* FI_VERBS_H */
