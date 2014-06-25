/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

/*
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"@(#)xgell.c	1.10	06/06/29 SMI"

/*
 *  Copyright (c) 2002-2005 Neterion, Inc.
 *  All right Reserved.
 *
 *  FileName :    xgell.c
 *
 *  Description:  Xge Link Layer data path implementation
 *
 */

#include "xgell.h"

#include <netinet/ip.h>
#include <netinet/tcp.h>

#define	XGELL_MAX_FRAME_SIZE(hldev)	((hldev)->config.mtu +	\
    sizeof (struct ether_vlan_header))

#define	HEADROOM		2	/* for DIX-only packets */

#ifdef XGELL_L3_ALIGNED
void header_free_func(void *arg) { }
frtn_t header_frtn = {header_free_func, NULL};
#endif

/* DMA attributes used for Tx side */
static struct ddi_dma_attr tx_dma_attr = {
	DMA_ATTR_V0,			/* dma_attr_version */
	0x0ULL,				/* dma_attr_addr_lo */
	0xFFFFFFFFFFFFFFFFULL,		/* dma_attr_addr_hi */
	0xFFFFFFFFULL,			/* dma_attr_count_max */
	0x1ULL,				/* dma_attr_align */
	0xFFF,				/* dma_attr_burstsizes */
	1,				/* dma_attr_minxfer */
	0xFFFFFFFFULL,			/* dma_attr_maxxfer */
	0xFFFFFFFFFFFFFFFFULL,		/* dma_attr_seg */
	4,				/* dma_attr_sgllen */
	1,				/* dma_attr_granular */
	0				/* dma_attr_flags */
};

/* Aligned DMA attributes used for Tx side */
struct ddi_dma_attr tx_dma_attr_align = {
	DMA_ATTR_V0,			/* dma_attr_version */
	0x0ULL,				/* dma_attr_addr_lo */
	0xFFFFFFFFFFFFFFFFULL,		/* dma_attr_addr_hi */
	0xFFFFFFFFULL,			/* dma_attr_count_max */
	4096,				/* dma_attr_align */
	0xFFF,				/* dma_attr_burstsizes */
	1,				/* dma_attr_minxfer */
	0xFFFFFFFFULL,			/* dma_attr_maxxfer */
	0xFFFFFFFFFFFFFFFFULL,		/* dma_attr_seg */
	4,				/* dma_attr_sgllen */
	1,				/* dma_attr_granular */
	0				/* dma_attr_flags */
};

/*
 * DMA attributes used when using ddi_dma_mem_alloc to
 * allocat HAL descriptors and Rx buffers during replenish
 */
static struct ddi_dma_attr hal_dma_attr = {
	DMA_ATTR_V0,			/* dma_attr_version */
	0x0ULL,				/* dma_attr_addr_lo */
	0xFFFFFFFFFFFFFFFFULL,		/* dma_attr_addr_hi */
	0xFFFFFFFFULL,			/* dma_attr_count_max */
	0x1ULL,				/* dma_attr_align */
	0xFFF,				/* dma_attr_burstsizes */
	1,				/* dma_attr_minxfer */
	0xFFFFFFFFULL,			/* dma_attr_maxxfer */
	0xFFFFFFFFFFFFFFFFULL,		/* dma_attr_seg */
	1,				/* dma_attr_sgllen */
	1,				/* dma_attr_granular */
	0				/* dma_attr_flags */
};

/*
 * Aligned DMA attributes used when using ddi_dma_mem_alloc to
 * allocat HAL descriptors and Rx buffers during replenish
 */
struct ddi_dma_attr hal_dma_attr_aligned = {
	DMA_ATTR_V0,			/* dma_attr_version */
	0x0ULL,				/* dma_attr_addr_lo */
	0xFFFFFFFFFFFFFFFFULL,		/* dma_attr_addr_hi */
	0xFFFFFFFFULL,			/* dma_attr_count_max */
	4096,				/* dma_attr_align */
	0xFFF,				/* dma_attr_burstsizes */
	1,				/* dma_attr_minxfer */
	0xFFFFFFFFULL,			/* dma_attr_maxxfer */
	0xFFFFFFFFFFFFFFFFULL,		/* dma_attr_seg */
	1,				/* dma_attr_sgllen */
	1,				/* dma_attr_granular */
	0				/* dma_attr_flags */
};

struct ddi_dma_attr *p_hal_dma_attr = &hal_dma_attr;
struct ddi_dma_attr *p_hal_dma_attr_aligned = &hal_dma_attr_aligned;

static int		xgell_m_stat(void *, uint_t, uint64_t *);
static int		xgell_m_start(void *);
static void		xgell_m_stop(void *);
static int		xgell_m_promisc(void *, boolean_t);
static int		xgell_m_multicst(void *, boolean_t, const uint8_t *);
static int		xgell_m_unicst(void *, const uint8_t *);
static void		xgell_m_ioctl(void *, queue_t *, mblk_t *);
static mblk_t 		*xgell_m_tx(void *, mblk_t *);
static boolean_t	xgell_m_getcapab(void *, mac_capab_t, void *);

#define	XGELL_M_CALLBACK_FLAGS	(MC_IOCTL | MC_GETCAPAB)

static mac_callbacks_t xgell_m_callbacks = {
	XGELL_M_CALLBACK_FLAGS,
	xgell_m_stat,
	xgell_m_start,
	xgell_m_stop,
	xgell_m_promisc,
	xgell_m_multicst,
	xgell_m_unicst,
	xgell_m_tx,
	NULL,
	xgell_m_ioctl,
	xgell_m_getcapab
};

/*
 * xge_device_poll
 *
 * Cyclic should call me every 1s. xge_callback_event_queued should call me
 * when HAL hope event was rescheduled.
 */
/*ARGSUSED*/
void
xge_device_poll(void *data)
{
	xgelldev_t *lldev = xge_hal_device_private(data);

	mutex_enter(&lldev->genlock);
	if (lldev->is_initialized) {
		xge_hal_device_poll(data);
		lldev->timeout_id = timeout(xge_device_poll, data,
		    XGE_DEV_POLL_TICKS);
	}
	mutex_exit(&lldev->genlock);
}

/*
 * xge_device_poll_now
 *
 * Will call xge_device_poll() immediately
 */
void
xge_device_poll_now(void *data)
{
	xgelldev_t *lldev = xge_hal_device_private(data);

	mutex_enter(&lldev->genlock);
	(void) untimeout(lldev->timeout_id);
	lldev->timeout_id = timeout(xge_device_poll, data, 0);
	mutex_exit(&lldev->genlock);

}

/*
 * xgell_callback_link_up
 *
 * This function called by HAL to notify HW link up state change.
 */
void
xgell_callback_link_up(void *userdata)
{
	xgelldev_t *lldev = (xgelldev_t *)userdata;

	mac_link_update(lldev->mh, LINK_STATE_UP);
	/* Link states should be reported to user whenever it changes */
	cmn_err(CE_NOTE, "!%s%d: Link is up [10 Gbps Full Duplex]",
	    XGELL_IFNAME, lldev->instance);
}

/*
 * xgell_callback_link_down
 *
 * This function called by HAL to notify HW link down state change.
 */
void
xgell_callback_link_down(void *userdata)
{
	xgelldev_t *lldev = (xgelldev_t *)userdata;

	mac_link_update(lldev->mh, LINK_STATE_DOWN);
	/* Link states should be reported to user whenever it changes */
	cmn_err(CE_NOTE, "!%s%d: Link is down", XGELL_IFNAME,
	    lldev->instance);
}

/*
 * xgell_rx_buffer_replenish_all
 *
 * To replenish all freed dtr(s) with buffers in free pool. It's called by
 * xgell_rx_buffer_recycle() or xgell_rx_1b_compl().
 * Must be called with pool_lock held.
 */
static void
xgell_rx_buffer_replenish_all(xgelldev_t *lldev)
{
	xge_hal_dtr_h dtr;
	xgell_rx_buffer_t *rx_buffer;
	xgell_rxd_priv_t *rxd_priv;

	while ((lldev->bf_pool.free > 0) &&
	    (xge_hal_ring_dtr_reserve(lldev->ring_main.channelh, &dtr) ==
	    XGE_HAL_OK)) {
		rx_buffer = lldev->bf_pool.head;
		lldev->bf_pool.head = rx_buffer->next;
		lldev->bf_pool.free--;

		xge_assert(rx_buffer);
		xge_assert(rx_buffer->dma_addr);

		rxd_priv = (xgell_rxd_priv_t *)
		    xge_hal_ring_dtr_private(lldev->ring_main.channelh, dtr);
		xge_hal_ring_dtr_1b_set(dtr, rx_buffer->dma_addr,
		    lldev->bf_pool.size);

		rxd_priv->rx_buffer = rx_buffer;
		xge_hal_ring_dtr_post(lldev->ring_main.channelh, dtr);
	}
}

/*
 * xgell_rx_buffer_release
 *
 * The only thing done here is to put the buffer back to the pool.
 */
static void
xgell_rx_buffer_release(xgell_rx_buffer_t *rx_buffer)
{
	xgelldev_t *lldev = rx_buffer->lldev;

	mutex_enter(&lldev->bf_pool.pool_lock);

	/* Put the buffer back to pool */
	rx_buffer->next = lldev->bf_pool.head;
	lldev->bf_pool.head = rx_buffer;

	lldev->bf_pool.free++;

	mutex_exit(&lldev->bf_pool.pool_lock);
}

/*
 * xgell_rx_buffer_recycle
 *
 * Called by desballoc() to "free" the resource.
 * We will try to replenish all descripters.
 */
static void
xgell_rx_buffer_recycle(char *arg)
{
	xgell_rx_buffer_t *rx_buffer = (xgell_rx_buffer_t *)arg;
	xgelldev_t *lldev = rx_buffer->lldev;

	xgell_rx_buffer_release(rx_buffer);

	mutex_enter(&lldev->bf_pool.pool_lock);
	lldev->bf_pool.post--;

	/*
	 * Before finding a good way to set this hiwat, just always call to
	 * replenish_all. *TODO*
	 */
	if (lldev->is_initialized != 0) {
		xgell_rx_buffer_replenish_all(lldev);
	}

	mutex_exit(&lldev->bf_pool.pool_lock);
}

/*
 * xgell_rx_buffer_alloc
 *
 * Allocate one rx buffer and return with the pointer to the buffer.
 * Return NULL if failed.
 */
static xgell_rx_buffer_t *
xgell_rx_buffer_alloc(xgelldev_t *lldev)
{
	xge_hal_device_t *hldev;
	void *vaddr;
	ddi_dma_handle_t dma_handle;
	ddi_acc_handle_t dma_acch;
	dma_addr_t dma_addr;
	uint_t ncookies;
	ddi_dma_cookie_t dma_cookie;
	size_t real_size;
	extern ddi_device_acc_attr_t *p_xge_dev_attr;
	xgell_rx_buffer_t *rx_buffer;

	hldev = lldev->devh;

	if (ddi_dma_alloc_handle(hldev->pdev, p_hal_dma_attr, DDI_DMA_SLEEP,
	    0, &dma_handle) != DDI_SUCCESS) {
		xge_debug_ll(XGE_ERR, "%s%d: can not allocate DMA handle",
		    XGELL_IFNAME, lldev->instance);
		goto handle_failed;
	}

	/* reserve some space at the end of the buffer for recycling */
	if (ddi_dma_mem_alloc(dma_handle, HEADROOM + lldev->bf_pool.size +
	    sizeof (xgell_rx_buffer_t), p_xge_dev_attr, DDI_DMA_STREAMING,
	    DDI_DMA_SLEEP, 0, (caddr_t *)&vaddr, &real_size, &dma_acch) !=
	    DDI_SUCCESS) {
		xge_debug_ll(XGE_ERR, "%s%d: can not allocate DMA-able memory",
		    XGELL_IFNAME, lldev->instance);
		goto mem_failed;
	}

	if (HEADROOM + lldev->bf_pool.size + sizeof (xgell_rx_buffer_t) >
	    real_size) {
		xge_debug_ll(XGE_ERR, "%s%d: can not allocate DMA-able memory",
		    XGELL_IFNAME, lldev->instance);
		goto bind_failed;
	}

	if (ddi_dma_addr_bind_handle(dma_handle, NULL, (char *)vaddr + HEADROOM,
	    lldev->bf_pool.size, DDI_DMA_READ | DDI_DMA_STREAMING,
	    DDI_DMA_SLEEP, 0, &dma_cookie, &ncookies) != DDI_SUCCESS) {
		xge_debug_ll(XGE_ERR, "%s%d: out of mapping for mblk",
		    XGELL_IFNAME, lldev->instance);
		goto bind_failed;
	}

	if (ncookies != 1 || dma_cookie.dmac_size < lldev->bf_pool.size) {
		xge_debug_ll(XGE_ERR, "%s%d: can not handle partial DMA",
		    XGELL_IFNAME, lldev->instance);
		goto check_failed;
	}

	dma_addr = dma_cookie.dmac_laddress;

	rx_buffer = (xgell_rx_buffer_t *)((char *)vaddr + real_size -
	    sizeof (xgell_rx_buffer_t));
	rx_buffer->next = NULL;
	rx_buffer->vaddr = vaddr;
	rx_buffer->dma_addr = dma_addr;
	rx_buffer->dma_handle = dma_handle;
	rx_buffer->dma_acch = dma_acch;
	rx_buffer->lldev = lldev;
	rx_buffer->frtn.free_func = xgell_rx_buffer_recycle;
	rx_buffer->frtn.free_arg = (void *)rx_buffer;

	return (rx_buffer);

check_failed:
	(void) ddi_dma_unbind_handle(dma_handle);
bind_failed:
	XGE_OS_MEMORY_CHECK_FREE(vaddr, 0);
	ddi_dma_mem_free(&dma_acch);
mem_failed:
	ddi_dma_free_handle(&dma_handle);
handle_failed:

	return (NULL);
}

/*
 * xgell_rx_destroy_buffer_pool
 *
 * Destroy buffer pool. If there is still any buffer hold by upper layer,
 * recorded by bf_pool.post, return DDI_FAILURE to reject to be unloaded.
 */
static int
xgell_rx_destroy_buffer_pool(xgelldev_t *lldev)
{
	xgell_rx_buffer_t *rx_buffer;
	ddi_dma_handle_t  dma_handle;
	ddi_acc_handle_t  dma_acch;
	int i;

	/*
	 * If there is any posted buffer, the driver should reject to be
	 * detached. Need notice upper layer to release them.
	 */
	if (lldev->bf_pool.post != 0) {
		xge_debug_ll(XGE_ERR,
		    "%s%d has some buffers not be recycled, try later!",
		    XGELL_IFNAME, lldev->instance);
		return (DDI_FAILURE);
	}

	/*
	 * Relase buffers one by one.
	 */
	for (i = lldev->bf_pool.total; i > 0; i--) {
		rx_buffer = lldev->bf_pool.head;
		xge_assert(rx_buffer != NULL);

		lldev->bf_pool.head = rx_buffer->next;

		dma_handle = rx_buffer->dma_handle;
		dma_acch = rx_buffer->dma_acch;

		if (ddi_dma_unbind_handle(dma_handle) != DDI_SUCCESS) {
			xge_debug_ll(XGE_ERR, "failed to unbind DMA handle!");
			lldev->bf_pool.head = rx_buffer;
			return (DDI_FAILURE);
		}
		ddi_dma_mem_free(&dma_acch);
		ddi_dma_free_handle(&dma_handle);

		lldev->bf_pool.total--;
		lldev->bf_pool.free--;
	}

	mutex_destroy(&lldev->bf_pool.pool_lock);
	return (DDI_SUCCESS);
}

/*
 * xgell_rx_create_buffer_pool
 *
 * Initialize RX buffer pool for all RX rings. Refer to rx_buffer_pool_t.
 */
static int
xgell_rx_create_buffer_pool(xgelldev_t *lldev)
{
	xge_hal_device_t *hldev;
	xgell_rx_buffer_t *rx_buffer;
	int i;

	hldev = (xge_hal_device_t *)lldev->devh;

	lldev->bf_pool.total = 0;
	lldev->bf_pool.size = XGELL_MAX_FRAME_SIZE(hldev);
	lldev->bf_pool.head = NULL;
	lldev->bf_pool.free = 0;
	lldev->bf_pool.post = 0;
	lldev->bf_pool.post_hiwat = lldev->config.rx_buffer_post_hiwat;
	lldev->bf_pool.recycle_hiwat = lldev->config.rx_buffer_recycle_hiwat;

	mutex_init(&lldev->bf_pool.pool_lock, NULL, MUTEX_DRIVER,
	    hldev->irqh);

	/*
	 * Allocate buffers one by one. If failed, destroy whole pool by
	 * call to xgell_rx_destroy_buffer_pool().
	 */
	for (i = 0; i < lldev->config.rx_buffer_total; i++) {
		if ((rx_buffer = xgell_rx_buffer_alloc(lldev)) == NULL) {
			(void) xgell_rx_destroy_buffer_pool(lldev);
			return (DDI_FAILURE);
		}

		rx_buffer->next = lldev->bf_pool.head;
		lldev->bf_pool.head = rx_buffer;

		lldev->bf_pool.total++;
		lldev->bf_pool.free++;
	}

	return (DDI_SUCCESS);
}

/*
 * xgell_rx_dtr_replenish
 *
 * Replenish descriptor with rx_buffer in RX buffer pool.
 * The dtr should be post right away.
 */
xge_hal_status_e
xgell_rx_dtr_replenish(xge_hal_channel_h channelh, xge_hal_dtr_h dtr, int index,
    void *userdata, xge_hal_channel_reopen_e reopen)
{
	xgell_ring_t *ring = userdata;
	xgelldev_t *lldev = ring->lldev;
	xgell_rx_buffer_t *rx_buffer;
	xgell_rxd_priv_t *rxd_priv;

	if (lldev->bf_pool.head == NULL) {
		xge_debug_ll(XGE_ERR, "no more available rx DMA buffer!");
		return (XGE_HAL_FAIL);
	}
	rx_buffer = lldev->bf_pool.head;
	lldev->bf_pool.head = rx_buffer->next;
	lldev->bf_pool.free--;

	xge_assert(rx_buffer);
	xge_assert(rx_buffer->dma_addr);

	rxd_priv = (xgell_rxd_priv_t *)
	    xge_hal_ring_dtr_private(lldev->ring_main.channelh, dtr);
	xge_hal_ring_dtr_1b_set(dtr, rx_buffer->dma_addr, lldev->bf_pool.size);

	rxd_priv->rx_buffer = rx_buffer;

	return (XGE_HAL_OK);
}

/*
 * xgell_get_ip_offset
 *
 * Calculate the offset to IP header.
 */
static inline int
xgell_get_ip_offset(xge_hal_dtr_info_t *ext_info)
{
	int ip_off;

	/* get IP-header offset */
	switch (ext_info->frame) {
	case XGE_HAL_FRAME_TYPE_DIX:
		ip_off = XGE_HAL_HEADER_ETHERNET_II_802_3_SIZE;
		break;
	case XGE_HAL_FRAME_TYPE_IPX:
		ip_off = (XGE_HAL_HEADER_ETHERNET_II_802_3_SIZE +
		    XGE_HAL_HEADER_802_2_SIZE +
		    XGE_HAL_HEADER_SNAP_SIZE);
		break;
	case XGE_HAL_FRAME_TYPE_LLC:
		ip_off = (XGE_HAL_HEADER_ETHERNET_II_802_3_SIZE +
		    XGE_HAL_HEADER_802_2_SIZE);
		break;
	case XGE_HAL_FRAME_TYPE_SNAP:
		ip_off = (XGE_HAL_HEADER_ETHERNET_II_802_3_SIZE +
		    XGE_HAL_HEADER_SNAP_SIZE);
		break;
	default:
		ip_off = 0;
		break;
	}

	if ((ext_info->proto & XGE_HAL_FRAME_PROTO_IPV4 ||
	    ext_info->proto & XGE_HAL_FRAME_PROTO_IPV6) &&
	    (ext_info->proto & XGE_HAL_FRAME_PROTO_VLAN_TAGGED)) {
		ip_off += XGE_HAL_HEADER_VLAN_SIZE;
	}

	return (ip_off);
}

/*
 * xgell_rx_hcksum_assoc
 *
 * Judge the packet type and then call to hcksum_assoc() to associate
 * h/w checksum information.
 */
static inline void
xgell_rx_hcksum_assoc(mblk_t *mp, char *vaddr, int pkt_length,
    xge_hal_dtr_info_t *ext_info)
{
	int cksum_flags = 0;
	int ip_off;

	ip_off = xgell_get_ip_offset(ext_info);

	if (!(ext_info->proto & XGE_HAL_FRAME_PROTO_IP_FRAGMENTED)) {
		if (ext_info->proto & XGE_HAL_FRAME_PROTO_TCP_OR_UDP) {
			if (ext_info->l3_cksum == XGE_HAL_L3_CKSUM_OK) {
				cksum_flags |= HCK_IPV4_HDRCKSUM;
			}
			if (ext_info->l4_cksum == XGE_HAL_L4_CKSUM_OK) {
				cksum_flags |= HCK_FULLCKSUM_OK;
			}
			if (cksum_flags) {
				cksum_flags |= HCK_FULLCKSUM;
				(void) hcksum_assoc(mp, NULL, NULL, 0,
				    0, 0, 0, cksum_flags, 0);
			}
		}
	} else if (ext_info->proto &
	    (XGE_HAL_FRAME_PROTO_IPV4 | XGE_HAL_FRAME_PROTO_IPV6)) {
		/*
		 * Just pass the partial cksum up to IP.
		 */
		int start, end = pkt_length - ip_off;

		if (ext_info->proto & XGE_HAL_FRAME_PROTO_IPV4) {
			struct ip *ip =
			    (struct ip *)(vaddr + ip_off);
			start = ip->ip_hl * 4 + ip_off;
		} else {
			start = ip_off + 40;
		}
		cksum_flags |= HCK_PARTIALCKSUM;
		(void) hcksum_assoc(mp, NULL, NULL, start, 0,
		    end, ntohs(ext_info->l4_cksum), cksum_flags,
		    0);
	}
}

/*
 * xgell_rx_1b_msg_alloc
 *
 * Allocate message header for data buffer, and decide if copy the packet to
 * new data buffer to release big rx_buffer to save memory.
 *
 * If the pkt_length <= XGELL_DMA_BUFFER_SIZE_LOWAT, call allocb() to allocate
 * new message and copy the payload in.
 */
static mblk_t *
xgell_rx_1b_msg_alloc(xgell_rx_buffer_t *rx_buffer, int pkt_length,
    xge_hal_dtr_info_t *ext_info, boolean_t *copyit)
{
	mblk_t *mp;
	mblk_t *nmp = NULL;
	char *vaddr;
	int hdr_length = 0;

#ifdef XGELL_L3_ALIGNED
	int doalign = 1;
	struct ip *ip;
	struct tcphdr *tcp;
	int tcp_off;
	int mp_align_len;
	int ip_off;

#endif

	vaddr = (char *)rx_buffer->vaddr + HEADROOM;
#ifdef XGELL_L3_ALIGNED
	ip_off = xgell_get_ip_offset(ext_info);

	/* Check ip_off with HEADROOM */
	if ((ip_off & 3) == HEADROOM) {
		doalign = 0;
	}

	/*
	 * Doalign? Check for types of packets.
	 */
	/* Is IPv4 or IPv6? */
	if (doalign && !(ext_info->proto & XGE_HAL_FRAME_PROTO_IPV4 ||
	    ext_info->proto & XGE_HAL_FRAME_PROTO_IPV6)) {
		doalign = 0;
	}

	/* Is TCP? */
	if (doalign &&
	    ((ip = (struct ip *)(vaddr + ip_off))->ip_p == IPPROTO_TCP)) {
		tcp_off = ip->ip_hl * 4 + ip_off;
		tcp = (struct tcphdr *)(vaddr + tcp_off);
		hdr_length = tcp_off + tcp->th_off * 4;
		if (pkt_length < (XGE_HAL_TCPIP_HEADER_MAX_SIZE +
		    XGE_HAL_MAC_HEADER_MAX_SIZE)) {
			hdr_length = pkt_length;
		}
	} else {
		doalign = 0;
	}
#endif

	/*
	 * Copy packet into new allocated message buffer, if pkt_length
	 * is less than XGELL_DMA_BUFFER_LOWAT
	 */
	if (*copyit || pkt_length <= XGELL_DMA_BUFFER_SIZE_LOWAT) {
		/* Keep room for alignment */
		if ((mp = allocb(pkt_length + HEADROOM + 4, 0)) == NULL) {
			return (NULL);
		}
#ifdef XGELL_L3_ALIGNED
		if (doalign) {
			mp_align_len =
			    (4 - ((uint64_t)(mp->b_rptr + ip_off) & 3));
			mp->b_rptr += mp_align_len;
		}
#endif
		bcopy(vaddr, mp->b_rptr, pkt_length);
		mp->b_wptr = mp->b_rptr + pkt_length;
		*copyit = B_TRUE;
		return (mp);
	}

	/*
	 * Just allocate mblk for current data buffer
	 */
	if ((nmp = (mblk_t *)desballoc((unsigned char *)vaddr, pkt_length, 0,
	    &rx_buffer->frtn)) == NULL) {
		/* Drop it */
		return (NULL);
	}

	/*
	 * Adjust the b_rptr/b_wptr in the mblk_t structure to point to
	 * payload.
	 */
	nmp->b_rptr += hdr_length;
	nmp->b_wptr += pkt_length;

#ifdef XGELL_L3_ALIGNED
	if (doalign) {
		if ((mp = esballoc(rx_buffer->header, hdr_length + 4, 0,
		    &header_frtn)) == NULL) {
			/* can not align! */
			mp = nmp;
			mp->b_rptr = (u8 *)vaddr;
			mp->b_wptr = mp->b_rptr + pkt_length;
			mp->b_next = NULL;
			mp->b_cont = NULL;
		} else {
			/* align packet's ip-header offset */
			mp_align_len =
			    (4 - ((uint64_t)(mp->b_rptr + ip_off) & 3));
			mp->b_rptr += mp_align_len;
			mp->b_wptr += mp_align_len + hdr_length;
			mp->b_cont = nmp;
			mp->b_next = NULL;
			nmp->b_cont = NULL;
			nmp->b_next = NULL;

			bcopy(vaddr, mp->b_rptr, hdr_length);
		}
	} else {
		/* no need to align */
		mp = nmp;
		mp->b_next = NULL;
		mp->b_cont = NULL;
	}
#else
	mp = nmp;
	mp->b_next = NULL;
	mp->b_cont = NULL;
#endif

	return (mp);
}

/*
 * xgell_rx_1b_compl
 *
 * If the interrupt is because of a received frame or if the receive ring
 * contains fresh as yet un-processed frames, this function is called.
 */
static xge_hal_status_e
xgell_rx_1b_compl(xge_hal_channel_h channelh, xge_hal_dtr_h dtr, u8 t_code,
    void *userdata)
{
	xgelldev_t *lldev = ((xgell_ring_t *)userdata)->lldev;
	xgell_rx_buffer_t *rx_buffer;
	mblk_t *mp_head = NULL;
	mblk_t *mp_end  = NULL;

	do {
		int ret;
		int pkt_length;
		dma_addr_t dma_data;
		mblk_t *mp;

		boolean_t copyit = B_FALSE;

		xgell_rxd_priv_t *rxd_priv = ((xgell_rxd_priv_t *)
		    xge_hal_ring_dtr_private(channelh, dtr));
		xge_hal_dtr_info_t ext_info;

		rx_buffer = rxd_priv->rx_buffer;

		xge_hal_ring_dtr_1b_get(channelh, dtr, &dma_data, &pkt_length);
		xge_hal_ring_dtr_info_get(channelh, dtr, &ext_info);

		xge_assert(dma_data == rx_buffer->dma_addr);

		if (t_code != 0) {
			xge_debug_ll(XGE_ERR, "%s%d: rx: dtr 0x%"PRIx64
			    " completed due to error t_code %01x", XGELL_IFNAME,
			    lldev->instance, (uint64_t)(uintptr_t)dtr, t_code);

			(void) xge_hal_device_handle_tcode(channelh, dtr,
			    t_code);
			xge_hal_ring_dtr_free(channelh, dtr); /* drop it */
			xgell_rx_buffer_release(rx_buffer);
			continue;
		}

		/*
		 * Sync the DMA memory
		 */
		ret = ddi_dma_sync(rx_buffer->dma_handle, 0, pkt_length,
		    DDI_DMA_SYNC_FORKERNEL);
		if (ret != DDI_SUCCESS) {
			xge_debug_ll(XGE_ERR, "%s%d: rx: can not do DMA sync",
			    XGELL_IFNAME, lldev->instance);
			xge_hal_ring_dtr_free(channelh, dtr); /* drop it */
			xgell_rx_buffer_release(rx_buffer);
			continue;
		}

		/*
		 * Allocate message for the packet.
		 */
		if (lldev->bf_pool.post > lldev->bf_pool.post_hiwat) {
			copyit = B_TRUE;
		} else {
			copyit = B_FALSE;
		}

		mp = xgell_rx_1b_msg_alloc(rx_buffer, pkt_length, &ext_info,
		    &copyit);

		xge_hal_ring_dtr_free(channelh, dtr);

		/*
		 * Release the buffer and recycle it later
		 */
		if ((mp == NULL) || copyit) {
			xgell_rx_buffer_release(rx_buffer);
		} else {
			/*
			 * Count it since the buffer should be loaned up.
			 */
			mutex_enter(&lldev->bf_pool.pool_lock);
			lldev->bf_pool.post++;
			mutex_exit(&lldev->bf_pool.pool_lock);
		}
		if (mp == NULL) {
			xge_debug_ll(XGE_ERR,
			    "%s%d: rx: can not allocate mp mblk", XGELL_IFNAME,
			    lldev->instance);
			continue;
		}

		/*
		 * Associate cksum_flags per packet type and h/w cksum flags.
		 */
		xgell_rx_hcksum_assoc(mp, (char *)rx_buffer->vaddr +
		    HEADROOM, pkt_length, &ext_info);

		if (mp_head == NULL) {
			mp_head = mp;
			mp_end = mp;
		} else {
			mp_end->b_next = mp;
			mp_end = mp;
		}

	} while (xge_hal_ring_dtr_next_completed(channelh, &dtr, &t_code) ==
	    XGE_HAL_OK);

	if (mp_head) {
		mac_rx(lldev->mh, ((xgell_ring_t *)userdata)->handle, mp_head);
	}

	/*
	 * Always call replenish_all to recycle rx_buffers.
	 */
	mutex_enter(&lldev->bf_pool.pool_lock);
	xgell_rx_buffer_replenish_all(lldev);
	mutex_exit(&lldev->bf_pool.pool_lock);

	return (XGE_HAL_OK);
}

/*
 * xgell_xmit_compl
 *
 * If an interrupt was raised to indicate DMA complete of the Tx packet,
 * this function is called. It identifies the last TxD whose buffer was
 * freed and frees all skbs whose data have already DMA'ed into the NICs
 * internal memory.
 */
static xge_hal_status_e
xgell_xmit_compl(xge_hal_channel_h channelh, xge_hal_dtr_h dtr, u8 t_code,
    void *userdata)
{
	xgelldev_t *lldev = userdata;

	do {
		xgell_txd_priv_t *txd_priv = ((xgell_txd_priv_t *)
		    xge_hal_fifo_dtr_private(dtr));
		mblk_t *mp = txd_priv->mblk;
#if !defined(XGELL_TX_NOMAP_COPY)
		int i;
#endif

		if (t_code) {
			xge_debug_ll(XGE_TRACE, "%s%d: tx: dtr 0x%"PRIx64
			    " completed due to error t_code %01x", XGELL_IFNAME,
			    lldev->instance, (uint64_t)(uintptr_t)dtr, t_code);

			(void) xge_hal_device_handle_tcode(channelh, dtr,
			    t_code);
		}

#if !defined(XGELL_TX_NOMAP_COPY)
		for (i = 0; i < txd_priv->handle_cnt; i++) {
			xge_assert(txd_priv->dma_handles[i]);
			(void) ddi_dma_unbind_handle(txd_priv->dma_handles[i]);
			ddi_dma_free_handle(&txd_priv->dma_handles[i]);
			txd_priv->dma_handles[i] = 0;
		}
#endif

		xge_hal_fifo_dtr_free(channelh, dtr);

		freemsg(mp);
		lldev->resched_avail++;

	} while (xge_hal_fifo_dtr_next_completed(channelh, &dtr, &t_code) ==
	    XGE_HAL_OK);

	if (lldev->resched_retry &&
	    xge_queue_produce_context(xge_hal_device_queue(lldev->devh),
	    XGELL_EVENT_RESCHED_NEEDED, lldev) == XGE_QUEUE_OK) {
	    xge_debug_ll(XGE_TRACE, "%s%d: IRQ produced event for queue %d",
		XGELL_IFNAME, lldev->instance,
		((xge_hal_channel_t *)lldev->fifo_channel)->post_qid);
		lldev->resched_send = lldev->resched_avail;
		lldev->resched_retry = 0;
	}

	return (XGE_HAL_OK);
}

/*
 * xgell_send
 * @hldev: pointer to s2hal_device_t strucutre
 * @mblk: pointer to network buffer, i.e. mblk_t structure
 *
 * Called by the xgell_m_tx to transmit the packet to the XFRAME firmware.
 * A pointer to an M_DATA message that contains the packet is passed to
 * this routine.
 */
static boolean_t
xgell_send(xge_hal_device_t *hldev, mblk_t *mp)
{
	mblk_t *bp;
	int retry, repeat;
	xge_hal_status_e status;
	xge_hal_dtr_h dtr;
	xgelldev_t *lldev = xge_hal_device_private(hldev);
	xgell_txd_priv_t *txd_priv;
	uint32_t pflags;
#ifndef XGELL_TX_NOMAP_COPY
	int handle_cnt, frag_cnt, ret, i;
#endif

_begin:
	retry = repeat = 0;
#ifndef XGELL_TX_NOMAP_COPY
	handle_cnt = frag_cnt = 0;
#endif

	if (!lldev->is_initialized || lldev->in_reset)
		return (B_FALSE);

	/*
	 * If the free Tx dtrs count reaches the lower threshold,
	 * inform the gld to stop sending more packets till the free
	 * dtrs count exceeds higher threshold. Driver informs the
	 * gld through gld_sched call, when the free dtrs count exceeds
	 * the higher threshold.
	 */
	if (__hal_channel_dtr_count(lldev->fifo_channel)
	    <= XGELL_TX_LEVEL_LOW) {
		xge_debug_ll(XGE_TRACE, "%s%d: queue %d: err on xmit,"
		    "free descriptors count at low threshold %d",
		    XGELL_IFNAME, lldev->instance,
		    ((xge_hal_channel_t *)lldev->fifo_channel)->post_qid,
		    XGELL_TX_LEVEL_LOW);
		retry = 1;
		goto _exit;
	}

	status = xge_hal_fifo_dtr_reserve(lldev->fifo_channel, &dtr);
	if (status != XGE_HAL_OK) {
		switch (status) {
		case XGE_HAL_INF_CHANNEL_IS_NOT_READY:
			xge_debug_ll(XGE_ERR,
			    "%s%d: channel %d is not ready.", XGELL_IFNAME,
			    lldev->instance,
			    ((xge_hal_channel_t *)
			    lldev->fifo_channel)->post_qid);
			retry = 1;
			goto _exit;
		case XGE_HAL_INF_OUT_OF_DESCRIPTORS:
			xge_debug_ll(XGE_TRACE, "%s%d: queue %d: error in xmit,"
			    " out of descriptors.", XGELL_IFNAME,
			    lldev->instance,
			    ((xge_hal_channel_t *)
			    lldev->fifo_channel)->post_qid);
			retry = 1;
			goto _exit;
		default:
			return (B_FALSE);
		}
	}

	txd_priv = xge_hal_fifo_dtr_private(dtr);
	txd_priv->mblk = mp;

	/*
	 * VLAN tag should be passed down along with MAC header, so h/w needn't
	 * do insertion.
	 *
	 * For NIC driver that has to strip and re-insert VLAN tag, the example
	 * is the other implementation for xge. The driver can simple bcopy()
	 * ether_vlan_header to overwrite VLAN tag and let h/w insert the tag
	 * automatically, since it's impossible that GLD sends down mp(s) with
	 * splited ether_vlan_header.
	 *
	 * struct ether_vlan_header *evhp;
	 * uint16_t tci;
	 *
	 * evhp = (struct ether_vlan_header *)mp->b_rptr;
	 * if (evhp->ether_tpid == htons(VLAN_TPID)) {
	 * 	tci = ntohs(evhp->ether_tci);
	 * 	(void) bcopy(mp->b_rptr, mp->b_rptr + VLAN_TAGSZ,
	 *	    2 * ETHERADDRL);
	 * 	mp->b_rptr += VLAN_TAGSZ;
	 *
	 * 	xge_hal_fifo_dtr_vlan_set(dtr, tci);
	 * }
	 */

#ifdef XGELL_TX_NOMAP_COPY
	for (bp = mp; bp != NULL; bp = bp->b_cont) {
		int mblen;
		xge_hal_status_e rc;

		/* skip zero-length message blocks */
		mblen = MBLKL(bp);
		if (mblen == 0) {
			continue;
		}
		rc = xge_hal_fifo_dtr_buffer_append(lldev->fifo_channel, dtr,
			bp->b_rptr, mblen);
		xge_assert(rc == XGE_HAL_OK);
	}
	xge_hal_fifo_dtr_buffer_finalize(lldev->fifo_channel, dtr, 0);
#else
	for (bp = mp; bp != NULL; bp = bp->b_cont) {
		int mblen;
		uint_t ncookies;
		ddi_dma_cookie_t dma_cookie;
		ddi_dma_handle_t dma_handle;

		/* skip zero-length message blocks */
		mblen = MBLKL(bp);
		if (mblen == 0) {
			continue;
		}

		ret = ddi_dma_alloc_handle(lldev->dev_info, &tx_dma_attr,
		    DDI_DMA_DONTWAIT, 0, &dma_handle);
		if (ret != DDI_SUCCESS) {
			xge_debug_ll(XGE_ERR,
			    "%s%d: can not allocate dma handle",
			    XGELL_IFNAME, lldev->instance);
			goto _exit_cleanup;
		}

		ret = ddi_dma_addr_bind_handle(dma_handle, NULL,
		    (caddr_t)bp->b_rptr, mblen,
		    DDI_DMA_WRITE | DDI_DMA_STREAMING, DDI_DMA_DONTWAIT, 0,
		    &dma_cookie, &ncookies);

		switch (ret) {
		case DDI_DMA_MAPPED:
			/* everything's fine */
			break;

		case DDI_DMA_NORESOURCES:
			xge_debug_ll(XGE_ERR,
			    "%s%d: can not bind dma address",
			    XGELL_IFNAME, lldev->instance);
			ddi_dma_free_handle(&dma_handle);
			goto _exit_cleanup;

		case DDI_DMA_NOMAPPING:
		case DDI_DMA_INUSE:
		case DDI_DMA_TOOBIG:
		default:
			/* drop packet, don't retry */
			xge_debug_ll(XGE_ERR,
			    "%s%d: can not map message buffer",
			    XGELL_IFNAME, lldev->instance);
			ddi_dma_free_handle(&dma_handle);
			goto _exit_cleanup;
		}

		if (ncookies + frag_cnt > XGE_HAL_DEFAULT_FIFO_FRAGS) {
			xge_debug_ll(XGE_ERR, "%s%d: too many fragments, "
			    "requested c:%d+f:%d", XGELL_IFNAME,
			    lldev->instance, ncookies, frag_cnt);
			(void) ddi_dma_unbind_handle(dma_handle);
			ddi_dma_free_handle(&dma_handle);
			goto _exit_cleanup;
		}

		/* setup the descriptors for this data buffer */
		while (ncookies) {
			xge_hal_fifo_dtr_buffer_set(lldev->fifo_channel, dtr,
			    frag_cnt++, dma_cookie.dmac_laddress,
			    dma_cookie.dmac_size);
			if (--ncookies) {
				ddi_dma_nextcookie(dma_handle, &dma_cookie);
			}

		}

		txd_priv->dma_handles[handle_cnt++] = dma_handle;

		if (bp->b_cont &&
		    (frag_cnt + XGE_HAL_DEFAULT_FIFO_FRAGS_THRESHOLD >=
		    XGE_HAL_DEFAULT_FIFO_FRAGS)) {
			mblk_t *nmp;

			xge_debug_ll(XGE_TRACE,
			    "too many FRAGs [%d], pull up them", frag_cnt);

			if ((nmp = msgpullup(bp->b_cont, -1)) == NULL) {
				/* Drop packet, don't retry */
				xge_debug_ll(XGE_ERR,
				    "%s%d: can not pullup message buffer",
				    XGELL_IFNAME, lldev->instance);
				goto _exit_cleanup;
			}
			freemsg(bp->b_cont);
			bp->b_cont = nmp;
		}
	}

	txd_priv->handle_cnt = handle_cnt;
#endif /* XGELL_TX_NOMAP_COPY */

	hcksum_retrieve(mp, NULL, NULL, NULL, NULL, NULL, NULL, &pflags);
	if (pflags & HCK_IPV4_HDRCKSUM) {
		xge_hal_fifo_dtr_cksum_set_bits(dtr,
		    XGE_HAL_TXD_TX_CKO_IPV4_EN);
	}
	if (pflags & HCK_FULLCKSUM) {
		xge_hal_fifo_dtr_cksum_set_bits(dtr, XGE_HAL_TXD_TX_CKO_TCP_EN |
		    XGE_HAL_TXD_TX_CKO_UDP_EN);
	}

	xge_hal_fifo_dtr_post(lldev->fifo_channel, dtr);

	return (B_TRUE);

_exit_cleanup:

#if !defined(XGELL_TX_NOMAP_COPY)
	for (i = 0; i < handle_cnt; i++) {
		(void) ddi_dma_unbind_handle(txd_priv->dma_handles[i]);
		ddi_dma_free_handle(&txd_priv->dma_handles[i]);
		txd_priv->dma_handles[i] = 0;
	}
#endif

	xge_hal_fifo_dtr_free(lldev->fifo_channel, dtr);

	if (repeat) {
		goto _begin;
	}

_exit:
	if (retry) {
		if (lldev->resched_avail != lldev->resched_send &&
		    xge_queue_produce_context(xge_hal_device_queue(lldev->devh),
		    XGELL_EVENT_RESCHED_NEEDED, lldev) == XGE_QUEUE_OK) {
			lldev->resched_send = lldev->resched_avail;
			return (B_FALSE);
		} else {
			lldev->resched_retry = 1;
		}
	}

	freemsg(mp);
	return (B_TRUE);
}

/*
 * xge_m_tx
 * @arg: pointer to the s2hal_device_t structure
 * @resid: resource id
 * @mp: pointer to the message buffer
 *
 * Called by MAC Layer to send a chain of packets
 */
static mblk_t *
xgell_m_tx(void *arg, mblk_t *mp)
{
	xge_hal_device_t *hldev = arg;
	mblk_t *next;

	while (mp != NULL) {
		next = mp->b_next;
		mp->b_next = NULL;

		if (!xgell_send(hldev, mp)) {
			mp->b_next = next;
			break;
		}
		mp = next;
	}

	return (mp);
}

/*
 * xgell_rx_dtr_term
 *
 * Function will be called by HAL to terminate all DTRs for
 * Ring(s) type of channels.
 */
static void
xgell_rx_dtr_term(xge_hal_channel_h channelh, xge_hal_dtr_h dtrh,
    xge_hal_dtr_state_e state, void *userdata, xge_hal_channel_reopen_e reopen)
{
	xgell_rxd_priv_t *rxd_priv =
	    ((xgell_rxd_priv_t *)xge_hal_ring_dtr_private(channelh, dtrh));
	xgell_rx_buffer_t *rx_buffer = rxd_priv->rx_buffer;

	if (state == XGE_HAL_DTR_STATE_POSTED) {
		xge_hal_ring_dtr_free(channelh, dtrh);
		xgell_rx_buffer_release(rx_buffer);
	}
}

/*
 * xgell_tx_term
 *
 * Function will be called by HAL to terminate all DTRs for
 * Fifo(s) type of channels.
 */
static void
xgell_tx_term(xge_hal_channel_h channelh, xge_hal_dtr_h dtrh,
    xge_hal_dtr_state_e state, void *userdata, xge_hal_channel_reopen_e reopen)
{
	xgell_txd_priv_t *txd_priv =
	    ((xgell_txd_priv_t *)xge_hal_fifo_dtr_private(dtrh));
	mblk_t *mp = txd_priv->mblk;
#if !defined(XGELL_TX_NOMAP_COPY)
	int i;
#endif
	/*
	 * for Tx we must clean up the DTR *only* if it has been
	 * posted!
	 */
	if (state != XGE_HAL_DTR_STATE_POSTED) {
		return;
	}

#if !defined(XGELL_TX_NOMAP_COPY)
	for (i = 0; i < txd_priv->handle_cnt; i++) {
		xge_assert(txd_priv->dma_handles[i]);
		(void) ddi_dma_unbind_handle(txd_priv->dma_handles[i]);
		ddi_dma_free_handle(&txd_priv->dma_handles[i]);
		txd_priv->dma_handles[i] = 0;
	}
#endif

	xge_hal_fifo_dtr_free(channelh, dtrh);

	freemsg(mp);
}

/*
 * xgell_tx_open
 * @lldev: the link layer object
 *
 * Initialize and open all Tx channels;
 */
static boolean_t
xgell_tx_open(xgelldev_t *lldev)
{
	xge_hal_status_e status;
	u64 adapter_status;
	xge_hal_channel_attr_t attr;

	attr.post_qid		= 0;
	attr.compl_qid		= 0;
	attr.callback		= xgell_xmit_compl;
	attr.per_dtr_space	= sizeof (xgell_txd_priv_t);
	attr.flags		= 0;
	attr.type		= XGE_HAL_CHANNEL_TYPE_FIFO;
	attr.userdata		= lldev;
	attr.dtr_init		= NULL;
	attr.dtr_term		= xgell_tx_term;

	if (xge_hal_device_status(lldev->devh, &adapter_status)) {
		xge_debug_ll(XGE_ERR, "%s%d: device is not ready "
		    "adaper status reads 0x%"PRIx64, XGELL_IFNAME,
		    lldev->instance, (uint64_t)adapter_status);
		return (B_FALSE);
	}

	status = xge_hal_channel_open(lldev->devh, &attr,
	    &lldev->fifo_channel, XGE_HAL_CHANNEL_OC_NORMAL);
	if (status != XGE_HAL_OK) {
		xge_debug_ll(XGE_ERR, "%s%d: cannot open Tx channel "
		    "got status code %d", XGELL_IFNAME,
		    lldev->instance, status);
		return (B_FALSE);
	}

	return (B_TRUE);
}

/*
 * xgell_rx_open
 * @lldev: the link layer object
 *
 * Initialize and open all Rx channels;
 */
static boolean_t
xgell_rx_open(xgelldev_t *lldev)
{
	xge_hal_status_e status;
	u64 adapter_status;
	xge_hal_channel_attr_t attr;

	attr.post_qid		= XGELL_RING_MAIN_QID;
	attr.compl_qid		= 0;
	attr.callback		= xgell_rx_1b_compl;
	attr.per_dtr_space	= sizeof (xgell_rxd_priv_t);
	attr.flags		= 0;
	attr.type		= XGE_HAL_CHANNEL_TYPE_RING;
	attr.dtr_init		= xgell_rx_dtr_replenish;
	attr.dtr_term		= xgell_rx_dtr_term;

	if (xge_hal_device_status(lldev->devh, &adapter_status)) {
		xge_debug_ll(XGE_ERR,
		    "%s%d: device is not ready adaper status reads 0x%"PRIx64,
		    XGELL_IFNAME, lldev->instance,
		    (uint64_t)adapter_status);
		return (B_FALSE);
	}

	lldev->ring_main.lldev = lldev;
	attr.userdata = &lldev->ring_main;

	status = xge_hal_channel_open(lldev->devh, &attr,
	    &lldev->ring_main.channelh, XGE_HAL_CHANNEL_OC_NORMAL);
	if (status != XGE_HAL_OK) {
		xge_debug_ll(XGE_ERR, "%s%d: cannot open Rx channel got status "
		    " code %d", XGELL_IFNAME, lldev->instance, status);
		return (B_FALSE);
	}

	return (B_TRUE);
}

static int
xgell_initiate_start(xgelldev_t *lldev)
{
	xge_hal_status_e status;
	xge_hal_device_t *hldev = lldev->devh;
	int maxpkt = hldev->config.mtu;

	/* check initial mtu before enabling the device */
	status = xge_hal_device_mtu_check(lldev->devh, maxpkt);
	if (status != XGE_HAL_OK) {
		xge_debug_ll(XGE_ERR, "%s%d: MTU size %d is invalid",
		    XGELL_IFNAME, lldev->instance, maxpkt);
		return (EINVAL);
	}

	/* set initial mtu before enabling the device */
	status = xge_hal_device_mtu_set(lldev->devh, maxpkt);
	if (status != XGE_HAL_OK) {
		xge_debug_ll(XGE_ERR, "%s%d: can not set new MTU %d",
		    XGELL_IFNAME, lldev->instance, maxpkt);
		return (EIO);
	}

	/* now, enable the device */
	status = xge_hal_device_enable(lldev->devh);
	if (status != XGE_HAL_OK) {
		xge_debug_ll(XGE_ERR, "%s%d: can not enable the device",
		    XGELL_IFNAME, lldev->instance);
		return (EIO);
	}

	if (!xgell_rx_open(lldev)) {
		status = xge_hal_device_disable(lldev->devh);
		if (status != XGE_HAL_OK) {
			u64 adapter_status;
			(void) xge_hal_device_status(lldev->devh,
			    &adapter_status);
			xge_debug_ll(XGE_ERR, "%s%d: can not safely disable "
			    "the device. adaper status 0x%"PRIx64
			    " returned status %d",
			    XGELL_IFNAME, lldev->instance,
			    (uint64_t)adapter_status, status);
		}
		xge_os_mdelay(1500);
		return (ENOMEM);
	}

#ifdef XGELL_TX_NOMAP_COPY
	hldev->config.fifo.alignment_size = XGELL_MAX_FRAME_SIZE(hldev);
#endif

	if (!xgell_tx_open(lldev)) {
		status = xge_hal_device_disable(lldev->devh);
		if (status != XGE_HAL_OK) {
			u64 adapter_status;
			(void) xge_hal_device_status(lldev->devh,
			    &adapter_status);
			xge_debug_ll(XGE_ERR, "%s%d: can not safely disable "
			    "the device. adaper status 0x%"PRIx64
			    " returned status %d",
			    XGELL_IFNAME, lldev->instance,
			    (uint64_t)adapter_status, status);
		}
		xge_os_mdelay(1500);
		xge_hal_channel_close(lldev->ring_main.channelh,
		    XGE_HAL_CHANNEL_OC_NORMAL);
		return (ENOMEM);
	}

	/* time to enable interrupts */
	xge_hal_device_intr_enable(lldev->devh);

	lldev->is_initialized = 1;

	return (0);
}

static void
xgell_initiate_stop(xgelldev_t *lldev)
{
	xge_hal_status_e status;

	lldev->is_initialized = 0;

	status = xge_hal_device_disable(lldev->devh);
	if (status != XGE_HAL_OK) {
		u64 adapter_status;
		(void) xge_hal_device_status(lldev->devh, &adapter_status);
		xge_debug_ll(XGE_ERR, "%s%d: can not safely disable "
		    "the device. adaper status 0x%"PRIx64" returned status %d",
		    XGELL_IFNAME, lldev->instance,
		    (uint64_t)adapter_status, status);
	}
	xge_hal_device_intr_disable(lldev->devh);

	xge_debug_ll(XGE_TRACE, "%s",
	    "waiting for device irq to become quiescent...");
	xge_os_mdelay(1500);

	xge_queue_flush(xge_hal_device_queue(lldev->devh));

	xge_hal_channel_close(lldev->ring_main.channelh,
	    XGE_HAL_CHANNEL_OC_NORMAL);

	xge_hal_channel_close(lldev->fifo_channel,
	    XGE_HAL_CHANNEL_OC_NORMAL);
}

/*
 * xgell_m_start
 * @arg: pointer to device private strucutre(hldev)
 *
 * This function is called by MAC Layer to enable the XFRAME
 * firmware to generate interrupts and also prepare the
 * driver to call mac_rx for delivering receive packets
 * to MAC Layer.
 */
static int
xgell_m_start(void *arg)
{
	xge_hal_device_t *hldev = arg;
	xgelldev_t *lldev = xge_hal_device_private(hldev);
	int ret;

	xge_debug_ll(XGE_TRACE, "%s%d: M_START", XGELL_IFNAME,
	    lldev->instance);

	mutex_enter(&lldev->genlock);

	if (lldev->is_initialized) {
		xge_debug_ll(XGE_ERR, "%s%d: device is already initialized",
		    XGELL_IFNAME, lldev->instance);
		mutex_exit(&lldev->genlock);
		return (EINVAL);
	}

	hldev->terminating = 0;
	if (ret = xgell_initiate_start(lldev)) {
		mutex_exit(&lldev->genlock);
		return (ret);
	}

	lldev->timeout_id = timeout(xge_device_poll, hldev, XGE_DEV_POLL_TICKS);

	if (!lldev->timeout_id) {
		xgell_initiate_stop(lldev);
		mutex_exit(&lldev->genlock);
		return (EINVAL);
	}

	mutex_exit(&lldev->genlock);

	return (0);
}

/*
 * xgell_m_stop
 * @arg: pointer to device private data (hldev)
 *
 * This function is called by the MAC Layer to disable
 * the XFRAME firmware for generating any interrupts and
 * also stop the driver from calling mac_rx() for
 * delivering data packets to the MAC Layer.
 */
static void
xgell_m_stop(void *arg)
{
	xge_hal_device_t *hldev;
	xgelldev_t *lldev;

	xge_debug_ll(XGE_TRACE, "%s", "MAC_STOP");

	hldev = arg;
	xge_assert(hldev);

	lldev = (xgelldev_t *)xge_hal_device_private(hldev);
	xge_assert(lldev);

	mutex_enter(&lldev->genlock);
	if (!lldev->is_initialized) {
		xge_debug_ll(XGE_ERR, "%s", "device is not initialized...");
		mutex_exit(&lldev->genlock);
		return;
	}

	xge_hal_device_terminating(hldev);
	xgell_initiate_stop(lldev);

	/* reset device */
	(void) xge_hal_device_reset(lldev->devh);

	mutex_exit(&lldev->genlock);

	(void) untimeout(lldev->timeout_id);

	xge_debug_ll(XGE_TRACE, "%s", "returning back to MAC Layer...");
}

/*
 * xgell_onerr_reset
 * @lldev: pointer to xgelldev_t structure
 *
 * This function is called by HAL Event framework to reset the HW
 * This function is must be called with genlock taken.
 */
int
xgell_onerr_reset(xgelldev_t *lldev)
{
	int rc = 0;

	if (!lldev->is_initialized) {
		xge_debug_ll(XGE_ERR, "%s%d: can not reset",
		    XGELL_IFNAME, lldev->instance);
		return (rc);
	}

	lldev->in_reset = 1;
	xgell_initiate_stop(lldev);

	/* reset device */
	(void) xge_hal_device_reset(lldev->devh);

	rc = xgell_initiate_start(lldev);
	lldev->in_reset = 0;

	return (rc);
}


/*
 * xgell_m_unicst
 * @arg: pointer to device private strucutre(hldev)
 * @mac_addr:
 *
 * This function is called by MAC Layer to set the physical address
 * of the XFRAME firmware.
 */
static int
xgell_m_unicst(void *arg, const uint8_t *macaddr)
{
	xge_hal_status_e status;
	xge_hal_device_t *hldev = arg;
	xgelldev_t *lldev = (xgelldev_t *)xge_hal_device_private(hldev);
	xge_debug_ll(XGE_TRACE, "%s", "MAC_UNICST");

	xge_debug_ll(XGE_TRACE, "%s", "M_UNICAST");

	mutex_enter(&lldev->genlock);

	xge_debug_ll(XGE_TRACE,
	    "setting macaddr: 0x%02x-%02x-%02x-%02x-%02x-%02x",
	    macaddr[0], macaddr[1], macaddr[2],
	    macaddr[3], macaddr[4], macaddr[5]);

	status = xge_hal_device_macaddr_set(hldev, 0, (uchar_t *)macaddr);
	if (status != XGE_HAL_OK) {
		xge_debug_ll(XGE_ERR, "%s%d: can not set mac address",
		    XGELL_IFNAME, lldev->instance);
		mutex_exit(&lldev->genlock);
		return (EIO);
	}

	mutex_exit(&lldev->genlock);

	return (0);
}


/*
 * xgell_m_multicst
 * @arg: pointer to device private strucutre(hldev)
 * @add:
 * @mc_addr:
 *
 * This function is called by MAC Layer to enable or
 * disable device-level reception of specific multicast addresses.
 */
static int
xgell_m_multicst(void *arg, boolean_t add, const uint8_t *mc_addr)
{
	xge_hal_status_e status;
	xge_hal_device_t *hldev = (xge_hal_device_t *)arg;
	xgelldev_t *lldev = xge_hal_device_private(hldev);

	xge_debug_ll(XGE_TRACE, "M_MULTICAST add %d", add);

	mutex_enter(&lldev->genlock);

	if (!lldev->is_initialized) {
		xge_debug_ll(XGE_ERR, "%s%d: can not set multicast",
		    XGELL_IFNAME, lldev->instance);
		mutex_exit(&lldev->genlock);
		return (EIO);
	}

	/* FIXME: missing HAL functionality: enable_one() */

	status = (add) ?
	    xge_hal_device_mcast_enable(hldev) :
	    xge_hal_device_mcast_disable(hldev);

	if (status != XGE_HAL_OK) {
		xge_debug_ll(XGE_ERR, "failed to %s multicast, status %d",
		    add ? "enable" : "disable", status);
		mutex_exit(&lldev->genlock);
		return (EIO);
	}

	mutex_exit(&lldev->genlock);

	return (0);
}


/*
 * xgell_m_promisc
 * @arg: pointer to device private strucutre(hldev)
 * @on:
 *
 * This function is called by MAC Layer to enable or
 * disable the reception of all the packets on the medium
 */
static int
xgell_m_promisc(void *arg, boolean_t on)
{
	xge_hal_device_t *hldev = (xge_hal_device_t *)arg;
	xgelldev_t *lldev = xge_hal_device_private(hldev);

	mutex_enter(&lldev->genlock);

	xge_debug_ll(XGE_TRACE, "%s", "MAC_PROMISC_SET");

	if (!lldev->is_initialized) {
		xge_debug_ll(XGE_ERR, "%s%d: can not set promiscuous",
		    XGELL_IFNAME, lldev->instance);
		mutex_exit(&lldev->genlock);
		return (EIO);
	}

	if (on) {
		xge_hal_device_promisc_enable(hldev);
	} else {
		xge_hal_device_promisc_disable(hldev);
	}

	mutex_exit(&lldev->genlock);

	return (0);
}

/*
 * xgell_m_stat
 * @arg: pointer to device private strucutre(hldev)
 *
 * This function is called by MAC Layer to get network statistics
 * from the driver.
 */
static int
xgell_m_stat(void *arg, uint_t stat, uint64_t *val)
{
	xge_hal_stats_hw_info_t *hw_info;
	xge_hal_device_t *hldev = (xge_hal_device_t *)arg;
	xgelldev_t *lldev = xge_hal_device_private(hldev);

	xge_debug_ll(XGE_TRACE, "%s", "MAC_STATS_GET");

	if (!mutex_tryenter(&lldev->genlock))
		return (EAGAIN);

	if (!lldev->is_initialized) {
		mutex_exit(&lldev->genlock);
		return (EAGAIN);
	}

	if (xge_hal_stats_hw(hldev, &hw_info) != XGE_HAL_OK) {
		mutex_exit(&lldev->genlock);
		return (EAGAIN);
	}

	switch (stat) {
	case MAC_STAT_IFSPEED:
		*val = 10000000000ull; /* 10G */
		break;

	case MAC_STAT_MULTIRCV:
		*val = hw_info->rmac_vld_mcst_frms;
		break;

	case MAC_STAT_BRDCSTRCV:
		*val = hw_info->rmac_vld_bcst_frms;
		break;

	case MAC_STAT_MULTIXMT:
		*val = hw_info->tmac_mcst_frms;
		break;

	case MAC_STAT_BRDCSTXMT:
		*val = hw_info->tmac_bcst_frms;
		break;

	case MAC_STAT_RBYTES:
		*val = hw_info->rmac_ttl_octets;
		break;

	case MAC_STAT_NORCVBUF:
		*val = hw_info->rmac_drop_frms;
		break;

	case MAC_STAT_IERRORS:
		*val = hw_info->rmac_discarded_frms;
		break;

	case MAC_STAT_OBYTES:
		*val = hw_info->tmac_ttl_octets;
		break;

	case MAC_STAT_NOXMTBUF:
		*val = hw_info->tmac_drop_frms;
		break;

	case MAC_STAT_OERRORS:
		*val = hw_info->tmac_any_err_frms;
		break;

	case MAC_STAT_IPACKETS:
		*val = hw_info->rmac_vld_frms;
		break;

	case MAC_STAT_OPACKETS:
		*val = hw_info->tmac_frms;
		break;

	case ETHER_STAT_FCS_ERRORS:
		*val = hw_info->rmac_fcs_err_frms;
		break;

	case ETHER_STAT_TOOLONG_ERRORS:
		*val = hw_info->rmac_long_frms;
		break;

	case ETHER_STAT_LINK_DUPLEX:
		*val = LINK_DUPLEX_FULL;
		break;

	default:
		mutex_exit(&lldev->genlock);
		return (ENOTSUP);
	}

	mutex_exit(&lldev->genlock);

	return (0);
}

/*
 * xgell_device_alloc - Allocate new LL device
 */
int
xgell_device_alloc(xge_hal_device_h devh,
    dev_info_t *dev_info, xgelldev_t **lldev_out)
{
	xgelldev_t *lldev;
	xge_hal_device_t *hldev = (xge_hal_device_t *)devh;
	int instance = ddi_get_instance(dev_info);

	*lldev_out = NULL;

	xge_debug_ll(XGE_TRACE, "trying to register etherenet device %s%d...",
	    XGELL_IFNAME, instance);

	lldev = kmem_zalloc(sizeof (xgelldev_t), KM_SLEEP);

	/* allocate mac */
	lldev->devh = hldev;
	lldev->instance = instance;
	lldev->dev_info = dev_info;

	*lldev_out = lldev;

	ddi_set_driver_private(dev_info, (caddr_t)hldev);

	return (DDI_SUCCESS);
}

/*
 * xgell_device_free
 */
void
xgell_device_free(xgelldev_t *lldev)
{
	xge_debug_ll(XGE_TRACE, "freeing device %s%d",
	    XGELL_IFNAME, lldev->instance);

	kmem_free(lldev, sizeof (xgelldev_t));
}

/*
 * xgell_ioctl
 */
static void
xgell_m_ioctl(void *arg, queue_t *wq, mblk_t *mp)
{
	xge_hal_device_t *hldev = (xge_hal_device_t *)arg;
	xgelldev_t *lldev = (xgelldev_t *)xge_hal_device_private(hldev);
	struct iocblk *iocp;
	int err = 0;
	int cmd;
	int need_privilege = 1;
	int ret = 0;


	iocp = (struct iocblk *)mp->b_rptr;
	iocp->ioc_error = 0;
	cmd = iocp->ioc_cmd;
	xge_debug_ll(XGE_TRACE, "MAC_IOCTL cmd 0x%x", cmd);
	switch (cmd) {
	case ND_GET:
		need_privilege = 0;
		/* FALLTHRU */
	case ND_SET:
		break;
	default:
		xge_debug_ll(XGE_TRACE, "unknown cmd 0x%x", cmd);
		miocnak(wq, mp, 0, EINVAL);
		return;
	}

	if (need_privilege) {
		err = secpolicy_net_config(iocp->ioc_cr, B_FALSE);
		if (err != 0) {
			xge_debug_ll(XGE_ERR,
			    "drv_priv(): rejected cmd 0x%x, err %d",
			    cmd, err);
			miocnak(wq, mp, 0, err);
			return;
		}
	}

	switch (cmd) {
	case ND_GET:
		/*
		 * If nd_getset() returns B_FALSE, the command was
		 * not valid (e.g. unknown name), so we just tell the
		 * top-level ioctl code to send a NAK (with code EINVAL).
		 *
		 * Otherwise, nd_getset() will have built the reply to
		 * be sent (but not actually sent it), so we tell the
		 * caller to send the prepared reply.
		 */
		ret = nd_getset(wq, lldev->ndp, mp);
		xge_debug_ll(XGE_TRACE, "got ndd get ioctl");
		break;

	case ND_SET:
		ret = nd_getset(wq, lldev->ndp, mp);
		xge_debug_ll(XGE_TRACE, "got ndd set ioctl");
		break;

	default:
		break;
	}

	if (ret == B_FALSE) {
		xge_debug_ll(XGE_ERR,
		    "nd_getset(): rejected cmd 0x%x, err %d",
		    cmd, err);
		miocnak(wq, mp, 0, EINVAL);
	} else {
		mp->b_datap->db_type = iocp->ioc_error == 0 ?
		    M_IOCACK : M_IOCNAK;
		qreply(wq, mp);
	}
}

/* ARGSUSED */
static boolean_t
xgell_m_getcapab(void *arg, mac_capab_t cap, void *cap_data)
{
	switch (cap) {
	case MAC_CAPAB_HCKSUM: {
		uint32_t *hcksum_txflags = cap_data;
		*hcksum_txflags = HCKSUM_INET_FULL_V4 | HCKSUM_INET_FULL_V6 |
		    HCKSUM_IPHDRCKSUM;
		break;
	}
	case MAC_CAPAB_POLL:
		/*
		 * Fallthrough to default, as we don't support GLDv3
		 * polling.  When blanking is implemented, we will need to
		 * change this to return B_TRUE in addition to registering
		 * an mc_resources callback.
		 */
	default:
		return (B_FALSE);
	}
	return (B_TRUE);
}

static int
xgell_stats_get(queue_t *q, mblk_t *mp, caddr_t cp, cred_t *credp)
{
	xgelldev_t *lldev = (xgelldev_t *)cp;
	xge_hal_status_e status;
	int count = 0, retsize;
	char *buf;

	buf = kmem_alloc(XGELL_STATS_BUFSIZE, KM_SLEEP);
	if (buf == NULL) {
		return (ENOSPC);
	}

	status = xge_hal_aux_stats_tmac_read(lldev->devh, XGELL_STATS_BUFSIZE,
	    buf, &retsize);
	if (status != XGE_HAL_OK) {
		kmem_free(buf, XGELL_STATS_BUFSIZE);
		xge_debug_ll(XGE_ERR, "tmac_read(): status %d", status);
		return (EINVAL);
	}
	count += retsize;

	status = xge_hal_aux_stats_rmac_read(lldev->devh,
	    XGELL_STATS_BUFSIZE - count,
	    buf+count, &retsize);
	if (status != XGE_HAL_OK) {
		kmem_free(buf, XGELL_STATS_BUFSIZE);
		xge_debug_ll(XGE_ERR, "rmac_read(): status %d", status);
		return (EINVAL);
	}
	count += retsize;

	status = xge_hal_aux_stats_pci_read(lldev->devh,
	    XGELL_STATS_BUFSIZE - count, buf + count, &retsize);
	if (status != XGE_HAL_OK) {
		kmem_free(buf, XGELL_STATS_BUFSIZE);
		xge_debug_ll(XGE_ERR, "pci_read(): status %d", status);
		return (EINVAL);
	}
	count += retsize;

	status = xge_hal_aux_stats_sw_dev_read(lldev->devh,
	    XGELL_STATS_BUFSIZE - count, buf + count, &retsize);
	if (status != XGE_HAL_OK) {
		kmem_free(buf, XGELL_STATS_BUFSIZE);
		xge_debug_ll(XGE_ERR, "sw_dev_read(): status %d", status);
		return (EINVAL);
	}
	count += retsize;

	status = xge_hal_aux_stats_hal_read(lldev->devh,
	    XGELL_STATS_BUFSIZE - count, buf + count, &retsize);
	if (status != XGE_HAL_OK) {
		kmem_free(buf, XGELL_STATS_BUFSIZE);
		xge_debug_ll(XGE_ERR, "pci_read(): status %d", status);
		return (EINVAL);
	}
	count += retsize;

	*(buf + count - 1) = '\0'; /* remove last '\n' */
	(void) mi_mpprintf(mp, "%s", buf);
	kmem_free(buf, XGELL_STATS_BUFSIZE);

	return (0);
}

static int
xgell_pciconf_get(queue_t *q, mblk_t *mp, caddr_t cp, cred_t *credp)
{
	xgelldev_t *lldev = (xgelldev_t *)cp;
	xge_hal_status_e status;
	int retsize;
	char *buf;

	buf = kmem_alloc(XGELL_PCICONF_BUFSIZE, KM_SLEEP);
	if (buf == NULL) {
		return (ENOSPC);
	}
	status = xge_hal_aux_pci_config_read(lldev->devh, XGELL_PCICONF_BUFSIZE,
	    buf, &retsize);
	if (status != XGE_HAL_OK) {
		kmem_free(buf, XGELL_PCICONF_BUFSIZE);
		xge_debug_ll(XGE_ERR, "pci_config_read(): status %d", status);
		return (EINVAL);
	}
	*(buf + retsize - 1) = '\0'; /* remove last '\n' */
	(void) mi_mpprintf(mp, "%s", buf);
	kmem_free(buf, XGELL_PCICONF_BUFSIZE);

	return (0);
}

static int
xgell_about_get(queue_t *q, mblk_t *mp, caddr_t cp, cred_t *credp)
{
	xgelldev_t *lldev = (xgelldev_t *)cp;
	xge_hal_status_e status;
	int retsize;
	char *buf;

	buf = kmem_alloc(XGELL_ABOUT_BUFSIZE, KM_SLEEP);
	if (buf == NULL) {
		return (ENOSPC);
	}
	status = xge_hal_aux_about_read(lldev->devh, XGELL_ABOUT_BUFSIZE,
	    buf, &retsize);
	if (status != XGE_HAL_OK) {
		kmem_free(buf, XGELL_ABOUT_BUFSIZE);
		xge_debug_ll(XGE_ERR, "about_read(): status %d", status);
		return (EINVAL);
	}
	*(buf + retsize - 1) = '\0'; /* remove last '\n' */
	(void) mi_mpprintf(mp, "%s", buf);
	kmem_free(buf, XGELL_ABOUT_BUFSIZE);

	return (0);
}

static unsigned long bar0_offset = 0x110; /* adapter_control */

static int
xgell_bar0_get(queue_t *q, mblk_t *mp, caddr_t cp, cred_t *credp)
{
	xgelldev_t *lldev = (xgelldev_t *)cp;
	xge_hal_status_e status;
	int retsize;
	char *buf;

	buf = kmem_alloc(XGELL_IOCTL_BUFSIZE, KM_SLEEP);
	if (buf == NULL) {
		return (ENOSPC);
	}
	status = xge_hal_aux_bar0_read(lldev->devh, bar0_offset,
	    XGELL_IOCTL_BUFSIZE, buf, &retsize);
	if (status != XGE_HAL_OK) {
		kmem_free(buf, XGELL_IOCTL_BUFSIZE);
		xge_debug_ll(XGE_ERR, "bar0_read(): status %d", status);
		return (EINVAL);
	}
	*(buf + retsize - 1) = '\0'; /* remove last '\n' */
	(void) mi_mpprintf(mp, "%s", buf);
	kmem_free(buf, XGELL_IOCTL_BUFSIZE);

	return (0);
}

static int
xgell_bar0_set(queue_t *q, mblk_t *mp, char *value, caddr_t cp, cred_t *credp)
{
	unsigned long old_offset = bar0_offset;
	char *end;

	if (value && *value == '0' &&
	    (*(value + 1) == 'x' || *(value + 1) == 'X')) {
		value += 2;
	}

	bar0_offset = mi_strtol(value, &end, 16);
	if (end == value) {
		bar0_offset = old_offset;
		return (EINVAL);
	}

	xge_debug_ll(XGE_TRACE, "bar0: new value %s:%lX", value, bar0_offset);

	return (0);
}

static int
xgell_debug_level_get(queue_t *q, mblk_t *mp, caddr_t cp, cred_t *credp)
{
	char *buf;

	buf = kmem_alloc(XGELL_IOCTL_BUFSIZE, KM_SLEEP);
	if (buf == NULL) {
		return (ENOSPC);
	}
	(void) mi_mpprintf(mp, "debug_level %d", xge_hal_driver_debug_level());
	kmem_free(buf, XGELL_IOCTL_BUFSIZE);

	return (0);
}

static int
xgell_debug_level_set(queue_t *q, mblk_t *mp, char *value, caddr_t cp,
    cred_t *credp)
{
	int level;
	char *end;

	level = mi_strtol(value, &end, 10);
	if (level < XGE_NONE || level > XGE_ERR || end == value) {
		return (EINVAL);
	}

	xge_hal_driver_debug_level_set(level);

	return (0);
}

static int
xgell_debug_module_mask_get(queue_t *q, mblk_t *mp, caddr_t cp, cred_t *credp)
{
	char *buf;

	buf = kmem_alloc(XGELL_IOCTL_BUFSIZE, KM_SLEEP);
	if (buf == NULL) {
		return (ENOSPC);
	}
	(void) mi_mpprintf(mp, "debug_module_mask 0x%08x",
	    xge_hal_driver_debug_module_mask());
	kmem_free(buf, XGELL_IOCTL_BUFSIZE);

	return (0);
}

static int
xgell_debug_module_mask_set(queue_t *q, mblk_t *mp, char *value, caddr_t cp,
			    cred_t *credp)
{
	u32 mask;
	char *end;

	if (value && *value == '0' &&
	    (*(value + 1) == 'x' || *(value + 1) == 'X')) {
		value += 2;
	}

	mask = mi_strtol(value, &end, 16);
	if (end == value) {
		return (EINVAL);
	}

	xge_hal_driver_debug_module_mask_set(mask);

	return (0);
}

static int
xgell_devconfig_get(queue_t *q, mblk_t *mp, caddr_t cp, cred_t *credp)
{
	xgelldev_t *lldev = (xgelldev_t *)(void *)cp;
	xge_hal_status_e status;
	int retsize;
	char *buf;

	buf = kmem_alloc(XGELL_DEVCONF_BUFSIZE, KM_SLEEP);
	if (buf == NULL) {
		return (ENOSPC);
	}
	status = xge_hal_aux_device_config_read(lldev->devh,
						XGELL_DEVCONF_BUFSIZE,
						buf, &retsize);
	if (status != XGE_HAL_OK) {
		kmem_free(buf, XGELL_DEVCONF_BUFSIZE);
		xge_debug_ll(XGE_ERR, "device_config_read(): status %d",
		    status);
		return (EINVAL);
	}
	*(buf + retsize - 1) = '\0'; /* remove last '\n' */
	(void) mi_mpprintf(mp, "%s", buf);
	kmem_free(buf, XGELL_DEVCONF_BUFSIZE);

	return (0);
}

/*
 * xgell_device_register
 * @devh: pointer on HAL device
 * @config: pointer on this network device configuration
 * @ll_out: output pointer. Will be assigned to valid LL device.
 *
 * This function will allocate and register network device
 */
int
xgell_device_register(xgelldev_t *lldev, xgell_config_t *config)
{
	mac_register_t *macp;
	xge_hal_device_t *hldev = (xge_hal_device_t *)lldev->devh;
	int err;


	if (nd_load(&lldev->ndp, "pciconf", xgell_pciconf_get, NULL,
	    (caddr_t)lldev) == B_FALSE)
		goto xgell_ndd_fail;

	if (nd_load(&lldev->ndp, "about", xgell_about_get, NULL,
	    (caddr_t)lldev) == B_FALSE)
		goto xgell_ndd_fail;

	if (nd_load(&lldev->ndp, "stats", xgell_stats_get, NULL,
	    (caddr_t)lldev) == B_FALSE)
		goto xgell_ndd_fail;

	if (nd_load(&lldev->ndp, "bar0", xgell_bar0_get, xgell_bar0_set,
	    (caddr_t)lldev) == B_FALSE)
		goto xgell_ndd_fail;

	if (nd_load(&lldev->ndp, "debug_level", xgell_debug_level_get,
	    xgell_debug_level_set, (caddr_t)lldev) == B_FALSE)
		goto xgell_ndd_fail;

	if (nd_load(&lldev->ndp, "debug_module_mask",
	    xgell_debug_module_mask_get, xgell_debug_module_mask_set,
	    (caddr_t)lldev) == B_FALSE)
		goto xgell_ndd_fail;

	if (nd_load(&lldev->ndp, "devconfig", xgell_devconfig_get, NULL,
	    (caddr_t)lldev) == B_FALSE)
		goto xgell_ndd_fail;

	bcopy(config, &lldev->config, sizeof (xgell_config_t));

	if (xgell_rx_create_buffer_pool(lldev) != DDI_SUCCESS) {
		nd_free(&lldev->ndp);
		xge_debug_ll(XGE_ERR, "unable to create RX buffer pool");
		return (DDI_FAILURE);
	}

	mutex_init(&lldev->genlock, NULL, MUTEX_DRIVER, hldev->irqh);

	if ((macp = mac_alloc(MAC_VERSION)) == NULL)
		goto xgell_register_fail;
	macp->m_type_ident = MAC_PLUGIN_IDENT_ETHER;
	macp->m_driver = hldev;
	macp->m_dip = lldev->dev_info;
	macp->m_src_addr = hldev->macaddr[0];
	macp->m_callbacks = &xgell_m_callbacks;
	macp->m_min_sdu = 0;
	macp->m_max_sdu = hldev->config.mtu;
	/*
	 * Finally, we're ready to register ourselves with the Nemo
	 * interface; if this succeeds, we're all ready to start()
	 */
	err = mac_register(macp, &lldev->mh);
	mac_free(macp);
	if (err != 0)
		goto xgell_register_fail;

	xge_debug_ll(XGE_TRACE, "etherenet device %s%d registered",
	    XGELL_IFNAME, lldev->instance);

	return (DDI_SUCCESS);

xgell_ndd_fail:
	nd_free(&lldev->ndp);
	xge_debug_ll(XGE_ERR, "%s", "unable to load ndd parameter");
	return (DDI_FAILURE);

xgell_register_fail:
	nd_free(&lldev->ndp);
	mutex_destroy(&lldev->genlock);
	/* Ignore return value, since RX not start */
	(void) xgell_rx_destroy_buffer_pool(lldev);
	xge_debug_ll(XGE_ERR, "%s", "unable to register networking device");
	return (DDI_FAILURE);
}

/*
 * xgell_device_unregister
 * @devh: pointer on HAL device
 * @lldev: pointer to valid LL device.
 *
 * This function will unregister and free network device
 */
int
xgell_device_unregister(xgelldev_t *lldev)
{
	/*
	 * Destroy RX buffer pool.
	 */
	if (xgell_rx_destroy_buffer_pool(lldev) != DDI_SUCCESS) {
		return (DDI_FAILURE);
	}

	if (mac_unregister(lldev->mh) != 0) {
		xge_debug_ll(XGE_ERR, "unable to unregister device %s%d",
		    XGELL_IFNAME, lldev->instance);
		return (DDI_FAILURE);
	}

	mutex_destroy(&lldev->genlock);

	nd_free(&lldev->ndp);

	xge_debug_ll(XGE_TRACE, "etherenet device %s%d unregistered",
	    XGELL_IFNAME, lldev->instance);

	return (DDI_SUCCESS);
}
