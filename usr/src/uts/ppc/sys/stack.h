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
 * Copyright 2006 Cyril Plisko.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef _SYS_STACK_H
#define	_SYS_STACK_H

#pragma ident	"%Z%%M%	%I%	%E% CP"

/*
 * PowerPC Stack Frame 
 */

#if !defined(_ASM)
#include <sys/types.h>
#include <sys/ppc_subr.h>
#endif	/* _ASM */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * PowerPC 32 bit stack frame structure
 *
 *      +---- ----------------+
 *      | Back Chain          |
 *      +---------------------+
 *      | Floating point      |
 *      | register save area  |
 *      +---------------------+
 *      | General register    |
 *      | save area           |
 *      +---------------------+
 *      | CR save area        |
 *      +---------------------+
 *      | Local variable space|
 *      +---------------------+
 *      | Parameter list area |
 *      +---------------------+
 *      | LR save word        |
 *      +---------------------+
 * r1-> | Back chain          |
 *      +---------------------+
 *
 *
 */
#define	WINDOWSIZE32	(64) /* FIX ME */
#define ARGPUSHSIZE32	(32) /* FIX ME */
#define ARGPUSH32	(WINDOWSIZE32 + 8) /* FIX ME */
#define MINFRAME32	(8)

/*
 * Stack alignment macros.
 */
#define STACK_ALIGN32	16
#define	STACK_BIAS32	0
#define SA32(X)	(((X)+(STACK_ALIGN32-1)) & ~(STACK_ALIGN32-1))


#define MINFRAME	MINFRAME32
#define STACK_ALIGN	STACK_ALIGN32
#define STACK_BIAS	STACK_BIAS32
#define SA(X)		SA32(X)

#if defined(_KERNEL) && !defined(_ASM)

#define	ASSERT_STACK_ALIGNED() \
	ASSERT(((uintptr_t)getsp() & (STACK_ALIGN - 1)) == 0)

void traceback(caddr_t);
void tracedump(void);

#endif  /* defined(_KERNEL) && !defined(_ASM) */

#define STACK_GROWTH_DOWN /* stacks grow from high to low addresses */

#ifdef __cplusplus
}
#endif

#endif /* _SYS_STACK_H */
