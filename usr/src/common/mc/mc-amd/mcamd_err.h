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
 *
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef _MCAMD_ERR_H
#define	_MCAMD_ERR_H

#pragma ident	"@(#)mcamd_err.h	1.1	06/02/11 SMI"

#ifdef __cplusplus
extern "C" {
#endif

#define	EMCAMD_BASE	2000	/* out of system's and consumer's way */

enum {
	EMCAMD_SYNDINVALID = EMCAMD_BASE,	/* invalid syndrome */
	EMCAMD_TREEINVALID,			/* invalid configuration tree */
	EMCAMD_NOADDR,				/* address not found */
	EMCAMD_NOTSUP				/* operation not supported */
};

extern const char *mcamd_errmsg(struct mcamd_hdl *);
extern const char *mcamd_strerror(int);
extern int mcamd_errno(struct mcamd_hdl *);
extern int mcamd_set_errno(struct mcamd_hdl *, int);
extern void *mcamd_set_errno_ptr(struct mcamd_hdl *, int);

#ifdef __cplusplus
}
#endif

#endif /* _MCAMD_ERR_H */
