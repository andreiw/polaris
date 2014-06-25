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

#ifndef _ASM_THREAD_H
#define	_ASM_THREAD_H

#pragma ident	"%Z%%M%	%I%	%E% SMI"

/*
 * Definition of threadp()
 */

#ifdef __cplusplus
extern "C" {
#endif

struct _kthread;

// XXX Should unify all definitions of THREAD_REG
// XXX and make all uses of THREAD_REG revolve around
// XXX a single naked register number as the basis
// XXX for all other derived symbols, such as "r2" or "%r2"

extern __inline__ struct _kthread *
threadp(void)
{

	register struct _kthread *thread_reg __asm__("r2");

	return (thread_reg);
}

#ifdef __cplusplus
}
#endif

#endif /* _ASM_THREAD_H */
