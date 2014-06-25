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

#ifndef	_SYS_REGSET_FP_H
#define	_SYS_REGSET_FP_H

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#if !defined(_ASM)

#if !defined(_XPG4_2) || defined(__EXTENSIONS__)

/*
 * Floating point definitions.
 */
typedef struct fpu {
	double		fpu_regs[32];	/* FPU regs - 32 doubles */
	unsigned	fpu_fpscr;	/* FPU status/control reg */
	unsigned 	fpu_valid;	/* nonzero IFF the rest of this */
					/* structure contains valid data */
} fpregset_t;

/*
 * Structure mcontext defines the complete hardware machine state.
 */
typedef struct {
	gregset_t	gregs;		/* general register set */
	fpregset_t	fpregs;		/* floating point register set */
	long		filler[8];
} mcontext_t;

#else /* XPG4v2 requires mcontext_t  -- XPG4v2 allowed identifiers */

/*
 * Floating point definitions.
 */
typedef struct __fpu {
	double		__fpu_regs[32];	/* FPU regs - 32 doubles */
	unsigned	__fpu_fpscr;	/* FPU status/control reg */
	unsigned 	__fpu_valid;	/* nonzero IFF the rest of this */
					/* structure contains valid data */
} fpregset_t;

/*
 * Structure mcontext defines the complete hardware machine state.
 */
typedef struct {
	gregset_t	__gregs;	/* general register set */
	fpregset_t	__fpregs;	/* floating point register set */
	long		__filler[8];
} mcontext_t;

#endif /* !defined(_XPG4_2) || defined(__EXTENSIONS__) */

#endif	/* !defined(_ASM) */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_REGSET_FP_H */
