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

#ifndef _SFX4500_DISK_H
#define	_SFX4500_DISK_H

#pragma ident	"@(#)sfx4500-disk.h	1.1	06/05/19 SMI"

/*
 * Externs for sfx4500-disk
 */

#include <diskmon_conf.h>

#ifdef __cplusplus
extern "C" {
#endif

#define	SFX4500_DISK_MODULE_VERSION "1.0"

#define	E_SUCCESS	0
#define	E_ERROR		1

extern cfgdata_t	*config_data;
extern fmd_xprt_t	*g_xprt_hdl;
extern fmd_hdl_t	*g_fm_hdl;


#ifdef __cplusplus
}
#endif

#endif /* _SFX4500_DISK_H */
