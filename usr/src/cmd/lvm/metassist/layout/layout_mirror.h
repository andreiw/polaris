/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
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
 * Copyright 2003 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef _LAYOUT_MIRROR_H
#define	_LAYOUT_MIRROR_H

#pragma ident	"@(#)layout_mirror.h	1.2	05/06/08 SMI"

#ifdef __cplusplus
extern "C" {
#endif

#include "volume_devconfig.h"
#include "volume_dlist.h"

extern int layout_mirror(
	devconfig_t	*request,
	uint16_t 	nsubs,
	uint64_t 	nbytes,
	dlist_t		**results);

extern int populate_explicit_mirror(
	devconfig_t	*req,
	dlist_t		**results);

#ifdef __cplusplus
}
#endif

#endif /* _LAYOUT_MIRROR_H */
