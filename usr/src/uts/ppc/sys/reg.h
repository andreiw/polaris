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


/* 2.6_leveraged #pragma ident	"@(#)reg.h	1.16	96/10/02 SMI" */

#ifndef	_SYS_REG_H
#define	_SYS_REG_H

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/reg_gp.h>

#if !defined(_XPG4_2) || defined(__EXTENSIONS__)

#if !defined(_ASM)
#include <sys/struct_regs.h>
#endif

/*
 * Distance from beginning of thread stack (t_stk) to saved regs struct.
 */
#define	REGOFF	MINFRAME

#endif /* !defined(_XPG4_2) || defined(__EXTENSIONS__) */

#ifdef	_KERNEL
#define	lwptoregs(lwp)	((struct regs *)((lwp)->lwp_regs))
#define	lwptofpu(lwp)	((struct fpu *)((lwp)->lwp_fpu))
#endif	/* _KERNEL */

#if !defined(_XPG4_2) || defined(__EXTENSIONS__)

/* Bit definitions in Condition Register for CR0 */
#define	CR0_LT	0x80000000	/* Negative bit in CR0 */
#define	CR0_GT	0x40000000	/* Positive bit in CR0 */
#define	CR0_EQ	0x20000000	/* Zero bit in CR0 */
#define	CR0_SO	0x10000000	/* Summary Overflow bit in CR0 */

/* Bit definitions in XER (Integer Exception Register) */
#define	XER_SO	0x80000000	/* Summary Overflow bit */
#define	XER_OV	0x40000000	/* Overflow bit */
#define	XER_CA	0x20000000	/* Carry bit */

#endif /* !defined(_XPG4_2) || defined(__EXTENSIONS__) */

#include <sys/regset_fp.h>

#if !defined(_XPG4_2) || defined(__EXTENSIONS__)

#include <sys/reg_fp.h>

#endif /* !defined(_XPG4_2) || defined(__EXTENSIONS__) */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_REG_H */
