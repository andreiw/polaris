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
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/


#ifndef _USTAT_H
#define	_USTAT_H

#pragma ident	"@(#)ustat.h	1.8	05/06/08 SMI"	/* SVr4.0 1.3.1.6 */

#include <sys/types.h>
#include <sys/ustat.h>

#ifdef	__cplusplus
extern "C" {
#endif

#if defined(__STDC__)
extern int ustat(dev_t, struct ustat *);
#else
extern int ustat();
#endif	/* end defined(_STDC) */

#ifdef	__cplusplus
}
#endif

#endif	/* _USTAT_H */
