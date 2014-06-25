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
 * Copyright 2002 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef	_CTRL_DESCRIPTORS_H
#define	_CTRL_DESCRIPTORS_H

#pragma ident	"@(#)ctrl_descriptors.h	1.2	05/06/08 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <cimapi.h>
#include <cimprovider.h>
#include "libdiskmgt.h"

CCIMInstance		*ctrl_descriptor_toCCIMInstance(char *hostName,
			    dm_descriptor_t dp, char *providerName, int *errp);

CCIMInstanceList	*ctrl_descriptors_toCCIMInstanceList(
			    char *providerName, dm_descriptor_t *dp, int *errp,
				int, ...);


#ifdef __cplusplus
}
#endif

#endif /* _CTRL_DESCRIPTORS_H */
