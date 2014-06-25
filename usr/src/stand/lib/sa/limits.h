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

#ifndef _SA_LIMITS_H
#define	_SA_LIMITS_H

#pragma ident	"@(#)limits.h	1.2	05/06/08 SMI"

/*
 * Exported interfaces for standalone's subset of libc's <limits.h>.
 * All standalone code *must* use this header rather than libc's.
 *
 * Since all the sources that #include this file do it spuriously, we provide
 * no definitions.  Note that we do this rather than removing the errant
 * #includes because the code they live in (openssl) is third-party and we're
 * still receiving code drops for it.
 */

 /* For building PPC libc fp port objects required for boot, see Makefile for those objects */
#define	CHAR_BIT		 0x8

#endif /* _SA_LIMITS_H */
