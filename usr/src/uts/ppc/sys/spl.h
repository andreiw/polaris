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
 * Copyright 2006 Sun Microsystems Laboratories.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef _SYS_SPL_H
#define	_SYS_SPL_H

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Convert system interrupt priorities (0-7) into a psr for splx.
 * In general, the processor priority (0-15) should be 2 times
 * the system priority.
 */

/*
 * on PPC hardware spl and priority are the same
 */
#define	pritospl(n)	(n)


/*
 * on PPC hardware these are all identity functions
 */
#define	ipltospl(n)	(n)
#define	spltoipl(n)	(n)
#define	spltopri(n)	(n)

/*
 * Hardware spl levels
 * it should be replace by the appropriate interrupt class info.
 */
#define	SPL8	15
#define	SPL7	13

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SPL_H */
