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

#ifndef	_SYS_DLD_H
#define	_SYS_DLD_H

#pragma ident	"@(#)dld.h	1.5	06/06/29 SMI"

/*
 * Data-Link Driver (public header).
 */

#include <sys/types.h>
#include <sys/stream.h>
#include <sys/mac.h>
#include <sys/dls.h>
#include <net/if.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Data-Link Driver Information (text emitted by modinfo(1m))
 */
#define	DLD_INFO	"Data-Link Driver v1.5"

#define	DLD_MAX_PPA	999
#define	DLD_MAX_MINOR	(DLD_MAX_PPA + 1)

/*
 * Options: To enable an option set the property name to a non-zero value
 *	    in kernel/drv/dld.conf.
 */

/*
 * Prevent use of the IP fast-path (direct M_DATA transmit).
 */
#define	DLD_PROP_NO_FASTPATH	"no-fastpath"

/*
 * Prevent advertising of the DL_CAPAB_POLL capability.
 */
#define	DLD_PROP_NO_POLL	"no-poll"

/*
 * Prevent advertising of the DL_CAPAB_ZEROCOPY capability.
 */
#define	DLD_PROP_NO_ZEROCOPY	"no-zerocopy"

/*
 * The name of the driver.
 */
#define	DLD_DRIVER_NAME		"dld"

/*
 * The name of the control minor node of dld.
 */
#define	DLD_CONTROL_MINOR_NAME	"ctl"
#define	DLD_CONTROL_MINOR	0
#define	DLD_CONTROL_DEV		"/devices/pseudo/" DLD_DRIVER_NAME "@0:" \
				DLD_CONTROL_MINOR_NAME

/*
 * IOCTL codes and data structures.
 */
#define	DLDIOC		('D' << 24 | 'L' << 16 | 'D' << 8)

#define	DLDIOCATTR	(DLDIOC | 0x03)

typedef struct dld_ioc_attr {
	char		dia_name[IFNAMSIZ];
	char		dia_dev[MAXNAMELEN];
	uint_t		dia_max_sdu;
	uint16_t	dia_vid;
} dld_ioc_attr_t;

#define	DLDIOCVLAN	(DLDIOC | 0x04)

typedef struct dld_ioc_vlan {
	char		div_name[IFNAMSIZ];
	uint_t		div_count;
} dld_ioc_vlan_t;

typedef struct dld_vlan_info {
	char		dvi_name[IFNAMSIZ];
} dld_vlan_info_t;

#ifdef _KERNEL
int	dld_getinfo(dev_info_t *, ddi_info_cmd_t, void *, void **);
int	dld_open(queue_t *, dev_t *, int, int, cred_t *);
int	dld_close(queue_t *);
void	dld_wput(queue_t *, mblk_t *);
void	dld_wsrv(queue_t *);
void	dld_init_ops(struct dev_ops *, const char *);
void	dld_fini_ops(struct dev_ops *);
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_DLD_H */
