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
/* Copyright (c) 1990 Mentat Inc. */

#pragma ident	"@(#)igmp.c	1.60	06/08/14 SMI"

/*
 * Internet Group Management Protocol (IGMP) routines.
 * Multicast Listener Discovery Protocol (MLD) routines.
 *
 * Written by Steve Deering, Stanford, May 1988.
 * Modified by Rosen Sharma, Stanford, Aug 1994.
 * Modified by Bill Fenner, Xerox PARC, Feb. 1995.
 *
 * MULTICAST 3.5.1.1
 */

#include <sys/types.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/strlog.h>
#include <sys/strsun.h>
#include <sys/systm.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/cmn_err.h>
#include <sys/atomic.h>
#include <sys/zone.h>

#include <sys/param.h>
#include <sys/socket.h>
#include <inet/ipclassifier.h>
#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/igmp_var.h>
#include <netinet/ip6.h>
#include <netinet/icmp6.h>

#include <inet/common.h>
#include <inet/mi.h>
#include <inet/nd.h>
#include <inet/ip.h>
#include <inet/ip6.h>
#include <inet/ip_multi.h>
#include <inet/ip_listutils.h>

#include <netinet/igmp.h>
#include <inet/ip_if.h>
#include <net/pfkeyv2.h>
#include <inet/ipsec_info.h>

static uint_t	igmp_query_in(ipha_t *ipha, igmpa_t *igmpa, ill_t *ill);
static uint_t	igmpv3_query_in(igmp3qa_t *igmp3qa, ill_t *ill, int igmplen);
static uint_t	mld_query_in(mld_hdr_t *mldh, ill_t *ill);
static uint_t	mldv2_query_in(mld2q_t *mld2q, ill_t *ill, int mldlen);
static void	igmp_sendpkt(ilm_t *ilm, uchar_t type, ipaddr_t addr);
static void	mld_sendpkt(ilm_t *ilm, uchar_t type, const in6_addr_t *v6addr);
static void	igmpv3_sendrpt(ipif_t *ipif, mrec_t *reclist);
static void	mldv2_sendrpt(ill_t *ill, mrec_t *reclist);
static mrec_t	*mcast_bldmrec(mcast_record_t type, in6_addr_t *grp,
		    slist_t *srclist, mrec_t *next);
static void	mcast_init_rtx(ill_t *ill, rtx_state_t *rtxp,
		    mcast_record_t rtype, slist_t *flist);
static mrec_t	*mcast_merge_rtx(ilm_t *ilm, mrec_t *rp, slist_t *flist);

/* Following protected by igmp_timer_lock */
static int 	igmp_time_to_next;	/* Time since last timeout */
static int 	igmp_timer_fired_last;
uint_t		igmp_deferred_next = INFINITY;
timeout_id_t	igmp_timeout_id = 0;
kmutex_t	igmp_timer_lock;

/* Protected by igmp_slowtimeout_lock */
timeout_id_t	igmp_slowtimeout_id = 0;
kmutex_t	igmp_slowtimeout_lock;

/* Following protected by mld_timer_lock */
static int 	mld_time_to_next;	/* Time since last timeout */
static int 	mld_timer_fired_last;
uint_t		mld_deferred_next = INFINITY;
timeout_id_t	mld_timeout_id = 0;
kmutex_t	mld_timer_lock;

/* Protected by mld_slowtimeout_lock */
timeout_id_t	mld_slowtimeout_id = 0;
kmutex_t	mld_slowtimeout_lock;

/*
 * Macros used to do timer len conversions.  Timer values are always
 * stored and passed to the timer functions as milliseconds; but the
 * default values and values from the wire may not be.
 *
 * And yes, it's obscure, but decisecond is easier to abbreviate than
 * "tenths of a second".
 */
#define	DSEC_TO_MSEC(dsec)	((dsec) * 100)
#define	SEC_TO_MSEC(sec)	((sec) * 1000)

/*
 * The first multicast join will trigger the igmp timers / mld timers
 * The unit for next is milliseconds.
 */
void
igmp_start_timers(unsigned next)
{
	int	time_left;
	/* Protected by igmp_timer_lock */
	static  boolean_t igmp_timer_setter_active;
	int	ret;

	ASSERT(next != 0 && next != INFINITY);

	mutex_enter(&igmp_timer_lock);

	if (igmp_timer_setter_active) {
		/*
		 * Serialize timer setters, one at a time. If the
		 * timer is currently being set by someone,
		 * just record the next time when it has to be
		 * invoked and return. The current setter will
		 * take care.
		 */
		igmp_time_to_next = MIN(igmp_time_to_next, next);
		mutex_exit(&igmp_timer_lock);
		return;
	} else {
		igmp_timer_setter_active = B_TRUE;
	}
	if (igmp_timeout_id == 0) {
		/*
		 * The timer is inactive. We need to start a timer
		 */
		igmp_time_to_next = next;
		igmp_timeout_id = timeout(igmp_timeout_handler, NULL,
		    MSEC_TO_TICK(igmp_time_to_next));
		igmp_timer_setter_active = B_FALSE;
		mutex_exit(&igmp_timer_lock);
		return;
	}

	/*
	 * The timer was scheduled sometime back for firing in
	 * 'igmp_time_to_next' ms and is active. We need to
	 * reschedule the timeout if the new 'next' will happen
	 * earlier than the currently scheduled timeout
	 */
	time_left = igmp_timer_fired_last +
	    MSEC_TO_TICK(igmp_time_to_next) - ddi_get_lbolt();
	if (time_left < MSEC_TO_TICK(next)) {
		igmp_timer_setter_active = B_FALSE;
		mutex_exit(&igmp_timer_lock);
		return;
	}

	mutex_exit(&igmp_timer_lock);
	ret = untimeout(igmp_timeout_id);
	mutex_enter(&igmp_timer_lock);
	/*
	 * The timeout was cancelled, or the timeout handler
	 * completed, while we were blocked in the untimeout.
	 * No other thread could have set the timer meanwhile
	 * since we serialized all the timer setters. Thus
	 * no timer is currently active nor executing nor will
	 * any timer fire in the future. We start the timer now
	 * if needed.
	 */
	if (ret == -1) {
		ASSERT(igmp_timeout_id == 0);
	} else {
		ASSERT(igmp_timeout_id != 0);
		igmp_timeout_id = 0;
	}
	if (igmp_time_to_next != 0) {
		igmp_time_to_next = MIN(igmp_time_to_next, next);
		igmp_timeout_id = timeout(igmp_timeout_handler, NULL,
		    MSEC_TO_TICK(igmp_time_to_next));
	}
	igmp_timer_setter_active = B_FALSE;
	mutex_exit(&igmp_timer_lock);
}

/*
 * mld_start_timers:
 * The unit for next is milliseconds.
 */
void
mld_start_timers(unsigned next)
{
	int	time_left;
	/* Protedted by mld_timer_lock */
	static  boolean_t mld_timer_setter_active;
	int	ret;

	ASSERT(next != 0 && next != INFINITY);

	mutex_enter(&mld_timer_lock);
	if (mld_timer_setter_active) {
		/*
		 * Serialize timer setters, one at a time. If the
		 * timer is currently being set by someone,
		 * just record the next time when it has to be
		 * invoked and return. The current setter will
		 * take care.
		 */
		mld_time_to_next = MIN(mld_time_to_next, next);
		mutex_exit(&mld_timer_lock);
		return;
	} else {
		mld_timer_setter_active = B_TRUE;
	}
	if (mld_timeout_id == 0) {
		/*
		 * The timer is inactive. We need to start a timer
		 */
		mld_time_to_next = next;
		mld_timeout_id = timeout(mld_timeout_handler, NULL,
		    MSEC_TO_TICK(mld_time_to_next));
		mld_timer_setter_active = B_FALSE;
		mutex_exit(&mld_timer_lock);
		return;
	}

	/*
	 * The timer was scheduled sometime back for firing in
	 * 'igmp_time_to_next' ms and is active. We need to
	 * reschedule the timeout if the new 'next' will happen
	 * earlier than the currently scheduled timeout
	 */
	time_left = mld_timer_fired_last +
	    MSEC_TO_TICK(mld_time_to_next) - ddi_get_lbolt();
	if (time_left < MSEC_TO_TICK(next)) {
		mld_timer_setter_active = B_FALSE;
		mutex_exit(&mld_timer_lock);
		return;
	}

	mutex_exit(&mld_timer_lock);
	ret = untimeout(mld_timeout_id);
	mutex_enter(&mld_timer_lock);
	/*
	 * The timeout was cancelled, or the timeout handler
	 * completed, while we were blocked in the untimeout.
	 * No other thread could have set the timer meanwhile
	 * since we serialized all the timer setters. Thus
	 * no timer is currently active nor executing nor will
	 * any timer fire in the future. We start the timer now
	 * if needed.
	 */
	if (ret == -1) {
		ASSERT(mld_timeout_id == 0);
	} else {
		ASSERT(mld_timeout_id != 0);
		mld_timeout_id = 0;
	}
	if (mld_time_to_next != 0) {
		mld_time_to_next = MIN(mld_time_to_next, next);
		mld_timeout_id = timeout(mld_timeout_handler, NULL,
		    MSEC_TO_TICK(mld_time_to_next));
	}
	mld_timer_setter_active = B_FALSE;
	mutex_exit(&mld_timer_lock);
}

/*
 * igmp_input:
 * Return 0 if the message is OK and should be handed to "raw" receivers.
 * Callers of igmp_input() may need to reinitialize variables that were copied
 * from the mblk as this calls pullupmsg().
 */
/* ARGSUSED */
int
igmp_input(queue_t *q, mblk_t *mp, ill_t *ill)
{
	igmpa_t 	*igmpa;
	ipha_t		*ipha = (ipha_t *)(mp->b_rptr);
	int		iphlen, igmplen, mblklen;
	ilm_t 		*ilm;
	uint32_t	src, dst;
	uint32_t 	group;
	uint_t		next;
	ipif_t 		*ipif;

	ASSERT(ill != NULL);
	ASSERT(!ill->ill_isv6);
	++igmpstat.igps_rcv_total;

	mblklen = MBLKL(mp);
	if (mblklen < 1 || mblklen < (iphlen = IPH_HDR_LENGTH(ipha))) {
		++igmpstat.igps_rcv_tooshort;
		freemsg(mp);
		return (-1);
	}
	igmplen = ntohs(ipha->ipha_length) - iphlen;
	/*
	 * Since msg sizes are more variable with v3, just pullup the
	 * whole thing now.
	 */
	if (MBLKL(mp) < (igmplen + iphlen)) {
		mblk_t *mp1;
		if ((mp1 = msgpullup(mp, -1)) == NULL) {
			++igmpstat.igps_rcv_tooshort;
			freemsg(mp);
			return (-1);
		}
		freemsg(mp);
		mp = mp1;
		ipha = (ipha_t *)(mp->b_rptr);
	}

	/*
	 * Validate lengths
	 */
	if (igmplen < IGMP_MINLEN) {
		++igmpstat.igps_rcv_tooshort;
		freemsg(mp);
		return (-1);
	}
	/*
	 * Validate checksum
	 */
	if (IP_CSUM(mp, iphlen, 0)) {
		++igmpstat.igps_rcv_badsum;
		freemsg(mp);
		return (-1);
	}

	igmpa = (igmpa_t *)(&mp->b_rptr[iphlen]);
	src = ipha->ipha_src;
	dst = ipha->ipha_dst;
	if (ip_debug > 1)
		(void) mi_strlog(ill->ill_rq, 1, SL_TRACE,
		    "igmp_input: src 0x%x, dst 0x%x on %s\n",
		    (int)ntohl(src), (int)ntohl(dst),
		    ill->ill_name);

	switch (igmpa->igmpa_type) {
	case IGMP_MEMBERSHIP_QUERY:
		/*
		 * packet length differentiates between v1/v2 and v3
		 * v1/v2 should be exactly 8 octets long; v3 is >= 12
		 */
		if (igmplen == IGMP_MINLEN) {
			next = igmp_query_in(ipha, igmpa, ill);
		} else if (igmplen >= IGMP_V3_QUERY_MINLEN) {
			next = igmpv3_query_in((igmp3qa_t *)igmpa, ill,
			    igmplen);
		} else {
			++igmpstat.igps_rcv_tooshort;
			freemsg(mp);
			return (-1);
		}
		if (next == 0) {
			freemsg(mp);
			return (-1);
		}

		if (next != INFINITY)
			igmp_start_timers(next);

		break;

	case IGMP_V1_MEMBERSHIP_REPORT:
	case IGMP_V2_MEMBERSHIP_REPORT:
		/*
		 * For fast leave to work, we have to know that we are the
		 * last person to send a report for this group. Reports
		 * generated by us are looped back since we could potentially
		 * be a multicast router, so discard reports sourced by me.
		 */
		mutex_enter(&ill->ill_lock);
		for (ipif = ill->ill_ipif; ipif != NULL;
		    ipif = ipif->ipif_next) {
			if (ipif->ipif_lcl_addr == src) {
				if (ip_debug > 1) {
					(void) mi_strlog(ill->ill_rq,
					    1,
					    SL_TRACE,
					    "igmp_input: we are only "
					    "member src 0x%x ipif_local 0x%x",
					    (int)ntohl(src),
					    (int)
					    ntohl(ipif->ipif_lcl_addr));
				}
				mutex_exit(&ill->ill_lock);
				return (0);
			}
		}
		mutex_exit(&ill->ill_lock);

		++igmpstat.igps_rcv_reports;
		group = igmpa->igmpa_group;
		if (!CLASSD(group)) {
			++igmpstat.igps_rcv_badreports;
			freemsg(mp);
			return (-1);
		}

		/*
		 * KLUDGE: if the IP source address of the report has an
		 * unspecified (i.e., zero) subnet number, as is allowed for
		 * a booting host, replace it with the correct subnet number
		 * so that a process-level multicast routing demon can
		 * determine which subnet it arrived from.  This is necessary
		 * to compensate for the lack of any way for a process to
		 * determine the arrival interface of an incoming packet.
		 *
		 * Requires that a copy of *this* message it passed up
		 * to the raw interface which is done by our caller.
		 */
		if ((src & htonl(0xFF000000U)) == 0) {	/* Minimum net mask */
			/* Pick the first ipif on this ill */
			mutex_enter(&ill->ill_lock);
			src = ill->ill_ipif->ipif_subnet;
			mutex_exit(&ill->ill_lock);
			ip1dbg(("igmp_input: changed src to 0x%x\n",
			    (int)ntohl(src)));
			ipha->ipha_src = src;
		}

		/*
		 * If we belong to the group being reported, and
		 * we are a 'Delaying member' in the RFC terminology,
		 * stop our timer for that group and 'clear flag' i.e.
		 * mark as IGMP_OTHERMEMBER. Do this for all logical
		 * interfaces on the given physical interface.
		 */
		mutex_enter(&ill->ill_lock);
		for (ipif = ill->ill_ipif; ipif != NULL;
		    ipif = ipif->ipif_next) {
			ilm = ilm_lookup_ipif(ipif, group);
			if (ilm != NULL) {
				++igmpstat.igps_rcv_ourreports;
				ilm->ilm_timer = INFINITY;
				ilm->ilm_state = IGMP_OTHERMEMBER;
			}
		} /* for */
		mutex_exit(&ill->ill_lock);
		break;

	case IGMP_V3_MEMBERSHIP_REPORT:
		/*
		 * Currently nothing to do here; IGMP router is not
		 * implemented in ip, and v3 hosts don't pay attention
		 * to membership reports.
		 */
		break;
	}
	/*
	 * Pass all valid IGMP packets up to any process(es) listening
	 * on a raw IGMP socket. Do not free the packet.
	 */
	return (0);
}

static uint_t
igmp_query_in(ipha_t *ipha, igmpa_t *igmpa, ill_t *ill)
{
	ilm_t	*ilm;
	int	timer;
	uint_t	next;

	++igmpstat.igps_rcv_queries;

	/*
	 * In the IGMPv2 specification, there are 3 states and a flag.
	 *
	 * In Non-Member state, we simply don't have a membership record.
	 * In Delaying Member state, our timer is running (ilm->ilm_timer
	 * < INFINITY).  In Idle Member state, our timer is not running
	 * (ilm->ilm_timer == INFINITY).
	 *
	 * The flag is ilm->ilm_state, it is set to IGMP_OTHERMEMBER if
	 * we have heard a report from another member, or IGMP_IREPORTEDLAST
	 * if I sent the last report.
	 */
	if (igmpa->igmpa_code == 0) {
		/*
		 * Query from an old router.
		 * Remember that the querier on this interface is old,
		 * and set the timer to the value in RFC 1112.
		 */


		mutex_enter(&ill->ill_lock);
		ill->ill_mcast_v1_time = 0;
		ill->ill_mcast_v1_tset = 1;
		if (ill->ill_mcast_type != IGMP_V1_ROUTER) {
			ip1dbg(("Received IGMPv1 Query on %s, switching mode "
			    "to IGMP_V1_ROUTER\n", ill->ill_name));
			atomic_add_16(&ill->ill_ifptr->illif_mcast_v1, 1);
			ill->ill_mcast_type = IGMP_V1_ROUTER;
		}
		mutex_exit(&ill->ill_lock);

		timer = SEC_TO_MSEC(IGMP_MAX_HOST_REPORT_DELAY);

		if (ipha->ipha_dst != htonl(INADDR_ALLHOSTS_GROUP) ||
		    igmpa->igmpa_group != 0) {
			++igmpstat.igps_rcv_badqueries;
			return (0);
		}

	} else {
		in_addr_t group;

		/*
		 * Query from a new router
		 * Simply do a validity check
		 */
		group = igmpa->igmpa_group;
		if (group != 0 && (!CLASSD(group))) {
			++igmpstat.igps_rcv_badqueries;
			return (0);
		}

		/*
		 * Switch interface state to v2 on receipt of a v2 query
		 * ONLY IF current state is v3.  Let things be if current
		 * state if v1 but do reset the v2-querier-present timer.
		 */
		mutex_enter(&ill->ill_lock);
		if (ill->ill_mcast_type == IGMP_V3_ROUTER) {
			ip1dbg(("Received IGMPv2 Query on %s, switching mode "
			    "to IGMP_V2_ROUTER", ill->ill_name));
			atomic_add_16(&ill->ill_ifptr->illif_mcast_v2, 1);
			ill->ill_mcast_type = IGMP_V2_ROUTER;
		}
		ill->ill_mcast_v2_time = 0;
		ill->ill_mcast_v2_tset = 1;
		mutex_exit(&ill->ill_lock);

		timer = DSEC_TO_MSEC((int)igmpa->igmpa_code);
	}

	if (ip_debug > 1) {
		mutex_enter(&ill->ill_lock);
		(void) mi_strlog(ill->ill_rq, 1, SL_TRACE,
		    "igmp_input: TIMER = igmp_code %d igmp_type 0x%x",
		    (int)ntohs(igmpa->igmpa_code),
		    (int)ntohs(igmpa->igmpa_type));
		mutex_exit(&ill->ill_lock);
	}

	/*
	 * -Start the timers in all of our membership records
	 *  for the physical interface on which the query
	 *  arrived, excluding those that belong to the "all
	 *  hosts" group (224.0.0.1).
	 *
	 * -Restart any timer that is already running but has
	 *  a value longer than the requested timeout.
	 *
	 * -Use the value specified in the query message as
	 *  the maximum timeout.
	 */
	next = (unsigned)INFINITY;
	mutex_enter(&ill->ill_lock);
	for (ilm = ill->ill_ilm; ilm; ilm = ilm->ilm_next) {

		/*
		 * A multicast router joins INADDR_ANY address
		 * to enable promiscuous reception of all
		 * mcasts from the interface. This INADDR_ANY
		 * is stored in the ilm_v6addr as V6 unspec addr
		 */
		if (!IN6_IS_ADDR_V4MAPPED(&ilm->ilm_v6addr))
			continue;
		if (ilm->ilm_addr == htonl(INADDR_ANY))
			continue;
		if (ilm->ilm_addr != htonl(INADDR_ALLHOSTS_GROUP) &&
		    (igmpa->igmpa_group == 0) ||
		    (igmpa->igmpa_group == ilm->ilm_addr)) {
			if (ilm->ilm_timer > timer) {
				MCAST_RANDOM_DELAY(ilm->ilm_timer, timer);
				if (ilm->ilm_timer < next)
					next = ilm->ilm_timer;
			}
		}
	}
	mutex_exit(&ill->ill_lock);

	return (next);
}

static uint_t
igmpv3_query_in(igmp3qa_t *igmp3qa, ill_t *ill, int igmplen)
{
	uint_t		i, next, mrd, qqi, timer, delay, numsrc;
	ilm_t		*ilm;
	ipaddr_t	*src_array;
	uint8_t		qrv;

	/* make sure numsrc matches packet size */
	numsrc = ntohs(igmp3qa->igmp3qa_numsrc);
	if (igmplen < IGMP_V3_QUERY_MINLEN + (numsrc * sizeof (ipaddr_t))) {
		++igmpstat.igps_rcv_tooshort;
		return (0);
	}
	src_array = (ipaddr_t *)&igmp3qa[1];

	++igmpstat.igps_rcv_queries;

	if ((mrd = (uint_t)igmp3qa->igmp3qa_mxrc) >= IGMP_V3_MAXRT_FPMIN) {
		uint_t hdrval, mant, exp;
		hdrval = (uint_t)igmp3qa->igmp3qa_mxrc;
		mant = hdrval & IGMP_V3_MAXRT_MANT_MASK;
		exp = (hdrval & IGMP_V3_MAXRT_EXP_MASK) >> 4;
		mrd = (mant | 0x10) << (exp + 3);
	}
	if (mrd == 0)
		mrd = MCAST_DEF_QUERY_RESP_INTERVAL;
	timer = DSEC_TO_MSEC(mrd);
	MCAST_RANDOM_DELAY(delay, timer);
	next = (unsigned)INFINITY;

	if ((qrv = igmp3qa->igmp3qa_sqrv & IGMP_V3_RV_MASK) == 0)
		ill->ill_mcast_rv = MCAST_DEF_ROBUSTNESS;
	else
		ill->ill_mcast_rv = qrv;

	if ((qqi = (uint_t)igmp3qa->igmp3qa_qqic) >= IGMP_V3_QQI_FPMIN) {
		uint_t hdrval, mant, exp;
		hdrval = (uint_t)igmp3qa->igmp3qa_qqic;
		mant = hdrval & IGMP_V3_QQI_MANT_MASK;
		exp = (hdrval & IGMP_V3_QQI_EXP_MASK) >> 4;
		qqi = (mant | 0x10) << (exp + 3);
	}
	ill->ill_mcast_qi = (qqi == 0) ? MCAST_DEF_QUERY_INTERVAL : qqi;

	/*
	 * If we have a pending general query response that's scheduled
	 * sooner than the delay we calculated for this response, then
	 * no action is required (RFC3376 section 5.2 rule 1)
	 */
	mutex_enter(&ill->ill_lock);
	if (ill->ill_global_timer < delay) {
		mutex_exit(&ill->ill_lock);
		return (next);
	}
	mutex_exit(&ill->ill_lock);

	/*
	 * Now take action depending upon query type:
	 * general, group specific, or group/source specific.
	 */
	if ((numsrc == 0) && (igmp3qa->igmp3qa_group == INADDR_ANY)) {
		/*
		 * general query
		 * We know global timer is either not running or is
		 * greater than our calculated delay, so reset it to
		 * our delay (random value in range [0, response time]).
		 */
		mutex_enter(&ill->ill_lock);
		ill->ill_global_timer = delay;
		next = ill->ill_global_timer;
		mutex_exit(&ill->ill_lock);

	} else {
		/* group or group/source specific query */
		mutex_enter(&ill->ill_lock);
		for (ilm = ill->ill_ilm; ilm; ilm = ilm->ilm_next) {
			if (!IN6_IS_ADDR_V4MAPPED(&ilm->ilm_v6addr) ||
			    (ilm->ilm_addr == htonl(INADDR_ANY)) ||
			    (ilm->ilm_addr == htonl(INADDR_ALLHOSTS_GROUP)) ||
			    (igmp3qa->igmp3qa_group != ilm->ilm_addr))
				continue;
			/*
			 * If the query is group specific or we have a
			 * pending group specific query, the response is
			 * group specific (pending sources list should be
			 * empty).  Otherwise, need to update the pending
			 * sources list for the group and source specific
			 * response.
			 */
			if (numsrc == 0 || (ilm->ilm_timer < INFINITY &&
			    SLIST_IS_EMPTY(ilm->ilm_pendsrcs))) {
group_query:
				FREE_SLIST(ilm->ilm_pendsrcs);
				ilm->ilm_pendsrcs = NULL;
			} else {
				boolean_t overflow;
				slist_t *pktl;
				if (numsrc > MAX_FILTER_SIZE ||
				    (ilm->ilm_pendsrcs == NULL &&
				    (ilm->ilm_pendsrcs = l_alloc()) == NULL)) {
					/*
					 * We've been sent more sources than
					 * we can deal with; or we can't deal
					 * with a source list at all.  Revert
					 * to a group specific query.
					 */
					goto group_query;
				}
				if ((pktl = l_alloc()) == NULL)
					goto group_query;
				pktl->sl_numsrc = numsrc;
				for (i = 0; i < numsrc; i++)
					IN6_IPADDR_TO_V4MAPPED(src_array[i],
					    &(pktl->sl_addr[i]));
				l_union_in_a(ilm->ilm_pendsrcs, pktl,
				    &overflow);
				l_free(pktl);
				if (overflow)
					goto group_query;
			}
			/* choose soonest timer */
			ilm->ilm_timer = MIN(ilm->ilm_timer, delay);
			if (ilm->ilm_timer < next)
				next = ilm->ilm_timer;
		}
		mutex_exit(&ill->ill_lock);
	}

	return (next);
}

void
igmp_joingroup(ilm_t *ilm)
{
	ill_t	*ill;

	ill = ilm->ilm_ipif->ipif_ill;

	ASSERT(IAM_WRITER_ILL(ill));
	ASSERT(ilm->ilm_ill == NULL && !ilm->ilm_ipif->ipif_isv6);

	mutex_enter(&ill->ill_lock);
	if (ilm->ilm_addr == htonl(INADDR_ALLHOSTS_GROUP)) {
		ilm->ilm_rtx.rtx_timer = INFINITY;
		ilm->ilm_state = IGMP_OTHERMEMBER;
		mutex_exit(&ill->ill_lock);
	} else {
		ip1dbg(("Querier mode %d, sending report, group %x\n",
		    ill->ill_mcast_type, htonl(ilm->ilm_addr)));
		if (ill->ill_mcast_type == IGMP_V1_ROUTER) {
			mutex_exit(&ill->ill_lock);
			igmp_sendpkt(ilm, IGMP_V1_MEMBERSHIP_REPORT, 0);
			mutex_enter(&ill->ill_lock);
		} else if (ill->ill_mcast_type == IGMP_V2_ROUTER) {
			mutex_exit(&ill->ill_lock);
			igmp_sendpkt(ilm, IGMP_V2_MEMBERSHIP_REPORT, 0);
			mutex_enter(&ill->ill_lock);
		} else if (ill->ill_mcast_type == IGMP_V3_ROUTER) {
			mrec_t *rp;
			mcast_record_t rtype;
			/*
			 * The possible state changes we need to handle here:
			 *   Old State	New State	Report
			 *
			 *   INCLUDE(0)	INCLUDE(X)	ALLOW(X),BLOCK(0)
			 *   INCLUDE(0)	EXCLUDE(X)	TO_EX(X)
			 *
			 * No need to send the BLOCK(0) report; ALLOW(X)
			 * is enough.
			 */
			rtype = (ilm->ilm_fmode == MODE_IS_INCLUDE) ?
			    ALLOW_NEW_SOURCES : CHANGE_TO_EXCLUDE;
			rp = mcast_bldmrec(rtype, &ilm->ilm_v6addr,
			    ilm->ilm_filter, NULL);
			mutex_exit(&ill->ill_lock);
			igmpv3_sendrpt(ilm->ilm_ipif, rp);
			mutex_enter(&ill->ill_lock);
			/*
			 * Set up retransmission state.  Timer is set below,
			 * for both v3 and older versions.
			 */
			mcast_init_rtx(ill, &ilm->ilm_rtx, rtype,
			    ilm->ilm_filter);
		}

		/* Set the ilm timer value */
		MCAST_RANDOM_DELAY(ilm->ilm_rtx.rtx_timer,
		    SEC_TO_MSEC(IGMP_MAX_HOST_REPORT_DELAY));
		ilm->ilm_state = IGMP_IREPORTEDLAST;
		mutex_exit(&ill->ill_lock);

		/*
		 * To avoid deadlock, we don't call igmp_start_timers from
		 * here. igmp_start_timers needs to call untimeout, and we
		 * can't hold the ipsq across untimeout since
		 * igmp_timeout_handler could be blocking trying to
		 * acquire the ipsq. Instead we start the timer after we get
		 * out of the ipsq in ipsq_exit.
		 */
		mutex_enter(&igmp_timer_lock);
		igmp_deferred_next = MIN(ilm->ilm_rtx.rtx_timer,
		    igmp_deferred_next);
		mutex_exit(&igmp_timer_lock);
	}

	if (ip_debug > 1) {
		(void) mi_strlog(ilm->ilm_ipif->ipif_ill->ill_rq, 1, SL_TRACE,
		    "igmp_joingroup: multicast_type %d timer %d",
		    (ilm->ilm_ipif->ipif_ill->ill_mcast_type),
		    (int)ntohl(ilm->ilm_rtx.rtx_timer));
	}
}

void
mld_joingroup(ilm_t *ilm)
{
	ill_t	*ill;

	ill = ilm->ilm_ill;

	ASSERT(IAM_WRITER_ILL(ill));
	ASSERT(ilm->ilm_ipif == NULL && ill->ill_isv6);

	mutex_enter(&ill->ill_lock);
	if (IN6_ARE_ADDR_EQUAL(&ipv6_all_hosts_mcast, &ilm->ilm_v6addr)) {
		ilm->ilm_rtx.rtx_timer = INFINITY;
		ilm->ilm_state = IGMP_OTHERMEMBER;
		mutex_exit(&ill->ill_lock);
	} else {
		if (ill->ill_mcast_type == MLD_V1_ROUTER) {
			mutex_exit(&ill->ill_lock);
			mld_sendpkt(ilm, MLD_LISTENER_REPORT, NULL);
			mutex_enter(&ill->ill_lock);
		} else {
			mrec_t *rp;
			mcast_record_t rtype;
			/*
			 * The possible state changes we need to handle here:
			 *	Old State   New State	Report
			 *
			 *	INCLUDE(0)  INCLUDE(X)	ALLOW(X),BLOCK(0)
			 *	INCLUDE(0)  EXCLUDE(X)	TO_EX(X)
			 *
			 * No need to send the BLOCK(0) report; ALLOW(X)
			 * is enough
			 */
			rtype = (ilm->ilm_fmode == MODE_IS_INCLUDE) ?
			    ALLOW_NEW_SOURCES : CHANGE_TO_EXCLUDE;
			rp = mcast_bldmrec(rtype, &ilm->ilm_v6addr,
			    ilm->ilm_filter, NULL);
			mutex_exit(&ill->ill_lock);
			mldv2_sendrpt(ill, rp);
			mutex_enter(&ill->ill_lock);
			/*
			 * Set up retransmission state.  Timer is set below,
			 * for both v2 and v1.
			 */
			mcast_init_rtx(ill, &ilm->ilm_rtx, rtype,
			    ilm->ilm_filter);
		}

		/* Set the ilm timer value */
		ASSERT(ill->ill_mcast_type != MLD_V2_ROUTER ||
		    ilm->ilm_rtx.rtx_cnt > 0);
		MCAST_RANDOM_DELAY(ilm->ilm_rtx.rtx_timer,
		    SEC_TO_MSEC(ICMP6_MAX_HOST_REPORT_DELAY));
		ilm->ilm_state = IGMP_IREPORTEDLAST;
		mutex_exit(&ill->ill_lock);

		/*
		 * To avoid deadlock, we don't call mld_start_timers from
		 * here. mld_start_timers needs to call untimeout, and we
		 * can't hold the ipsq (i.e. the lock) across untimeout
		 * since mld_timeout_handler could be blocking trying to
		 * acquire the ipsq. Instead we start the timer after we get
		 * out of the ipsq in ipsq_exit
		 */
		mutex_enter(&mld_timer_lock);
		mld_deferred_next = MIN(ilm->ilm_rtx.rtx_timer,
		    mld_deferred_next);
		mutex_exit(&mld_timer_lock);
	}

	if (ip_debug > 1) {
		(void) mi_strlog(ilm->ilm_ill->ill_rq, 1, SL_TRACE,
		    "mld_joingroup: multicast_type %d timer %d",
		    (ilm->ilm_ill->ill_mcast_type),
		    (int)ntohl(ilm->ilm_rtx.rtx_timer));
	}
}

void
igmp_leavegroup(ilm_t *ilm)
{
	ill_t *ill = ilm->ilm_ipif->ipif_ill;

	ASSERT(ilm->ilm_ill == NULL);
	ASSERT(!ill->ill_isv6);

	mutex_enter(&ill->ill_lock);
	if (ilm->ilm_state == IGMP_IREPORTEDLAST &&
	    ill->ill_mcast_type == IGMP_V2_ROUTER &&
	    (ilm->ilm_addr != htonl(INADDR_ALLHOSTS_GROUP))) {
		mutex_exit(&ill->ill_lock);
		igmp_sendpkt(ilm, IGMP_V2_LEAVE_GROUP,
		    (htonl(INADDR_ALLRTRS_GROUP)));
		return;
	} else if ((ill->ill_mcast_type == IGMP_V3_ROUTER) &&
	    (ilm->ilm_addr != htonl(INADDR_ALLHOSTS_GROUP))) {
		mrec_t *rp;
		/*
		 * The possible state changes we need to handle here:
		 *	Old State	New State	Report
		 *
		 *	INCLUDE(X)	INCLUDE(0)	ALLOW(0),BLOCK(X)
		 *	EXCLUDE(X)	INCLUDE(0)	TO_IN(0)
		 *
		 * No need to send the ALLOW(0) report; BLOCK(X) is enough
		 */
		if (ilm->ilm_fmode == MODE_IS_INCLUDE) {
			rp = mcast_bldmrec(BLOCK_OLD_SOURCES, &ilm->ilm_v6addr,
			    ilm->ilm_filter, NULL);
		} else {
			rp = mcast_bldmrec(CHANGE_TO_INCLUDE, &ilm->ilm_v6addr,
			    NULL, NULL);
		}
		mutex_exit(&ill->ill_lock);
		igmpv3_sendrpt(ilm->ilm_ipif, rp);
		return;
	}
	mutex_exit(&ill->ill_lock);
}

void
mld_leavegroup(ilm_t *ilm)
{
	ill_t *ill = ilm->ilm_ill;

	ASSERT(ilm->ilm_ipif == NULL);
	ASSERT(ill->ill_isv6);

	mutex_enter(&ill->ill_lock);
	if (ilm->ilm_state == IGMP_IREPORTEDLAST &&
	    ill->ill_mcast_type == MLD_V1_ROUTER &&
	    (!IN6_ARE_ADDR_EQUAL(&ipv6_all_hosts_mcast, &ilm->ilm_v6addr))) {
		mutex_exit(&ill->ill_lock);
		mld_sendpkt(ilm, MLD_LISTENER_REDUCTION, &ipv6_all_rtrs_mcast);
		return;
	} else if ((ill->ill_mcast_type == MLD_V2_ROUTER) &&
	    (!IN6_ARE_ADDR_EQUAL(&ipv6_all_hosts_mcast, &ilm->ilm_v6addr))) {
		mrec_t *rp;
		/*
		 * The possible state changes we need to handle here:
		 *	Old State	New State	Report
		 *
		 *	INCLUDE(X)	INCLUDE(0)	ALLOW(0),BLOCK(X)
		 *	EXCLUDE(X)	INCLUDE(0)	TO_IN(0)
		 *
		 * No need to send the ALLOW(0) report; BLOCK(X) is enough
		 */
		if (ilm->ilm_fmode == MODE_IS_INCLUDE) {
			rp = mcast_bldmrec(BLOCK_OLD_SOURCES, &ilm->ilm_v6addr,
			    ilm->ilm_filter, NULL);
		} else {
			rp = mcast_bldmrec(CHANGE_TO_INCLUDE, &ilm->ilm_v6addr,
			    NULL, NULL);
		}
		mutex_exit(&ill->ill_lock);
		mldv2_sendrpt(ill, rp);
		return;
	}
	mutex_exit(&ill->ill_lock);
}

void
igmp_statechange(ilm_t *ilm, mcast_record_t fmode, slist_t *flist)
{
	ill_t *ill;
	mrec_t *rp;

	ASSERT(ilm != NULL);

	/* state change reports should only be sent if the router is v3 */
	if (ilm->ilm_ipif->ipif_ill->ill_mcast_type != IGMP_V3_ROUTER)
		return;

	if (ilm->ilm_ill == NULL) {
		ASSERT(ilm->ilm_ipif != NULL);
		ill = ilm->ilm_ipif->ipif_ill;
	} else {
		ill = ilm->ilm_ill;
	}

	mutex_enter(&ill->ill_lock);

	/*
	 * Compare existing(old) state with the new state and prepare
	 * State Change Report, according to the rules in RFC 3376:
	 *
	 *	Old State	New State	State Change Report
	 *
	 *	INCLUDE(A)	INCLUDE(B)	ALLOW(B-A),BLOCK(A-B)
	 *	EXCLUDE(A)	EXCLUDE(B)	ALLOW(A-B),BLOCK(B-A)
	 *	INCLUDE(A)	EXCLUDE(B)	TO_EX(B)
	 *	EXCLUDE(A)	INCLUDE(B)	TO_IN(B)
	 */

	if (ilm->ilm_fmode == fmode) {
		slist_t	*a_minus_b = NULL, *b_minus_a = NULL;
		slist_t *allow, *block;
		if (((a_minus_b = l_alloc()) == NULL) ||
		    ((b_minus_a = l_alloc()) == NULL)) {
			l_free(a_minus_b);
			if (ilm->ilm_fmode == MODE_IS_INCLUDE)
				goto send_to_ex;
			else
				goto send_to_in;
		}
		l_difference(ilm->ilm_filter, flist, a_minus_b);
		l_difference(flist, ilm->ilm_filter, b_minus_a);
		if (ilm->ilm_fmode == MODE_IS_INCLUDE) {
			allow = b_minus_a;
			block = a_minus_b;
		} else {
			allow = a_minus_b;
			block = b_minus_a;
		}
		rp = NULL;
		if (!SLIST_IS_EMPTY(allow))
			rp = mcast_bldmrec(ALLOW_NEW_SOURCES, &ilm->ilm_v6addr,
			    allow, rp);
		if (!SLIST_IS_EMPTY(block))
			rp = mcast_bldmrec(BLOCK_OLD_SOURCES, &ilm->ilm_v6addr,
			    block, rp);
		l_free(a_minus_b);
		l_free(b_minus_a);
	} else if (ilm->ilm_fmode == MODE_IS_INCLUDE) {
send_to_ex:
		rp = mcast_bldmrec(CHANGE_TO_EXCLUDE, &ilm->ilm_v6addr, flist,
		    NULL);
	} else {
send_to_in:
		rp = mcast_bldmrec(CHANGE_TO_INCLUDE, &ilm->ilm_v6addr, flist,
		    NULL);
	}

	/*
	 * Need to set up retransmission state; merge the new info with the
	 * current state (which may be null).  If the timer is not currently
	 * running, start it (need to do a delayed start of the timer as
	 * we're currently in the sq).
	 */
	rp = mcast_merge_rtx(ilm, rp, flist);
	if (ilm->ilm_rtx.rtx_timer == INFINITY) {
		MCAST_RANDOM_DELAY(ilm->ilm_rtx.rtx_timer,
		    SEC_TO_MSEC(IGMP_MAX_HOST_REPORT_DELAY));
		mutex_enter(&igmp_timer_lock);
		igmp_deferred_next = MIN(igmp_deferred_next,
		    ilm->ilm_rtx.rtx_timer);
		mutex_exit(&igmp_timer_lock);
	}

	mutex_exit(&ill->ill_lock);
	igmpv3_sendrpt(ilm->ilm_ipif, rp);
}

void
mld_statechange(ilm_t *ilm, mcast_record_t fmode, slist_t *flist)
{
	ill_t *ill;
	mrec_t *rp = NULL;

	ASSERT(ilm != NULL);

	ill = ilm->ilm_ill;

	/* only need to send if we have an mldv2-capable router */
	mutex_enter(&ill->ill_lock);
	if (ill->ill_mcast_type != MLD_V2_ROUTER) {
		mutex_exit(&ill->ill_lock);
		return;
	}

	/*
	 * Compare existing (old) state with the new state passed in
	 * and send appropriate MLDv2 State Change Report.
	 *
	 *	Old State	New State	State Change Report
	 *
	 *	INCLUDE(A)	INCLUDE(B)	ALLOW(B-A),BLOCK(A-B)
	 *	EXCLUDE(A)	EXCLUDE(B)	ALLOW(A-B),BLOCK(B-A)
	 *	INCLUDE(A)	EXCLUDE(B)	TO_EX(B)
	 *	EXCLUDE(A)	INCLUDE(B)	TO_IN(B)
	 */
	if (ilm->ilm_fmode == fmode) {
		slist_t	*a_minus_b = NULL, *b_minus_a = NULL;
		slist_t *allow, *block;
		if (((a_minus_b = l_alloc()) == NULL) ||
		    ((b_minus_a = l_alloc()) == NULL)) {
			l_free(a_minus_b);
			if (ilm->ilm_fmode == MODE_IS_INCLUDE)
				goto send_to_ex;
			else
				goto send_to_in;
		}
		l_difference(ilm->ilm_filter, flist, a_minus_b);
		l_difference(flist, ilm->ilm_filter, b_minus_a);
		if (ilm->ilm_fmode == MODE_IS_INCLUDE) {
			allow = b_minus_a;
			block = a_minus_b;
		} else {
			allow = a_minus_b;
			block = b_minus_a;
		}
		if (!SLIST_IS_EMPTY(allow))
			rp = mcast_bldmrec(ALLOW_NEW_SOURCES, &ilm->ilm_v6addr,
			    allow, rp);
		if (!SLIST_IS_EMPTY(block))
			rp = mcast_bldmrec(BLOCK_OLD_SOURCES, &ilm->ilm_v6addr,
			    block, rp);
		l_free(a_minus_b);
		l_free(b_minus_a);
	} else if (ilm->ilm_fmode == MODE_IS_INCLUDE) {
send_to_ex:
		rp = mcast_bldmrec(CHANGE_TO_EXCLUDE, &ilm->ilm_v6addr, flist,
		    NULL);
	} else {
send_to_in:
		rp = mcast_bldmrec(CHANGE_TO_INCLUDE, &ilm->ilm_v6addr, flist,
		    NULL);
	}

	/*
	 * Need to set up retransmission state; merge the new info with the
	 * current state (which may be null).  If the timer is not currently
	 * running, start it (need to do a deferred start of the timer as
	 * we're currently in the sq).
	 */
	rp = mcast_merge_rtx(ilm, rp, flist);
	ASSERT(ilm->ilm_rtx.rtx_cnt > 0);
	if (ilm->ilm_rtx.rtx_timer == INFINITY) {
		MCAST_RANDOM_DELAY(ilm->ilm_rtx.rtx_timer,
		    SEC_TO_MSEC(ICMP6_MAX_HOST_REPORT_DELAY));
		mutex_enter(&mld_timer_lock);
		mld_deferred_next =
		    MIN(mld_deferred_next, ilm->ilm_rtx.rtx_timer);
		mutex_exit(&mld_timer_lock);
	}

	mutex_exit(&ill->ill_lock);
	mldv2_sendrpt(ill, rp);
}

uint_t
igmp_timeout_handler_per_ill(ill_t *ill, int elapsed)
{
	uint_t	next = INFINITY;
	ilm_t	*ilm;
	ipif_t	*ipif;
	mrec_t	*rp = NULL;
	mrec_t	*rtxrp = NULL;
	rtx_state_t *rtxp;
	mcast_record_t	rtype;

	ASSERT(IAM_WRITER_ILL(ill));

	mutex_enter(&ill->ill_lock);

	/* First check the global timer on this interface */
	if (ill->ill_global_timer == INFINITY)
		goto per_ilm_timer;
	if (ill->ill_global_timer <= elapsed) {
		ill->ill_global_timer = INFINITY;
		/*
		 * Send report for each group on this interface.
		 * Since we just set the global timer (received a v3 general
		 * query), need to skip the all hosts addr (224.0.0.1), per
		 * RFC 3376 section 5.
		 */
		for (ilm = ill->ill_ilm; ilm != NULL; ilm = ilm->ilm_next) {
			if (ilm->ilm_addr == htonl(INADDR_ALLHOSTS_GROUP))
				continue;
			ASSERT(ilm->ilm_ipif != NULL);
			ilm->ilm_ipif->ipif_igmp_rpt =
			    mcast_bldmrec(ilm->ilm_fmode, &ilm->ilm_v6addr,
			    ilm->ilm_filter, ilm->ilm_ipif->ipif_igmp_rpt);
			/*
			 * Since we're sending a report on this group, okay
			 * to delete pending group-specific timers.  Note
			 * that group-specific retransmit timers still need
			 * to be checked in the per_ilm_timer for-loop.
			 */
			ilm->ilm_timer = INFINITY;
			ilm->ilm_state = IGMP_IREPORTEDLAST;
			FREE_SLIST(ilm->ilm_pendsrcs);
			ilm->ilm_pendsrcs = NULL;
		}
		/*
		 * We've built per-ipif mrec lists; walk the ill's ipif list
		 * and send a report for each ipif that has an mrec list.
		 */
		for (ipif = ill->ill_ipif; ipif != NULL;
		    ipif = ipif->ipif_next) {
			if (ipif->ipif_igmp_rpt == NULL)
				continue;
			mutex_exit(&ill->ill_lock);
			igmpv3_sendrpt(ipif, ipif->ipif_igmp_rpt);
			mutex_enter(&ill->ill_lock);
			/* mrec list was freed by igmpv3_sendrpt() */
			ipif->ipif_igmp_rpt = NULL;
		}
	} else {
		ill->ill_global_timer -= elapsed;
		if (ill->ill_global_timer < next)
			next = ill->ill_global_timer;
	}

per_ilm_timer:
	for (ilm = ill->ill_ilm; ilm != NULL; ilm = ilm->ilm_next) {
		if (ilm->ilm_timer == INFINITY)
			goto per_ilm_rtxtimer;

		if (ilm->ilm_timer > elapsed) {
			ilm->ilm_timer -= elapsed;
			if (ilm->ilm_timer < next)
				next = ilm->ilm_timer;

			if (ip_debug > 1) {
				(void) mi_strlog(ill->ill_rq, 1, SL_TRACE,
				    "igmp_timo_hlr 2: ilm_timr %d elap %d "
				    "typ %d nxt %d",
				    (int)ntohl(ilm->ilm_timer), elapsed,
				    (ill->ill_mcast_type), next);
			}

			goto per_ilm_rtxtimer;
		}

		/* the timer has expired, need to take action */
		ilm->ilm_timer = INFINITY;
		ilm->ilm_state = IGMP_IREPORTEDLAST;
		if (ill->ill_mcast_type == IGMP_V1_ROUTER) {
			mutex_exit(&ill->ill_lock);
			igmp_sendpkt(ilm, IGMP_V1_MEMBERSHIP_REPORT, 0);
			mutex_enter(&ill->ill_lock);
		} else if (ill->ill_mcast_type == IGMP_V2_ROUTER) {
			mutex_exit(&ill->ill_lock);
			igmp_sendpkt(ilm, IGMP_V2_MEMBERSHIP_REPORT, 0);
			mutex_enter(&ill->ill_lock);
		} else {
			slist_t *rsp;
			if (!SLIST_IS_EMPTY(ilm->ilm_pendsrcs) &&
			    (rsp = l_alloc()) != NULL) {
				/*
				 * Contents of reply depend on pending
				 * requested source list.
				 */
				if (ilm->ilm_fmode == MODE_IS_INCLUDE) {
					l_intersection(ilm->ilm_filter,
					    ilm->ilm_pendsrcs, rsp);
				} else {
					l_difference(ilm->ilm_pendsrcs,
					    ilm->ilm_filter, rsp);
				}
				FREE_SLIST(ilm->ilm_pendsrcs);
				ilm->ilm_pendsrcs = NULL;
				if (!SLIST_IS_EMPTY(rsp))
					rp = mcast_bldmrec(MODE_IS_INCLUDE,
					    &ilm->ilm_v6addr, rsp, rp);
				FREE_SLIST(rsp);
			} else {
				/*
				 * Either the pending request is just group-
				 * specific, or we couldn't get the resources
				 * (rsp) to build a source-specific reply.
				 */
				rp = mcast_bldmrec(ilm->ilm_fmode,
				    &ilm->ilm_v6addr, ilm->ilm_filter, rp);
			}
			mutex_exit(&ill->ill_lock);
			igmpv3_sendrpt(ill->ill_ipif, rp);
			mutex_enter(&ill->ill_lock);
			rp = NULL;
		}

		if (ip_debug > 1) {
			(void) mi_strlog(ill->ill_rq, 1, SL_TRACE,
			    "igmp_timo_hlr 1: ilm_timr %d elap %d "
			    "typ %d nxt %d",
			    (int)ntohl(ilm->ilm_timer), elapsed,
			    (ill->ill_mcast_type), next);
		}

per_ilm_rtxtimer:
		rtxp = &ilm->ilm_rtx;

		if (rtxp->rtx_timer == INFINITY)
			continue;
		if (rtxp->rtx_timer > elapsed) {
			rtxp->rtx_timer -= elapsed;
			if (rtxp->rtx_timer < next)
				next = rtxp->rtx_timer;
			continue;
		}

		rtxp->rtx_timer = INFINITY;
		ilm->ilm_state = IGMP_IREPORTEDLAST;
		if (ill->ill_mcast_type == IGMP_V1_ROUTER) {
			mutex_exit(&ill->ill_lock);
			igmp_sendpkt(ilm, IGMP_V1_MEMBERSHIP_REPORT, 0);
			mutex_enter(&ill->ill_lock);
			continue;
		} else if (ill->ill_mcast_type == IGMP_V2_ROUTER) {
			mutex_exit(&ill->ill_lock);
			igmp_sendpkt(ilm, IGMP_V2_MEMBERSHIP_REPORT, 0);
			mutex_enter(&ill->ill_lock);
			continue;
		}

		/*
		 * The retransmit timer has popped, and our router is
		 * IGMPv3.  We have to delve into the retransmit state
		 * stored in the ilm.
		 *
		 * Decrement the retransmit count.  If the fmode rtx
		 * count is active, decrement it, and send a filter
		 * mode change report with the ilm's source list.
		 * Otherwise, send a source list change report with
		 * the current retransmit lists.
		 */
		ASSERT(rtxp->rtx_cnt > 0);
		ASSERT(rtxp->rtx_cnt >= rtxp->rtx_fmode_cnt);
		rtxp->rtx_cnt--;
		if (rtxp->rtx_fmode_cnt > 0) {
			rtxp->rtx_fmode_cnt--;
			rtype = (ilm->ilm_fmode == MODE_IS_INCLUDE) ?
			    CHANGE_TO_INCLUDE : CHANGE_TO_EXCLUDE;
			rtxrp = mcast_bldmrec(rtype, &ilm->ilm_v6addr,
			    ilm->ilm_filter, rtxrp);
		} else {
			rtxrp = mcast_bldmrec(ALLOW_NEW_SOURCES,
			    &ilm->ilm_v6addr, rtxp->rtx_allow, rtxrp);
			rtxrp = mcast_bldmrec(BLOCK_OLD_SOURCES,
			    &ilm->ilm_v6addr, rtxp->rtx_block, rtxrp);
		}
		if (rtxp->rtx_cnt > 0) {
			MCAST_RANDOM_DELAY(rtxp->rtx_timer,
			    SEC_TO_MSEC(IGMP_MAX_HOST_REPORT_DELAY));
			if (rtxp->rtx_timer < next)
				next = rtxp->rtx_timer;
		} else {
			CLEAR_SLIST(rtxp->rtx_allow);
			CLEAR_SLIST(rtxp->rtx_block);
		}
		mutex_exit(&ill->ill_lock);
		igmpv3_sendrpt(ilm->ilm_ipif, rtxrp);
		mutex_enter(&ill->ill_lock);
		rtxrp = NULL;
	}

	mutex_exit(&ill->ill_lock);

	return (next);
}

/*
 * igmp_timeout_handler:
 * Called when there are timeout events, every next * TMEOUT_INTERVAL (tick).
 * Returns number of ticks to next event (or 0 if none).
 *
 * As part of multicast join and leave igmp we may need to send out an
 * igmp request. The igmp related state variables in the ilm are protected
 * by ill_lock. A single global igmp timer is used to track igmp timeouts.
 * igmp_timer_lock protects the global igmp_timeout_id. igmp_start_timers
 * starts the igmp timer if needed. It serializes multiple threads trying to
 * simultaneously start the timer using the igmp_timer_setter_active flag.
 *
 * igmp_input() receives igmp queries and responds to the queries
 * in a delayed fashion by posting a timer i.e. it calls igmp_start_timers().
 * Later the igmp_timer fires, the timeout handler igmp_timerout_handler()
 * performs the action exclusively after entering each ill's ipsq as writer.
 * The actual igmp timeout handler needs to run in the ipsq since it has to
 * access the ilm's and we don't want another exclusive operation like
 * say an IPMP failover to be simultaneously moving the ilms from one ill to
 * another.
 *
 * The igmp_slowtimeo() function is called thru another timer.
 * igmp_slowtimeout_lock protects the igmp_slowtimeout_id
 */

/* ARGSUSED */
void
igmp_timeout_handler(void *arg)
{
	ill_t	*ill;
	int	elapsed;	/* Since last call */
	uint_t  global_next = INFINITY;
	uint_t  next;
	ill_walk_context_t ctx;
	boolean_t success;

	mutex_enter(&igmp_timer_lock);
	ASSERT(igmp_timeout_id != 0);
	igmp_timer_fired_last = ddi_get_lbolt();
	elapsed = igmp_time_to_next;
	igmp_time_to_next = 0;
	mutex_exit(&igmp_timer_lock);

	rw_enter(&ill_g_lock, RW_READER);
	ill = ILL_START_WALK_V4(&ctx);
	for (; ill != NULL; ill = ill_next(&ctx, ill)) {
		ASSERT(!ill->ill_isv6);
		/*
		 * We may not be able to refhold the ill if the ill/ipif
		 * is changing. But we need to make sure that the ill will
		 * not vanish. So we just bump up the ill_waiter count.
		 */
		if (!ill_waiter_inc(ill))
			continue;
		rw_exit(&ill_g_lock);
		success = ipsq_enter(ill, B_TRUE);
		if (success) {
			next = igmp_timeout_handler_per_ill(ill, elapsed);
			if (next < global_next)
				global_next = next;
			ipsq_exit(ill->ill_phyint->phyint_ipsq, B_FALSE,
			    B_TRUE);
		}
		rw_enter(&ill_g_lock, RW_READER);
		ill_waiter_dcr(ill);
	}
	rw_exit(&ill_g_lock);

	mutex_enter(&igmp_timer_lock);
	ASSERT(igmp_timeout_id != 0);
	igmp_timeout_id = 0;
	mutex_exit(&igmp_timer_lock);

	if (global_next != INFINITY)
		igmp_start_timers(global_next);
}

/*
 * mld_timeout_handler:
 * Called when there are timeout events, every next (tick).
 * Returns number of ticks to next event (or 0 if none).
 */
/* ARGSUSED */
uint_t
mld_timeout_handler_per_ill(ill_t *ill, int elapsed)
{
	ilm_t 	*ilm;
	uint_t	next = INFINITY;
	mrec_t	*rp, *rtxrp;
	rtx_state_t *rtxp;
	mcast_record_t	rtype;

	ASSERT(IAM_WRITER_ILL(ill));

	mutex_enter(&ill->ill_lock);

	/*
	 * First check the global timer on this interface; the global timer
	 * is not used for MLDv1, so if it's set we can assume we're v2.
	 */
	if (ill->ill_global_timer == INFINITY)
		goto per_ilm_timer;
	if (ill->ill_global_timer <= elapsed) {
		ill->ill_global_timer = INFINITY;
		/*
		 * Send report for each group on this interface.
		 * Since we just set the global timer (received a v2 general
		 * query), need to skip the all hosts addr (ff02::1), per
		 * RFC 3810 section 6.
		 */
		rp = NULL;
		for (ilm = ill->ill_ilm; ilm != NULL; ilm = ilm->ilm_next) {
			if (IN6_ARE_ADDR_EQUAL(&ilm->ilm_v6addr,
			    &ipv6_all_hosts_mcast))
				continue;
			rp = mcast_bldmrec(ilm->ilm_fmode, &ilm->ilm_v6addr,
			    ilm->ilm_filter, rp);
			/*
			 * Since we're sending a report on this group, okay
			 * to delete pending group-specific timers.  Note
			 * that group-specific retransmit timers still need
			 * to be checked in the per_ilm_timer for-loop.
			 */
			ilm->ilm_timer = INFINITY;
			ilm->ilm_state = IGMP_IREPORTEDLAST;
			FREE_SLIST(ilm->ilm_pendsrcs);
			ilm->ilm_pendsrcs = NULL;
		}
		mutex_exit(&ill->ill_lock);
		mldv2_sendrpt(ill, rp);
		mutex_enter(&ill->ill_lock);
	} else {
		ill->ill_global_timer -= elapsed;
		if (ill->ill_global_timer < next)
			next = ill->ill_global_timer;
	}

per_ilm_timer:
	rp = rtxrp = NULL;
	for (ilm = ill->ill_ilm; ilm != NULL; ilm = ilm->ilm_next) {
		if (ilm->ilm_timer == INFINITY)
			goto per_ilm_rtxtimer;

		if (ilm->ilm_timer > elapsed) {
			ilm->ilm_timer -= elapsed;
			if (ilm->ilm_timer < next)
				next = ilm->ilm_timer;

			if (ip_debug > 1) {
				(void) mi_strlog(ill->ill_rq, 1, SL_TRACE,
				    "igmp_timo_hlr 2: ilm_timr"
				    " %d elap %d typ %d nxt %d",
				    (int)ntohl(ilm->ilm_timer), elapsed,
				    (ill->ill_mcast_type), next);
			}

			goto per_ilm_rtxtimer;
		}

		/* the timer has expired, need to take action */
		ilm->ilm_timer = INFINITY;
		ilm->ilm_state = IGMP_IREPORTEDLAST;
		if (ill->ill_mcast_type == MLD_V1_ROUTER) {
			mutex_exit(&ill->ill_lock);
			mld_sendpkt(ilm, MLD_LISTENER_REPORT, NULL);
			mutex_enter(&ill->ill_lock);
		} else {
			slist_t *rsp;
			if (!SLIST_IS_EMPTY(ilm->ilm_pendsrcs) &&
			    (rsp = l_alloc()) != NULL) {
				/*
				 * Contents of reply depend on pending
				 * requested source list.
				 */
				if (ilm->ilm_fmode == MODE_IS_INCLUDE) {
					l_intersection(ilm->ilm_filter,
					    ilm->ilm_pendsrcs, rsp);
				} else {
					l_difference(ilm->ilm_pendsrcs,
					    ilm->ilm_filter, rsp);
				}
				FREE_SLIST(ilm->ilm_pendsrcs);
				ilm->ilm_pendsrcs = NULL;
				if (!SLIST_IS_EMPTY(rsp))
					rp = mcast_bldmrec(MODE_IS_INCLUDE,
					    &ilm->ilm_v6addr, rsp, rp);
				FREE_SLIST(rsp);
			} else {
				rp = mcast_bldmrec(ilm->ilm_fmode,
				    &ilm->ilm_v6addr, ilm->ilm_filter, rp);
			}
		}

		if (ip_debug > 1) {
			(void) mi_strlog(ill->ill_rq, 1, SL_TRACE,
			    "igmp_timo_hlr 1: ilm_timr %d elap %d "
			    "typ %d nxt %d",
			    (int)ntohl(ilm->ilm_timer), elapsed,
			    (ill->ill_mcast_type), next);
		}

per_ilm_rtxtimer:
		rtxp = &ilm->ilm_rtx;

		if (rtxp->rtx_timer == INFINITY)
			continue;
		if (rtxp->rtx_timer > elapsed) {
			rtxp->rtx_timer -= elapsed;
			if (rtxp->rtx_timer < next)
				next = rtxp->rtx_timer;
			continue;
		}

		rtxp->rtx_timer = INFINITY;
		ilm->ilm_state = IGMP_IREPORTEDLAST;
		if (ill->ill_mcast_type == MLD_V1_ROUTER) {
			mutex_exit(&ill->ill_lock);
			mld_sendpkt(ilm, MLD_LISTENER_REPORT, NULL);
			mutex_enter(&ill->ill_lock);
			continue;
		}

		/*
		 * The retransmit timer has popped, and our router is
		 * MLDv2.  We have to delve into the retransmit state
		 * stored in the ilm.
		 *
		 * Decrement the retransmit count.  If the fmode rtx
		 * count is active, decrement it, and send a filter
		 * mode change report with the ilm's source list.
		 * Otherwise, send a source list change report with
		 * the current retransmit lists.
		 */
		ASSERT(rtxp->rtx_cnt > 0);
		ASSERT(rtxp->rtx_cnt >= rtxp->rtx_fmode_cnt);
		rtxp->rtx_cnt--;
		if (rtxp->rtx_fmode_cnt > 0) {
			rtxp->rtx_fmode_cnt--;
			rtype = (ilm->ilm_fmode == MODE_IS_INCLUDE) ?
			    CHANGE_TO_INCLUDE : CHANGE_TO_EXCLUDE;
			rtxrp = mcast_bldmrec(rtype, &ilm->ilm_v6addr,
			    ilm->ilm_filter, rtxrp);
		} else {
			rtxrp = mcast_bldmrec(ALLOW_NEW_SOURCES,
			    &ilm->ilm_v6addr, rtxp->rtx_allow, rtxrp);
			rtxrp = mcast_bldmrec(BLOCK_OLD_SOURCES,
			    &ilm->ilm_v6addr, rtxp->rtx_block, rtxrp);
		}
		if (rtxp->rtx_cnt > 0) {
			MCAST_RANDOM_DELAY(rtxp->rtx_timer,
			    SEC_TO_MSEC(ICMP6_MAX_HOST_REPORT_DELAY));
			if (rtxp->rtx_timer < next)
				next = rtxp->rtx_timer;
		} else {
			CLEAR_SLIST(rtxp->rtx_allow);
			CLEAR_SLIST(rtxp->rtx_block);
		}
	}

	if (ill->ill_mcast_type == MLD_V2_ROUTER) {
		mutex_exit(&ill->ill_lock);
		mldv2_sendrpt(ill, rp);
		mldv2_sendrpt(ill, rtxrp);
		return (next);
	}

	mutex_exit(&ill->ill_lock);

	return (next);
}

/*
 * mld_timeout_handler:
 * Called when there are timeout events, every next * TMEOUT_INTERVAL (tick).
 * Returns number of ticks to next event (or 0 if none).
 * MT issues are same as igmp_timeout_handler
 */
/* ARGSUSED */
void
mld_timeout_handler(void *arg)
{
	ill_t	*ill;
	int	elapsed;	/* Since last call */
	uint_t  global_next = INFINITY;
	uint_t  next;
	ill_walk_context_t ctx;
	boolean_t success;

	mutex_enter(&mld_timer_lock);
	ASSERT(mld_timeout_id != 0);
	mld_timer_fired_last = ddi_get_lbolt();
	elapsed = mld_time_to_next;
	mld_time_to_next = 0;
	mutex_exit(&mld_timer_lock);

	rw_enter(&ill_g_lock, RW_READER);
	ill = ILL_START_WALK_V6(&ctx);
	for (; ill != NULL; ill = ill_next(&ctx, ill)) {
		ASSERT(ill->ill_isv6);
		/*
		 * We may not be able to refhold the ill if the ill/ipif
		 * is changing. But we need to make sure that the ill will
		 * not vanish. So we just bump up the ill_waiter count.
		 */
		if (!ill_waiter_inc(ill))
			continue;
		rw_exit(&ill_g_lock);
		success = ipsq_enter(ill, B_TRUE);
		if (success) {
			next = mld_timeout_handler_per_ill(ill, elapsed);
			if (next < global_next)
				global_next = next;
			ipsq_exit(ill->ill_phyint->phyint_ipsq, B_TRUE,
			    B_FALSE);
		}
		rw_enter(&ill_g_lock, RW_READER);
		ill_waiter_dcr(ill);
	}
	rw_exit(&ill_g_lock);

	mutex_enter(&mld_timer_lock);
	ASSERT(mld_timeout_id != 0);
	mld_timeout_id = 0;
	mutex_exit(&mld_timer_lock);

	if (global_next != INFINITY)
		mld_start_timers(global_next);
}

/*
 * Calculate the Older Version Querier Present timeout value, in number
 * of slowtimo intervals, for the given ill.
 */
#define	OVQP(ill) \
	((1000 * (((ill)->ill_mcast_rv * (ill)->ill_mcast_qi) \
	+ MCAST_QUERY_RESP_INTERVAL)) / MCAST_SLOWTIMO_INTERVAL)

/*
 * igmp_slowtimo:
 * - Resets to new router if we didnt we hear from the router
 *   in IGMP_AGE_THRESHOLD seconds.
 * - Resets slowtimeout.
 */
/* ARGSUSED */
void
igmp_slowtimo(void *arg)
{
	ill_t	*ill;
	ill_if_t *ifp;
	avl_tree_t *avl_tree;

	/* Hold the ill_g_lock so that we can safely walk the ill list */
	rw_enter(&ill_g_lock, RW_READER);

	/*
	 * The ill_if_t list is circular, hence the odd loop parameters.
	 *
	 * We can't use the ILL_START_WALK and ill_next() wrappers for this
	 * walk, as we need to check the illif_mcast_* fields in the ill_if_t
	 * structure (allowing us to skip if none of the instances have timers
	 * running).
	 */
	for (ifp = IP_V4_ILL_G_LIST; ifp != (ill_if_t *)&IP_V4_ILL_G_LIST;
	    ifp = ifp->illif_next) {
		/*
		 * illif_mcast_v[12] are set using atomics. If an ill hears
		 * a V1 or V2 query now and we miss seeing the count now,
		 * we will see it the next time igmp_slowtimo is called.
		 */
		if (ifp->illif_mcast_v1 == 0 && ifp->illif_mcast_v2 == 0)
			continue;

		avl_tree = &ifp->illif_avl_by_ppa;
		for (ill = avl_first(avl_tree); ill != NULL;
		    ill = avl_walk(avl_tree, ill, AVL_AFTER)) {
			mutex_enter(&ill->ill_lock);
			if (ill->ill_mcast_v1_tset == 1)
				ill->ill_mcast_v1_time++;
			if (ill->ill_mcast_v2_tset == 1)
				ill->ill_mcast_v2_time++;
			if (ill->ill_mcast_type == IGMP_V1_ROUTER) {
				if (ill->ill_mcast_v1_time >= OVQP(ill)) {
					if (ill->ill_mcast_v2_tset > 0) {
						ip1dbg(("V1 query timer "
						    "expired on %s; switching "
						    "mode to IGMP_V2\n",
						    ill->ill_name));
						ill->ill_mcast_type =
						    IGMP_V2_ROUTER;
					} else {
						ip1dbg(("V1 query timer "
						    "expired on %s; switching "
						    "mode to IGMP_V3\n",
						    ill->ill_name));
						ill->ill_mcast_type =
						    IGMP_V3_ROUTER;
					}
					ill->ill_mcast_v1_time = 0;
					ill->ill_mcast_v1_tset = 0;
					atomic_add_16(&ifp->illif_mcast_v1, -1);
				}
			}
			if (ill->ill_mcast_type == IGMP_V2_ROUTER) {
				if (ill->ill_mcast_v2_time >= OVQP(ill)) {
					ip1dbg(("V2 query timer expired on "
					    "%s; switching mode to IGMP_V3\n",
					    ill->ill_name));
					ill->ill_mcast_type = IGMP_V3_ROUTER;
					ill->ill_mcast_v2_time = 0;
					ill->ill_mcast_v2_tset = 0;
					atomic_add_16(&ifp->illif_mcast_v2, -1);
				}
			}
			mutex_exit(&ill->ill_lock);
		}

	}
	rw_exit(&ill_g_lock);
	mutex_enter(&igmp_slowtimeout_lock);
	igmp_slowtimeout_id = timeout(igmp_slowtimo, NULL,
		MSEC_TO_TICK(MCAST_SLOWTIMO_INTERVAL));
	mutex_exit(&igmp_slowtimeout_lock);
}

/*
 * mld_slowtimo:
 * - Resets to newer version if we didn't hear from the older version router
 *   in MLD_AGE_THRESHOLD seconds.
 * - Restarts slowtimeout.
 */
/* ARGSUSED */
void
mld_slowtimo(void *arg)
{
	ill_t *ill;
	ill_if_t *ifp;
	avl_tree_t *avl_tree;

	/* See comments in igmp_slowtimo() above... */
	rw_enter(&ill_g_lock, RW_READER);
	for (ifp = IP_V6_ILL_G_LIST; ifp != (ill_if_t *)&IP_V6_ILL_G_LIST;
	    ifp = ifp->illif_next) {

		if (ifp->illif_mcast_v1 == 0)
			continue;

		avl_tree = &ifp->illif_avl_by_ppa;
		for (ill = avl_first(avl_tree); ill != NULL;
		    ill = avl_walk(avl_tree, ill, AVL_AFTER)) {
			mutex_enter(&ill->ill_lock);
			if (ill->ill_mcast_v1_tset == 1)
				ill->ill_mcast_v1_time++;
			if (ill->ill_mcast_type == MLD_V1_ROUTER) {
				if (ill->ill_mcast_v1_time >= OVQP(ill)) {
					ip1dbg(("MLD query timer expired on"
					    " %s; switching mode to MLD_V2\n",
					    ill->ill_name));
					ill->ill_mcast_type = MLD_V2_ROUTER;
					ill->ill_mcast_v1_time = 0;
					ill->ill_mcast_v1_tset = 0;
					atomic_add_16(&ifp->illif_mcast_v1, -1);
				}
			}
			mutex_exit(&ill->ill_lock);
		}
	}
	rw_exit(&ill_g_lock);
	mutex_enter(&mld_slowtimeout_lock);
	mld_slowtimeout_id = timeout(mld_slowtimo, NULL,
	    MSEC_TO_TICK(MCAST_SLOWTIMO_INTERVAL));
	mutex_exit(&mld_slowtimeout_lock);
}

/*
 * igmp_sendpkt:
 * This will send to ip_wput like icmp_inbound.
 * Note that the lower ill (on which the membership is kept) is used
 * as an upper ill to pass in the multicast parameters.
 */
static void
igmp_sendpkt(ilm_t *ilm, uchar_t type, ipaddr_t addr)
{
	mblk_t	*mp;
	igmpa_t	*igmpa;
	uint8_t *rtralert;
	ipha_t	*ipha;
	int	hdrlen = sizeof (ipha_t) + RTRALERT_LEN;
	size_t	size  = hdrlen + sizeof (igmpa_t);
	ipif_t 	*ipif = ilm->ilm_ipif;
	ill_t 	*ill  = ipif->ipif_ill;	/* Will be the "lower" ill */
	mblk_t	*first_mp;
	ipsec_out_t *io;
	zoneid_t zoneid;

	/*
	 * We need to make sure this packet goes out on an ipif. If
	 * there is some global policy match in ip_wput_ire, we need
	 * to get to the right interface after IPSEC processing.
	 * To make sure this multicast packet goes out on the right
	 * interface, we attach an ipsec_out and initialize ill_index
	 * like we did in ip_wput. To make sure that this packet does
	 * not get forwarded on other interfaces or looped back, we
	 * set ipsec_out_dontroute to B_TRUE and ipsec_out_multicast_loop
	 * to B_FALSE.
	 *
	 * We also need to make sure that this does not get load balanced
	 * if it hits ip_newroute_ipif. So, we initialize ipsec_out_attach_if
	 * here. If it gets load balanced, switches supporting igmp snooping
	 * will send the packet that it receives for this multicast group
	 * to the interface that we are sending on. As we have joined the
	 * multicast group on this ill, by sending the packet out on this
	 * ill, we receive all the packets back on this ill.
	 */
	first_mp = allocb(sizeof (ipsec_info_t), BPRI_HI);
	if (first_mp == NULL)
		return;

	first_mp->b_datap->db_type = M_CTL;
	first_mp->b_wptr += sizeof (ipsec_info_t);
	bzero(first_mp->b_rptr, sizeof (ipsec_info_t));
	/* ipsec_out_secure is B_FALSE now */
	io = (ipsec_out_t *)first_mp->b_rptr;
	io->ipsec_out_type = IPSEC_OUT;
	io->ipsec_out_len = sizeof (ipsec_out_t);
	io->ipsec_out_use_global_policy = B_TRUE;
	io->ipsec_out_ill_index = ill->ill_phyint->phyint_ifindex;
	io->ipsec_out_attach_if = B_TRUE;
	io->ipsec_out_multicast_loop = B_FALSE;
	io->ipsec_out_dontroute = B_TRUE;
	if ((zoneid = ilm->ilm_zoneid) == ALL_ZONES)
		zoneid = GLOBAL_ZONEID;
	io->ipsec_out_zoneid = zoneid;

	mp = allocb(size, BPRI_HI);
	if (mp == NULL) {
		freemsg(first_mp);
		return;
	}
	mp->b_wptr = mp->b_rptr + size;
	first_mp->b_cont = mp;

	ipha = (ipha_t *)mp->b_rptr;
	rtralert = (uint8_t *)&(ipha[1]);
	igmpa = (igmpa_t *)&(rtralert[RTRALERT_LEN]);
	igmpa->igmpa_type   = type;
	igmpa->igmpa_code   = 0;
	igmpa->igmpa_group  = ilm->ilm_addr;
	igmpa->igmpa_cksum  = 0;
	igmpa->igmpa_cksum  = IP_CSUM(mp, hdrlen, 0);
	if (igmpa->igmpa_cksum == 0)
		igmpa->igmpa_cksum = 0xffff;

	rtralert[0] = IPOPT_COPY | IPOPT_RTRALERT;
	rtralert[1] = RTRALERT_LEN;
	rtralert[2] = 0;
	rtralert[3] = 0;

	ipha->ipha_version_and_hdr_length = (IP_VERSION << 4)
	    | (IP_SIMPLE_HDR_LENGTH_IN_WORDS + RTRALERT_LEN_IN_WORDS);
	ipha->ipha_type_of_service 	= 0;
	ipha->ipha_length = htons(size);
	ipha->ipha_ident = 0;
	ipha->ipha_fragment_offset_and_flags = 0;
	ipha->ipha_ttl 		= IGMP_TTL;
	ipha->ipha_protocol 	= IPPROTO_IGMP;
	ipha->ipha_hdr_checksum 	= 0;
	ipha->ipha_dst 		= addr ? addr : igmpa->igmpa_group;
	ipha->ipha_src 		= ipif->ipif_src_addr;
	/*
	 * Request loopback of the report if we are acting as a multicast
	 * router, so that the process-level routing demon can hear it.
	 */
	/*
	 * This will run multiple times for the same group if there are members
	 * on the same group for multiple ipif's on the same ill. The
	 * igmp_input code will suppress this due to the loopback thus we
	 * always loopback membership report.
	 */
	ASSERT(ill->ill_rq != NULL);
	ip_multicast_loopback(ill->ill_rq, ill, first_mp, 0, ilm->ilm_zoneid);

	ip_wput_multicast(ill->ill_wq, first_mp, ipif);

	++igmpstat.igps_snd_reports;
}

/*
 * Sends an IGMP_V3_MEMBERSHIP_REPORT message out the ill associated
 * with the passed-in ipif.  The report will contain one group record
 * for each element of reclist.  If this causes packet length to
 * exceed ipif->ipif_ill->ill_max_frag, multiple reports are sent.
 * reclist is assumed to be made up of buffers allocated by mcast_bldmrec(),
 * and those buffers are freed here.
 */
static void
igmpv3_sendrpt(ipif_t *ipif, mrec_t *reclist)
{
	ipsec_out_t *io;
	igmp3ra_t *igmp3ra;
	grphdra_t *grphdr;
	mblk_t *first_mp, *mp;
	ipha_t *ipha;
	uint8_t *rtralert;
	ipaddr_t *src_array;
	int i, j, numrec, more_src_cnt;
	size_t hdrsize, size, rsize;
	ill_t *ill = ipif->ipif_ill;
	mrec_t *rp, *cur_reclist;
	mrec_t *next_reclist = reclist;
	boolean_t morepkts;
	zoneid_t zoneid;

	/* if there aren't any records, there's nothing to send */
	if (reclist == NULL)
		return;

	hdrsize = sizeof (ipha_t) + RTRALERT_LEN;
nextpkt:
	size = hdrsize + sizeof (igmp3ra_t);
	morepkts = B_FALSE;
	more_src_cnt = 0;
	cur_reclist = next_reclist;
	numrec = 0;
	for (rp = cur_reclist; rp != NULL; rp = rp->mrec_next) {
		rsize = sizeof (grphdra_t) +
		    (rp->mrec_srcs.sl_numsrc * sizeof (ipaddr_t));
		if (size + rsize > ill->ill_max_frag) {
			if (rp == cur_reclist) {
				/*
				 * If the first mrec we looked at is too big
				 * to fit in a single packet (i.e the source
				 * list is too big), we must either truncate
				 * the list (if TO_EX or IS_EX), or send
				 * multiple reports for the same group (all
				 * other types).
				 */
				int srcspace, srcsperpkt;
				srcspace = ill->ill_max_frag - (size +
				    sizeof (grphdra_t));
				srcsperpkt = srcspace / sizeof (ipaddr_t);
				/*
				 * Increment size and numrec, because we will
				 * be sending a record for the mrec we're
				 * looking at now.
				 */
				size += sizeof (grphdra_t) +
				    (srcsperpkt * sizeof (ipaddr_t));
				numrec++;
				if (rp->mrec_type == MODE_IS_EXCLUDE ||
				    rp->mrec_type == CHANGE_TO_EXCLUDE) {
					rp->mrec_srcs.sl_numsrc = srcsperpkt;
					if (rp->mrec_next == NULL) {
						/* no more packets to send */
						break;
					} else {
						/*
						 * more packets, but we're
						 * done with this mrec.
						 */
						next_reclist = rp->mrec_next;
					}
				} else {
					more_src_cnt = rp->mrec_srcs.sl_numsrc
					    - srcsperpkt;
					rp->mrec_srcs.sl_numsrc = srcsperpkt;
					/*
					 * We'll fix up this mrec (remove the
					 * srcs we've already sent) before
					 * returning to nextpkt above.
					 */
					next_reclist = rp;
				}
			} else {
				next_reclist = rp;
			}
			morepkts = B_TRUE;
			break;
		}
		size += rsize;
		numrec++;
	}

	/*
	 * See comments in igmp_sendpkt() about initializing for ipsec and
	 * load balancing requirements.
	 */
	first_mp = allocb(sizeof (ipsec_info_t), BPRI_HI);
	if (first_mp == NULL)
		goto free_reclist;

	first_mp->b_datap->db_type = M_CTL;
	first_mp->b_wptr += sizeof (ipsec_info_t);
	bzero(first_mp->b_rptr, sizeof (ipsec_info_t));
	/* ipsec_out_secure is B_FALSE now */
	io = (ipsec_out_t *)first_mp->b_rptr;
	io->ipsec_out_type = IPSEC_OUT;
	io->ipsec_out_len = sizeof (ipsec_out_t);
	io->ipsec_out_use_global_policy = B_TRUE;
	io->ipsec_out_ill_index = ill->ill_phyint->phyint_ifindex;
	io->ipsec_out_attach_if = B_TRUE;
	io->ipsec_out_multicast_loop = B_FALSE;
	io->ipsec_out_dontroute = B_TRUE;
	if ((zoneid = ipif->ipif_zoneid) == ALL_ZONES)
		zoneid = GLOBAL_ZONEID;
	io->ipsec_out_zoneid = zoneid;

	mp = allocb(size, BPRI_HI);
	if (mp == NULL) {
		freemsg(first_mp);
		goto free_reclist;
	}
	bzero((char *)mp->b_rptr, size);
	mp->b_wptr = (uchar_t *)(mp->b_rptr + size);
	first_mp->b_cont = mp;

	ipha = (ipha_t *)mp->b_rptr;
	rtralert = (uint8_t *)&(ipha[1]);
	igmp3ra = (igmp3ra_t *)&(rtralert[RTRALERT_LEN]);
	grphdr = (grphdra_t *)&(igmp3ra[1]);

	rp = cur_reclist;
	for (i = 0; i < numrec; i++) {
		grphdr->grphdra_type = rp->mrec_type;
		grphdr->grphdra_numsrc = htons(rp->mrec_srcs.sl_numsrc);
		grphdr->grphdra_group = V4_PART_OF_V6(rp->mrec_group);
		src_array = (ipaddr_t *)&(grphdr[1]);

		for (j = 0; j < rp->mrec_srcs.sl_numsrc; j++)
			src_array[j] = V4_PART_OF_V6(rp->mrec_srcs.sl_addr[j]);

		grphdr = (grphdra_t *)&(src_array[j]);
		rp = rp->mrec_next;
	}

	igmp3ra->igmp3ra_type = IGMP_V3_MEMBERSHIP_REPORT;
	igmp3ra->igmp3ra_numrec = htons(numrec);
	igmp3ra->igmp3ra_cksum = IP_CSUM(mp, hdrsize, 0);

	rtralert[0] = IPOPT_COPY | IPOPT_RTRALERT;
	rtralert[1] = RTRALERT_LEN;
	rtralert[2] = 0;
	rtralert[3] = 0;

	ipha->ipha_version_and_hdr_length = IP_VERSION << 4
	    | (IP_SIMPLE_HDR_LENGTH_IN_WORDS + RTRALERT_LEN_IN_WORDS);
	ipha->ipha_type_of_service = IPTOS_PREC_INTERNETCONTROL;
	ipha->ipha_length = htons(size);
	ipha->ipha_ttl = IGMP_TTL;
	ipha->ipha_protocol = IPPROTO_IGMP;
	ipha->ipha_dst = htonl(INADDR_ALLRPTS_GROUP);
	ipha->ipha_src = ipif->ipif_src_addr;

	/*
	 * Request loopback of the report if we are acting as a multicast
	 * router, so that the process-level routing daemon can hear it.
	 *
	 * This will run multiple times for the same group if there are
	 * members on the same group for multiple ipifs on the same ill.
	 * The igmp_input code will suppress this due to the loopback;
	 * thus we always loopback membership report.
	 */
	ASSERT(ill->ill_rq != NULL);
	ip_multicast_loopback(ill->ill_rq, ill, mp, 0, ipif->ipif_zoneid);

	ip_wput_multicast(ill->ill_wq, first_mp, ipif);

	++igmpstat.igps_snd_reports;

	if (morepkts) {
		if (more_src_cnt > 0) {
			int index, mvsize;
			slist_t *sl = &next_reclist->mrec_srcs;
			index = sl->sl_numsrc;
			mvsize = more_src_cnt * sizeof (in6_addr_t);
			(void) memmove(&sl->sl_addr[0], &sl->sl_addr[index],
			    mvsize);
			sl->sl_numsrc = more_src_cnt;
		}
		goto nextpkt;
	}

free_reclist:
	while (reclist != NULL) {
		rp = reclist->mrec_next;
		mi_free(reclist);
		reclist = rp;
	}
}

/*
 * mld_input:
 */
/* ARGSUSED */
void
mld_input(queue_t *q, mblk_t *mp, ill_t *ill)
{
	ip6_t		*ip6h = (ip6_t *)(mp->b_rptr);
	mld_hdr_t	*mldh;
	ilm_t		*ilm;
	ipif_t		*ipif;
	uint16_t	hdr_length, exthdr_length;
	in6_addr_t	*v6group_ptr, *lcladdr_ptr;
	uint_t		next;
	int		mldlen;

	BUMP_MIB(ill->ill_icmp6_mib, ipv6IfIcmpInGroupMembTotal);

	/* Make sure the src address of the packet is link-local */
	if (!(IN6_IS_ADDR_LINKLOCAL(&ip6h->ip6_src))) {
		BUMP_MIB(ill->ill_icmp6_mib, ipv6IfIcmpInErrors);
		freemsg(mp);
		return;
	}

	if (ip6h->ip6_hlim != 1) {
		BUMP_MIB(ill->ill_icmp6_mib, ipv6IfIcmpBadHoplimit);
		freemsg(mp);
		return;
	}

	/* Get to the icmp header part */
	if (ip6h->ip6_nxt != IPPROTO_ICMPV6) {
		hdr_length = ip_hdr_length_v6(mp, ip6h);
		exthdr_length = hdr_length - IPV6_HDR_LEN;
	} else {
		hdr_length = IPV6_HDR_LEN;
		exthdr_length = 0;
	}
	mldlen = ntohs(ip6h->ip6_plen) - exthdr_length;

	/* An MLD packet must at least be 24 octets to be valid */
	if (mldlen < MLD_MINLEN) {
		BUMP_MIB(ill->ill_icmp6_mib, ipv6IfIcmpInErrors);
		freemsg(mp);
		return;
	}

	mldh = (mld_hdr_t *)(&mp->b_rptr[hdr_length]);

	switch (mldh->mld_type) {
	case MLD_LISTENER_QUERY:
		/*
		 * packet length differentiates between v1 and v2.  v1
		 * query should be exactly 24 octets long; v2 is >= 28.
		 */
		if (mldlen == MLD_MINLEN) {
			next = mld_query_in(mldh, ill);
		} else if (mldlen >= MLD_V2_QUERY_MINLEN) {
			next = mldv2_query_in((mld2q_t *)mldh, ill, mldlen);
		} else {
			BUMP_MIB(ill->ill_icmp6_mib, ipv6IfIcmpInErrors);
			freemsg(mp);
			return;
		}
		if (next == 0) {
			freemsg(mp);
			return;
		}

		if (next != INFINITY)
			mld_start_timers(next);
		break;

	case MLD_LISTENER_REPORT: {

		ASSERT(ill->ill_ipif != NULL);
		/*
		 * For fast leave to work, we have to know that we are the
		 * last person to send a report for this group.  Reports
		 * generated by us are looped back since we could potentially
		 * be a multicast router, so discard reports sourced by me.
		 */
		lcladdr_ptr = &(ill->ill_ipif->ipif_v6subnet);
		mutex_enter(&ill->ill_lock);
		for (ipif = ill->ill_ipif; ipif != NULL;
		    ipif = ipif->ipif_next) {
			if (IN6_ARE_ADDR_EQUAL(&ipif->ipif_v6lcl_addr,
			    lcladdr_ptr)) {
				if (ip_debug > 1) {
					char    buf1[INET6_ADDRSTRLEN];
					char	buf2[INET6_ADDRSTRLEN];

					(void) mi_strlog(ill->ill_rq,
					    1,
					    SL_TRACE,
					    "mld_input: we are only "
					    "member src %s ipif_local %s",
					    inet_ntop(AF_INET6, lcladdr_ptr,
					    buf1, sizeof (buf1)),
					    inet_ntop(AF_INET6,
					    &ipif->ipif_v6lcl_addr,
					    buf2, sizeof (buf2)));
				}
				mutex_exit(&ill->ill_lock);
				freemsg(mp);
				return;
			}
		}
		mutex_exit(&ill->ill_lock);
		BUMP_MIB(ill->ill_icmp6_mib, ipv6IfIcmpInGroupMembResponses);

		v6group_ptr = &mldh->mld_addr;
		if (!IN6_IS_ADDR_MULTICAST(v6group_ptr)) {
			BUMP_MIB(ill->ill_icmp6_mib,
			    ipv6IfIcmpInGroupMembBadReports);
			freemsg(mp);
			return;
		}


		/*
		 * If we belong to the group being reported, and we are a
		 * 'Delaying member' per the RFC terminology, stop our timer
		 * for that group and 'clear flag' i.e. mark ilm_state as
		 * IGMP_OTHERMEMBER. With zones, there can be multiple group
		 * membership entries for the same group address (one per zone)
		 * so we need to walk the ill_ilm list.
		 */
		mutex_enter(&ill->ill_lock);
		for (ilm = ill->ill_ilm; ilm != NULL; ilm = ilm->ilm_next) {
			if (!IN6_ARE_ADDR_EQUAL(&ilm->ilm_v6addr, v6group_ptr))
			    continue;
			BUMP_MIB(ill->ill_icmp6_mib,
			    ipv6IfIcmpInGroupMembOurReports);

			ilm->ilm_timer = INFINITY;
			ilm->ilm_state = IGMP_OTHERMEMBER;
		}
		mutex_exit(&ill->ill_lock);
		break;
	}
	case MLD_LISTENER_REDUCTION:
		BUMP_MIB(ill->ill_icmp6_mib, ipv6IfIcmpInGroupMembReductions);
		break;
	}
	/*
	 * All MLD packets have already been passed up to any
	 * process(es) listening on a ICMP6 raw socket. This
	 * has been accomplished in ip_deliver_local_v6 prior to
	 * this function call. It is assumed that the multicast daemon
	 * will have a SOCK_RAW IPPROTO_ICMPV6 (and presumbly use the
	 * ICMP6_FILTER socket option to only receive the MLD messages)
	 * Thus we can free the MLD message block here
	 */
	freemsg(mp);
}

/*
 * Handles an MLDv1 Listener Query.  Returns 0 on error, or the appropriate
 * (non-zero, unsigned) timer value to be set on success.
 */
static uint_t
mld_query_in(mld_hdr_t *mldh, ill_t *ill)
{
	ilm_t	*ilm;
	int	timer;
	uint_t	next;
	in6_addr_t *v6group;

	BUMP_MIB(ill->ill_icmp6_mib, ipv6IfIcmpInGroupMembQueries);

	/*
	 * In the MLD specification, there are 3 states and a flag.
	 *
	 * In Non-Listener state, we simply don't have a membership record.
	 * In Delaying state, our timer is running (ilm->ilm_timer < INFINITY)
	 * In Idle Member state, our timer is not running (ilm->ilm_timer ==
	 * INFINITY)
	 *
	 * The flag is ilm->ilm_state, it is set to IGMP_OTHERMEMBER if
	 * we have heard a report from another member, or IGMP_IREPORTEDLAST
	 * if I sent the last report.
	 */
	v6group = &mldh->mld_addr;
	if (!(IN6_IS_ADDR_UNSPECIFIED(v6group)) &&
	    ((!IN6_IS_ADDR_MULTICAST(v6group)))) {
		BUMP_MIB(ill->ill_icmp6_mib, ipv6IfIcmpInGroupMembBadQueries);
		return (0);
	}

	/* Need to do compatibility mode checking */
	mutex_enter(&ill->ill_lock);
	ill->ill_mcast_v1_time = 0;
	ill->ill_mcast_v1_tset = 1;
	if (ill->ill_mcast_type == MLD_V2_ROUTER) {
		ip1dbg(("Received MLDv1 Query on %s, switching mode to "
		    "MLD_V1_ROUTER\n", ill->ill_name));
		atomic_add_16(&ill->ill_ifptr->illif_mcast_v1, 1);
		ill->ill_mcast_type = MLD_V1_ROUTER;
	}
	mutex_exit(&ill->ill_lock);

	timer = (int)ntohs(mldh->mld_maxdelay);
	if (ip_debug > 1) {
		(void) mi_strlog(ill->ill_rq, 1, SL_TRACE,
		    "mld_input: TIMER = mld_maxdelay %d mld_type 0x%x",
		    timer, (int)mldh->mld_type);
	}

	/*
	 * -Start the timers in all of our membership records for
	 * the physical interface on which the query arrived,
	 * excl:
	 *	1.  those that belong to the "all hosts" group,
	 *	2.  those with 0 scope, or 1 node-local scope.
	 *
	 * -Restart any timer that is already running but has a value
	 * longer that the requested timeout.
	 * -Use the value specified in the query message as the
	 * maximum timeout.
	 */
	next = INFINITY;
	mutex_enter(&ill->ill_lock);
	for (ilm = ill->ill_ilm; ilm != NULL; ilm = ilm->ilm_next) {
		ASSERT(!IN6_IS_ADDR_V4MAPPED(&ilm->ilm_v6addr));

		if (IN6_IS_ADDR_UNSPECIFIED(&ilm->ilm_v6addr) ||
		    IN6_IS_ADDR_MC_NODELOCAL(&ilm->ilm_v6addr) ||
		    IN6_IS_ADDR_MC_RESERVED(&ilm->ilm_v6addr))
			continue;
		if ((!IN6_ARE_ADDR_EQUAL(&ilm->ilm_v6addr,
		    &ipv6_all_hosts_mcast)) &&
		    (IN6_IS_ADDR_UNSPECIFIED(v6group)) ||
		    (IN6_ARE_ADDR_EQUAL(v6group, &ilm->ilm_v6addr))) {
			if (timer == 0) {
				/* Respond immediately */
				ilm->ilm_timer = INFINITY;
				ilm->ilm_state = IGMP_IREPORTEDLAST;
				mutex_exit(&ill->ill_lock);
				mld_sendpkt(ilm, MLD_LISTENER_REPORT, NULL);
				mutex_enter(&ill->ill_lock);
				break;
			}
			if (ilm->ilm_timer > timer) {
				MCAST_RANDOM_DELAY(ilm->ilm_timer, timer);
				if (ilm->ilm_timer < next)
					next = ilm->ilm_timer;
			}
			break;
		}
	}
	mutex_exit(&ill->ill_lock);

	return (next);
}

/*
 * Handles an MLDv2 Listener Query.  On error, returns 0; on success,
 * returns the appropriate (non-zero, unsigned) timer value (which may
 * be INFINITY) to be set.
 */
static uint_t
mldv2_query_in(mld2q_t *mld2q, ill_t *ill, int mldlen)
{
	ilm_t	*ilm;
	in6_addr_t *v6group, *src_array;
	uint_t	next, numsrc, i, mrd, delay, qqi;
	uint8_t	qrv;

	v6group = &mld2q->mld2q_addr;
	numsrc = ntohs(mld2q->mld2q_numsrc);

	/* make sure numsrc matches packet size */
	if (mldlen < MLD_V2_QUERY_MINLEN + (numsrc * sizeof (in6_addr_t))) {
		BUMP_MIB(ill->ill_icmp6_mib, ipv6IfIcmpInErrors);
		return (0);
	}
	src_array = (in6_addr_t *)&mld2q[1];

	BUMP_MIB(ill->ill_icmp6_mib, ipv6IfIcmpInGroupMembQueries);

	/* extract Maximum Response Delay from code in header */
	mrd = ntohs(mld2q->mld2q_mxrc);
	if (mrd >= MLD_V2_MAXRT_FPMIN) {
		uint_t hdrval, mant, exp;
		hdrval = mrd;
		mant = hdrval & MLD_V2_MAXRT_MANT_MASK;
		exp = (hdrval & MLD_V2_MAXRT_EXP_MASK) >> 12;
		mrd = (mant | 0x1000) << (exp + 3);
	}
	MCAST_RANDOM_DELAY(delay, mrd);
	next = (unsigned)INFINITY;

	if ((qrv = mld2q->mld2q_sqrv & MLD_V2_RV_MASK) == 0)
		ill->ill_mcast_rv = MCAST_DEF_ROBUSTNESS;
	else
		ill->ill_mcast_rv = qrv;

	if ((qqi = (uint_t)mld2q->mld2q_qqic) >= MLD_V2_QQI_FPMIN) {
		uint_t mant, exp;
		mant = qqi & MLD_V2_QQI_MANT_MASK;
		exp = (qqi & MLD_V2_QQI_EXP_MASK) >> 12;
		qqi = (mant | 0x10) << (exp + 3);
	}
	ill->ill_mcast_qi = (qqi == 0) ? MCAST_DEF_QUERY_INTERVAL : qqi;

	/*
	 * If we have a pending general query response that's scheduled
	 * sooner than the delay we calculated for this response, then
	 * no action is required (MLDv2 draft section 6.2 rule 1)
	 */
	mutex_enter(&ill->ill_lock);
	if (ill->ill_global_timer < delay) {
		mutex_exit(&ill->ill_lock);
		return (next);
	}
	mutex_exit(&ill->ill_lock);

	/*
	 * Now take action depending on query type: general,
	 * group specific, or group/source specific.
	 */
	if ((numsrc == 0) && IN6_IS_ADDR_UNSPECIFIED(v6group)) {
		/*
		 * general query
		 * We know global timer is either not running or is
		 * greater than our calculated delay, so reset it to
		 * our delay (random value in range [0, response time])
		 */
		mutex_enter(&ill->ill_lock);
		ill->ill_global_timer = delay;
		next = ill->ill_global_timer;
		mutex_exit(&ill->ill_lock);

	} else {
		/* group or group/source specific query */
		mutex_enter(&ill->ill_lock);
		for (ilm = ill->ill_ilm; ilm != NULL; ilm = ilm->ilm_next) {
			if (IN6_IS_ADDR_UNSPECIFIED(&ilm->ilm_v6addr) ||
			    IN6_IS_ADDR_MC_NODELOCAL(&ilm->ilm_v6addr) ||
			    IN6_IS_ADDR_MC_RESERVED(&ilm->ilm_v6addr) ||
			    !IN6_ARE_ADDR_EQUAL(v6group, &ilm->ilm_v6addr))
				continue;

			/*
			 * If the query is group specific or we have a
			 * pending group specific query, the response is
			 * group specific (pending sources list should be
			 * empty).  Otherwise, need to update the pending
			 * sources list for the group and source specific
			 * response.
			 */
			if (numsrc == 0 || (ilm->ilm_timer < INFINITY &&
			    SLIST_IS_EMPTY(ilm->ilm_pendsrcs))) {
group_query:
				FREE_SLIST(ilm->ilm_pendsrcs);
				ilm->ilm_pendsrcs = NULL;
			} else {
				boolean_t overflow;
				slist_t *pktl;
				if (numsrc > MAX_FILTER_SIZE ||
				    (ilm->ilm_pendsrcs == NULL &&
				    (ilm->ilm_pendsrcs = l_alloc()) == NULL)) {
					/*
					 * We've been sent more sources than
					 * we can deal with; or we can't deal
					 * with a source list at all. Revert
					 * to a group specific query.
					 */
					goto group_query;
				}
				if ((pktl = l_alloc()) == NULL)
					goto group_query;
				pktl->sl_numsrc = numsrc;
				for (i = 0; i < numsrc; i++)
					pktl->sl_addr[i] = src_array[i];
				l_union_in_a(ilm->ilm_pendsrcs, pktl,
				    &overflow);
				l_free(pktl);
				if (overflow)
					goto group_query;
			}
			/* set timer to soonest value */
			ilm->ilm_timer = MIN(ilm->ilm_timer, delay);
			if (ilm->ilm_timer < next)
				next = ilm->ilm_timer;
			break;
		}
		mutex_exit(&ill->ill_lock);
	}

	return (next);
}

/*
 * Send MLDv1 response packet with hoplimit 1
 */
static void
mld_sendpkt(ilm_t *ilm, uchar_t type, const in6_addr_t *v6addr)
{
	mblk_t		*mp;
	mld_hdr_t	*mldh;
	ip6_t 		*ip6h;
	ip6_hbh_t	*ip6hbh;
	struct ip6_opt_router	*ip6router;
	size_t		size = IPV6_HDR_LEN + sizeof (mld_hdr_t);
	ill_t		*ill = ilm->ilm_ill;   /* Will be the "lower" ill */
	ipif_t		*ipif;
	ip6i_t		*ip6i;

	/*
	 * We need to place a router alert option in this packet.  The length
	 * of the options must be a multiple of 8.  The hbh option header is 2
	 * bytes followed by the 4 byte router alert option.  That leaves
	 * 2 bytes of pad for a total of 8 bytes.
	 */
	const int	router_alert_length = 8;

	ASSERT(ill->ill_isv6);

	/*
	 * We need to make sure that this packet does not get load balanced.
	 * So, we allocate an ip6i_t and set ATTACH_IF. ip_wput_v6 and
	 * ip_newroute_ipif_v6 knows how to handle such packets.
	 * If it gets load balanced, switches supporting MLD snooping
	 * (in the future) will send the packet that it receives for this
	 * multicast group to the interface that we are sending on. As we have
	 * joined the multicast group on this ill, by sending the packet out
	 * on this ill, we receive all the packets back on this ill.
	 */
	size += sizeof (ip6i_t) + router_alert_length;
	mp = allocb(size, BPRI_HI);
	if (mp == NULL)
		return;
	bzero(mp->b_rptr, size);
	mp->b_wptr = mp->b_rptr + size;

	ip6i = (ip6i_t *)mp->b_rptr;
	ip6i->ip6i_vcf = IPV6_DEFAULT_VERS_AND_FLOW;
	ip6i->ip6i_nxt = IPPROTO_RAW;
	ip6i->ip6i_flags = IP6I_ATTACH_IF | IP6I_HOPLIMIT;
	ip6i->ip6i_ifindex = ill->ill_phyint->phyint_ifindex;

	ip6h = (ip6_t *)&ip6i[1];
	ip6hbh = (struct ip6_hbh *)&ip6h[1];
	ip6router = (struct ip6_opt_router *)&ip6hbh[1];
	/*
	 * A zero is a pad option of length 1.  The bzero of the whole packet
	 * above will pad between ip6router and mld.
	 */
	mldh = (mld_hdr_t *)((uint8_t *)ip6hbh + router_alert_length);

	mldh->mld_type = type;
	mldh->mld_addr = ilm->ilm_v6addr;

	ip6router->ip6or_type = IP6OPT_ROUTER_ALERT;
	ip6router->ip6or_len = 2;
	ip6router->ip6or_value[0] = 0;
	ip6router->ip6or_value[1] = IP6_ALERT_MLD;

	ip6hbh->ip6h_nxt = IPPROTO_ICMPV6;
	ip6hbh->ip6h_len = 0;

	ip6h->ip6_vcf = IPV6_DEFAULT_VERS_AND_FLOW;
	ip6h->ip6_plen = htons(sizeof (*mldh) + router_alert_length);
	ip6h->ip6_nxt = IPPROTO_HOPOPTS;
	ip6h->ip6_hops = MLD_HOP_LIMIT;
	if (v6addr == NULL)
		ip6h->ip6_dst =  ilm->ilm_v6addr;
	else
		ip6h->ip6_dst = *v6addr;

	/* ipif returned by ipif_lookup_zoneid is link-local (if present) */
	if (ipif_lookup_zoneid(ill, ilm->ilm_zoneid, IPIF_UP, &ipif)) {
		ip6h->ip6_src = ipif->ipif_v6src_addr;
		ipif_refrele(ipif);
	} else {
		/* Otherwise, use IPv6 default address selection. */
		ip6h->ip6_src = ipv6_all_zeros;
	}

	/*
	 * Prepare for checksum by putting icmp length in the icmp
	 * checksum field. The checksum is calculated in ip_wput_v6.
	 */
	mldh->mld_cksum = htons(sizeof (*mldh));

	/*
	 * ip_wput will automatically loopback the multicast packet to
	 * the conn if multicast loopback is enabled.
	 * The MIB stats corresponding to this outgoing MLD packet
	 * will be accounted for in ip_wput->ip_wput_v6->ip_wput_ire_v6
	 * ->icmp_update_out_mib_v6 function call.
	 */
	(void) ip_output_v6(NULL, mp, ill->ill_wq, IP_WPUT);
}

/*
 * Sends an MLD_V2_LISTENER_REPORT message out the passed-in ill.  The
 * report will contain one multicast address record for each element of
 * reclist.  If this causes packet length to exceed ill->ill_max_frag,
 * multiple reports are sent.  reclist is assumed to be made up of
 * buffers allocated by mcast_bldmrec(), and those buffers are freed here.
 */
static void
mldv2_sendrpt(ill_t *ill, mrec_t *reclist)
{
	mblk_t		*mp;
	mld2r_t		*mld2r;
	mld2mar_t	*mld2mar;
	in6_addr_t	*srcarray;
	ip6_t		*ip6h;
	ip6_hbh_t	*ip6hbh;
	ip6i_t		*ip6i;
	struct ip6_opt_router	*ip6router;
	size_t		size, optlen, padlen, icmpsize, rsize;
	ipif_t		*ipif;
	int		i, numrec, more_src_cnt;
	mrec_t		*rp, *cur_reclist;
	mrec_t		*next_reclist = reclist;
	boolean_t	morepkts;

	/* If there aren't any records, there's nothing to send */
	if (reclist == NULL)
		return;

	ASSERT(ill->ill_isv6);

	/*
	 * Total option length (optlen + padlen) must be a multiple of
	 * 8 bytes.  We assume here that optlen <= 8, so the total option
	 * length will be 8.  Assert this in case anything ever changes.
	 */
	optlen = sizeof (ip6_hbh_t) + sizeof (struct ip6_opt_router);
	ASSERT(optlen <= 8);
	padlen = 8 - optlen;
nextpkt:
	icmpsize = sizeof (mld2r_t);
	size = IPV6_HDR_LEN + optlen + padlen + icmpsize;
	morepkts = B_FALSE;
	more_src_cnt = 0;
	for (rp = cur_reclist = next_reclist, numrec = 0; rp != NULL;
	    rp = rp->mrec_next, numrec++) {
		rsize = sizeof (mld2mar_t) +
		    (rp->mrec_srcs.sl_numsrc * sizeof (in6_addr_t));
		if (size + rsize > ill->ill_max_frag) {
			if (rp == cur_reclist) {
				/*
				 * If the first mrec we looked at is too big
				 * to fit in a single packet (i.e the source
				 * list is too big), we must either truncate
				 * the list (if TO_EX or IS_EX), or send
				 * multiple reports for the same group (all
				 * other types).
				 */
				int srcspace, srcsperpkt;
				srcspace = ill->ill_max_frag -
				    (size + sizeof (mld2mar_t));
				srcsperpkt = srcspace / sizeof (in6_addr_t);
				/*
				 * Increment icmpsize and size, because we will
				 * be sending a record for the mrec we're
				 * looking at now.
				 */
				rsize = sizeof (mld2mar_t) +
				    (srcsperpkt * sizeof (in6_addr_t));
				icmpsize += rsize;
				size += rsize;
				if (rp->mrec_type == MODE_IS_EXCLUDE ||
				    rp->mrec_type == CHANGE_TO_EXCLUDE) {
					rp->mrec_srcs.sl_numsrc = srcsperpkt;
					if (rp->mrec_next == NULL) {
						/* no more packets to send */
						break;
					} else {
						/*
						 * more packets, but we're
						 * done with this mrec.
						 */
						next_reclist = rp->mrec_next;
					}
				} else {
					more_src_cnt = rp->mrec_srcs.sl_numsrc
					    - srcsperpkt;
					rp->mrec_srcs.sl_numsrc = srcsperpkt;
					/*
					 * We'll fix up this mrec (remove the
					 * srcs we've already sent) before
					 * returning to nextpkt above.
					 */
					next_reclist = rp;
				}
			} else {
				next_reclist = rp;
			}
			morepkts = B_TRUE;
			break;
		}
		icmpsize += rsize;
		size += rsize;
	}

	/*
	 * We need to make sure that this packet does not get load balanced.
	 * So, we allocate an ip6i_t and set ATTACH_IF. ip_wput_v6 and
	 * ip_newroute_ipif_v6 know how to handle such packets.
	 * If it gets load balanced, switches supporting MLD snooping
	 * (in the future) will send the packet that it receives for this
	 * multicast group to the interface that we are sending on. As we have
	 * joined the multicast group on this ill, by sending the packet out
	 * on this ill, we receive all the packets back on this ill.
	 */
	size += sizeof (ip6i_t);
	mp = allocb(size, BPRI_HI);
	if (mp == NULL)
		goto free_reclist;
	bzero(mp->b_rptr, size);
	mp->b_wptr = mp->b_rptr + size;

	ip6i = (ip6i_t *)mp->b_rptr;
	ip6i->ip6i_vcf = IPV6_DEFAULT_VERS_AND_FLOW;
	ip6i->ip6i_nxt = IPPROTO_RAW;
	ip6i->ip6i_flags = IP6I_ATTACH_IF;
	ip6i->ip6i_ifindex = ill->ill_phyint->phyint_ifindex;

	ip6h = (ip6_t *)&(ip6i[1]);
	ip6hbh = (ip6_hbh_t *)&(ip6h[1]);
	ip6router = (struct ip6_opt_router *)&(ip6hbh[1]);
	mld2r = (mld2r_t *)((uint8_t *)ip6hbh + optlen + padlen);
	mld2mar = (mld2mar_t *)&(mld2r[1]);

	ip6h->ip6_vcf = IPV6_DEFAULT_VERS_AND_FLOW;
	ip6h->ip6_plen = htons(optlen + padlen + icmpsize);
	ip6h->ip6_nxt = IPPROTO_HOPOPTS;
	ip6h->ip6_hops = MLD_HOP_LIMIT;
	ip6h->ip6_dst = ipv6_all_v2rtrs_mcast;
	/* ipif returned by ipif_lookup_zoneid is link-local (if present) */
	if (ipif_lookup_zoneid(ill, ALL_ZONES, IPIF_UP, &ipif)) {
		ip6h->ip6_src = ipif->ipif_v6src_addr;
		ipif_refrele(ipif);
	} else {
		/* otherwise, use IPv6 default address selection. */
		ip6h->ip6_src = ipv6_all_zeros;
	}

	ip6hbh->ip6h_nxt = IPPROTO_ICMPV6;
	/*
	 * ip6h_len is the number of 8-byte words, not including the first
	 * 8 bytes; we've assumed optlen + padlen == 8 bytes; hence len = 0.
	 */
	ip6hbh->ip6h_len = 0;

	ip6router->ip6or_type = IP6OPT_ROUTER_ALERT;
	ip6router->ip6or_len = 2;
	ip6router->ip6or_value[0] = 0;
	ip6router->ip6or_value[1] = IP6_ALERT_MLD;

	mld2r->mld2r_type = MLD_V2_LISTENER_REPORT;
	mld2r->mld2r_nummar = htons(numrec);
	/*
	 * Prepare for the checksum by putting icmp length in the icmp
	 * checksum field. The checksum is calculated in ip_wput_v6.
	 */
	mld2r->mld2r_cksum = htons(icmpsize);

	for (rp = cur_reclist; rp != NULL; rp = rp->mrec_next) {
		mld2mar->mld2mar_type = rp->mrec_type;
		mld2mar->mld2mar_auxlen = 0;
		mld2mar->mld2mar_numsrc = htons(rp->mrec_srcs.sl_numsrc);
		mld2mar->mld2mar_group = rp->mrec_group;
		srcarray = (in6_addr_t *)&(mld2mar[1]);

		for (i = 0; i < rp->mrec_srcs.sl_numsrc; i++)
			srcarray[i] = rp->mrec_srcs.sl_addr[i];

		mld2mar = (mld2mar_t *)&(srcarray[i]);
	}

	/*
	 * ip_wput will automatically loopback the multicast packet to
	 * the conn if multicast loopback is enabled.
	 * The MIB stats corresponding to this outgoing MLD packet
	 * will be accounted for in ip_wput->ip_wput_v6->ip_wput_ire_v6
	 * ->icmp_update_out_mib_v6 function call.
	 */
	(void) ip_output_v6(NULL, mp, ill->ill_wq, IP_WPUT);

	if (morepkts) {
		if (more_src_cnt > 0) {
			int index, mvsize;
			slist_t *sl = &next_reclist->mrec_srcs;
			index = sl->sl_numsrc;
			mvsize = more_src_cnt * sizeof (in6_addr_t);
			(void) memmove(&sl->sl_addr[0], &sl->sl_addr[index],
			    mvsize);
			sl->sl_numsrc = more_src_cnt;
		}
		goto nextpkt;
	}

free_reclist:
	while (reclist != NULL) {
		rp = reclist->mrec_next;
		mi_free(reclist);
		reclist = rp;
	}
}

static mrec_t *
mcast_bldmrec(mcast_record_t type, in6_addr_t *grp, slist_t *srclist,
    mrec_t *next)
{
	mrec_t *rp;
	int i;

	if ((type == ALLOW_NEW_SOURCES || type == BLOCK_OLD_SOURCES) &&
	    SLIST_IS_EMPTY(srclist))
		return (next);

	rp = (mrec_t *)mi_alloc(sizeof (mrec_t), BPRI_HI);
	if (rp == NULL)
		return (next);

	rp->mrec_next = next;
	rp->mrec_type = type;
	rp->mrec_auxlen = 0;
	rp->mrec_group = *grp;
	if (srclist == NULL) {
		rp->mrec_srcs.sl_numsrc = 0;
	} else {
		rp->mrec_srcs.sl_numsrc = srclist->sl_numsrc;
		for (i = 0; i < srclist->sl_numsrc; i++)
			rp->mrec_srcs.sl_addr[i] = srclist->sl_addr[i];
	}

	return (rp);
}

/*
 * Set up initial retransmit state.  If memory cannot be allocated for
 * the source lists, simply create as much state as is possible; memory
 * allocation failures are considered one type of transient error that
 * the retransmissions are designed to overcome (and if they aren't
 * transient, there are bigger problems than failing to notify the
 * router about multicast group membership state changes).
 */
static void
mcast_init_rtx(ill_t *ill, rtx_state_t *rtxp, mcast_record_t rtype,
    slist_t *flist)
{
	/*
	 * There are only three possibilities for rtype:
	 *	New join, transition from INCLUDE {} to INCLUDE {flist}
	 *	  => rtype is ALLOW_NEW_SOURCES
	 *	New join, transition from INCLUDE {} to EXCLUDE {flist}
	 *	  => rtype is CHANGE_TO_EXCLUDE
	 *	State change that involves a filter mode change
	 *	  => rtype is either CHANGE_TO_INCLUDE or CHANGE_TO_EXCLUDE
	 */
	ASSERT(rtype == CHANGE_TO_EXCLUDE || rtype == CHANGE_TO_INCLUDE ||
	    rtype == ALLOW_NEW_SOURCES);

	rtxp->rtx_cnt = ill->ill_mcast_rv;

	switch (rtype) {
	case CHANGE_TO_EXCLUDE:
		rtxp->rtx_fmode_cnt = ill->ill_mcast_rv;
		CLEAR_SLIST(rtxp->rtx_allow);
		COPY_SLIST(flist, rtxp->rtx_block);
		break;
	case ALLOW_NEW_SOURCES:
	case CHANGE_TO_INCLUDE:
		rtxp->rtx_fmode_cnt =
		    rtype == ALLOW_NEW_SOURCES ? 0 : ill->ill_mcast_rv;
		CLEAR_SLIST(rtxp->rtx_block);
		COPY_SLIST(flist, rtxp->rtx_allow);
		break;
	}
}

/*
 * The basic strategy here, as extrapolated from RFC 3810 section 6.1 and
 * RFC 3376 section 5.1, covers three cases:
 *	* The current state change is a filter mode change
 *		Set filter mode retransmit counter; set retransmit allow or
 *		block list to new source list as appropriate, and clear the
 *		retransmit list that was not set; send TO_IN or TO_EX with
 *		new source list.
 *	* The current state change is a source list change, but the filter
 *	  mode retransmit counter is > 0
 *		Decrement filter mode retransmit counter; set retransmit
 *		allow or block list to  new source list as appropriate,
 *		and clear the retransmit list that was not set; send TO_IN
 *		or TO_EX with new source list.
 *	* The current state change is a source list change, and the filter
 *	  mode retransmit counter is 0.
 *		Merge existing rtx allow and block lists with new state:
 *		  rtx_allow = (new allow + rtx_allow) - new block
 *		  rtx_block = (new block + rtx_block) - new allow
 *		Send ALLOW and BLOCK records for new retransmit lists;
 *		decrement retransmit counter.
 *
 * As is the case for mcast_init_rtx(), memory allocation failures are
 * acceptable; we just create as much state as we can.
 */
static mrec_t *
mcast_merge_rtx(ilm_t *ilm, mrec_t *mreclist, slist_t *flist)
{
	ill_t *ill;
	rtx_state_t *rtxp = &ilm->ilm_rtx;
	mcast_record_t txtype;
	mrec_t *rp, *rpnext, *rtnmrec;
	boolean_t ovf;

	ill = (ilm->ilm_ill == NULL ? ilm->ilm_ipif->ipif_ill : ilm->ilm_ill);

	if (mreclist == NULL)
		return (mreclist);

	/*
	 * A filter mode change is indicated by a single mrec, which is
	 * either TO_IN or TO_EX.  In this case, we just need to set new
	 * retransmit state as if this were an initial join.  There is
	 * no change to the mrec list.
	 */
	if (mreclist->mrec_type == CHANGE_TO_INCLUDE ||
	    mreclist->mrec_type == CHANGE_TO_EXCLUDE) {
		mcast_init_rtx(ill, rtxp, mreclist->mrec_type,
		    &mreclist->mrec_srcs);
		return (mreclist);
	}

	/*
	 * Only the source list has changed
	 */
	rtxp->rtx_cnt = ill->ill_mcast_rv;
	if (rtxp->rtx_fmode_cnt > 0) {
		/* but we're still sending filter mode change reports */
		rtxp->rtx_fmode_cnt--;
		if (ilm->ilm_fmode == MODE_IS_INCLUDE) {
			CLEAR_SLIST(rtxp->rtx_block);
			COPY_SLIST(flist, rtxp->rtx_allow);
			txtype = CHANGE_TO_INCLUDE;
		} else {
			CLEAR_SLIST(rtxp->rtx_allow);
			COPY_SLIST(flist, rtxp->rtx_block);
			txtype = CHANGE_TO_EXCLUDE;
		}
		/* overwrite first mrec with new info */
		mreclist->mrec_type = txtype;
		l_copy(flist, &mreclist->mrec_srcs);
		/* then free any remaining mrecs */
		for (rp = mreclist->mrec_next; rp != NULL; rp = rpnext) {
			rpnext = rp->mrec_next;
			mi_free(rp);
		}
		mreclist->mrec_next = NULL;
		rtnmrec = mreclist;
	} else {
		mrec_t *allow_mrec, *block_mrec;
		/*
		 * Just send the source change reports; but we need to
		 * recalculate the ALLOW and BLOCK lists based on previous
		 * state and new changes.
		 */
		rtnmrec = mreclist;
		allow_mrec = block_mrec = NULL;
		for (rp = mreclist; rp != NULL; rp = rp->mrec_next) {
			ASSERT(rp->mrec_type == ALLOW_NEW_SOURCES ||
			    rp->mrec_type == BLOCK_OLD_SOURCES);
			if (rp->mrec_type == ALLOW_NEW_SOURCES)
				allow_mrec = rp;
			else
				block_mrec = rp;
		}
		/*
		 * Perform calculations:
		 *   new_allow = mrec_allow + (rtx_allow - mrec_block)
		 *   new_block = mrec_block + (rtx_block - mrec_allow)
		 *
		 * Each calc requires two steps, for example:
		 *   rtx_allow = rtx_allow - mrec_block;
		 *   new_allow = mrec_allow + rtx_allow;
		 *
		 * Store results in mrec lists, and then copy into rtx lists.
		 * We do it in this order in case the rtx list hasn't been
		 * alloc'd yet; if it hasn't and our alloc fails, that's okay,
		 * Overflows are also okay.
		 */
		if (block_mrec != NULL) {
			l_difference_in_a(rtxp->rtx_allow,
			    &block_mrec->mrec_srcs);
		}
		if (allow_mrec != NULL) {
			l_difference_in_a(rtxp->rtx_block,
			    &allow_mrec->mrec_srcs);
			l_union_in_a(&allow_mrec->mrec_srcs, rtxp->rtx_allow,
			    &ovf);
		}
		if (block_mrec != NULL) {
			l_union_in_a(&block_mrec->mrec_srcs, rtxp->rtx_block,
			    &ovf);
			COPY_SLIST(&block_mrec->mrec_srcs, rtxp->rtx_block);
		} else {
			rtnmrec = mcast_bldmrec(BLOCK_OLD_SOURCES,
			    &ilm->ilm_v6addr, rtxp->rtx_block, allow_mrec);
		}
		if (allow_mrec != NULL) {
			COPY_SLIST(&allow_mrec->mrec_srcs, rtxp->rtx_allow);
		} else {
			rtnmrec = mcast_bldmrec(ALLOW_NEW_SOURCES,
			    &ilm->ilm_v6addr, rtxp->rtx_allow, block_mrec);
		}
	}

	return (rtnmrec);
}
