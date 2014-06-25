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

#ifndef	_SYS_REG_GP_H
#define	_SYS_REG_GP_H

#pragma ident	"%Z%%M%	%I%	%E% SMI"

/*
 * Definitions of PPC general-purpose registers and their locations (offsets)
 * within a register save area.
 *
 * Pure definitions.  For use by reg.h and regset.h
 * Defined here to eliminate redundant definitions
 * that used to be in reg.h and regset.h
 */

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * A gregset_t is defined as an array type for compatibility with
 * the reference source.  This is important due to differences in
 * the way the C language treats arrays and structures as parameters.
 *
 * Note that NGREG is really (sizeof (struct regs) / sizeof (greg_t)),
 * but that the ABI defines it absolutely to be 39.
 */
#define	_NGREG	39

#if !defined(_XPG4_2) || defined(__EXTENSIONS__)

#define	NGREG	_NGREG

/*
 * Location of the users' stored registers.
 * Usage is u.u_ar0[XX].
 */

#define	R_R0	0
#define	R_R1	1
#define	R_R2	2
#define	R_R3	3
#define	R_R4	4
#define	R_R5	5
#define	R_R6	6
#define	R_R7	7
#define	R_R8	8
#define	R_R9	9
#define	R_R10	10
#define	R_R11	11
#define	R_R12	12
#define	R_R13	13
#define	R_R14	14
#define	R_R15	15
#define	R_R16	16
#define	R_R17	17
#define	R_R18	18
#define	R_R19	19
#define	R_R20	20
#define	R_R21	21
#define	R_R22	22
#define	R_R23	23
#define	R_R24	24
#define	R_R25	25
#define	R_R26	26
#define	R_R27	27
#define	R_R28	28
#define	R_R29	29
#define	R_R30	30
#define	R_R31	31
#define	R_CR	32
#define	R_LR	33
#define	R_PC	34
#define	R_MSR	35
#define	R_CTR	36
#define	R_XER	37
#define	R_MQ	38
/*
 * NOTE: The MQ register is available only on the MPU601, and contains the
 * product/dividend for multiply/divide operations.  It is included at the
 * end of the register save area, so that non-ABI compliant software that
 * utilizes this 601-specific register will not be clobbered during context
 * switches.
 */

#endif /* !defined(_XPG4_2) || defined(__EXTENSIONS__) */

#if defined(_ASM)

#include <sys/reg_gpr.h>

#else	/* !defined(_ASM) */

#include <sys/isa_defs.h>
#include <sys/types.h>

#if defined(__powerpc64)
typedef uint64_t	greg_t;
#elif defined(__powerpc)
typedef uint32_t	greg_t;
#elif defined(__x86)
/*
 * This is here just to allow genassym to work.
 * Genassym runs Sun's C compiler on an x86 build machine,
 * which, of course, does not have any builtin definitions
 * for PowerPC targets.  Get rid of this as soon as we get
 * a sane set of build tools -- either native PowerPC tools
 * or tools that are smarter about cross-compilation.
 */
typedef uint32_t	greg_t;
#else
#error "Not __powerpc64 and not __powerpc."
#endif

typedef greg_t		gregset_t[_NGREG];

#endif	/* !defined(_ASM) */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_REG_GP_H */
