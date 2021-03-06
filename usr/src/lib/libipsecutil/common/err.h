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

#ifndef	_ERR_H
#define	_ERR_H

#pragma ident	"@(#)err.h	1.2	05/06/08 SMI"

/*
 * Headers and definitions for support functions that are shared by
 * the ipsec utilities ipseckey and ikeadm.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>

/*
 * Function Prototypes
 */

/* Program exit and warning calls.   Thank you NetBSD. */
void err(int, const char *, ...);
void verr(int, const char *, va_list);
void errx(int, const char *, ...);
void verrx(int, const char *, va_list);
void warn(const char *, ...);
void vwarn(const char *, va_list);
void warnx(const char *, ...);
void vwarnx(const char *, va_list);

#ifdef __cplusplus
}
#endif

#endif	/* _ERR_H */
