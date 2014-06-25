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

#ifndef _ASM_FLUSH_H
#define	_ASM_FLUSH_H

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#if !defined(__lint) && defined(__GNUC__)

/*
 * doflush() need to be inlined by krtld.
 * It makes the instruction at addr valid for instruction execution,
 * and is called before any function can be called, i.e.
 * before _kobj_boot() can correctly call kobj_init().
 */
extern __inline__ void
doflush(void * addr)
{
	__asm__ __volatile__(
			"addi	%%r3, %0, 0\n\t"
			"subf	%%r0, %%r0, %%r0\n\t"
			"dcbst	%%r0,%%r3\n\t"	/* force to memory */
			"sync\n\t"		/* guarantee dcbst is done before icbi */
			"icbi	%%r0,%%r3\n\t"	/* force out of instr cache */
			"sync\n\t"		/* sync for the icbi */
			"isync"
			: "=r" (addr)
			: "0" (addr)
			: "%r0", "%r3");
}

#endif	/* !__lint && __GNUC__ */

#ifdef	__cplusplus
}
#endif

#endif	/* _ASM_FLUSH_H */
