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

#ifndef _SYS_PSW_H
#define	_SYS_PSW_H

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#include <sys/msr.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Definition of bits in the PowerPC MSR (Machine Status Register).
 *
 * NOTE: The MSR is a 32-bit register on the MPU601,
 *       but is a 64-bit register on other PowerPC processors.
 *
 * NOTE: The previous note is somewhat out of date.
 *       There are modern 32-bit PowerPC processors
 *       with 32-bit MSR register.
 *
 *
 * MSR bits defined in the PowerPC architecture, present on the 601.
 *
 * NOTE:
 *   The names of the form *_FLD_MASK are generated.
 *   See usr/src/uts/ppc/sysgen/ .fd files.
 *   The shorter names are what have been used, traditionally.
 *   We keep them around, for now.
 */
#define	MSR_EE	MSR_EE_FLD_MASK
#define	MSR_PR	MSR_PR_FLD_MASK
#define	MSR_FP	MSR_FP_FLD_MASK
#define	MSR_ME	MSR_ME_FLD_MASK
#define	MSR_FE0	MSR_FE0_FLD_MASK
#define	MSR_SE	MSR_SE_FLD_MASK
#define	MSR_FE1	MSR_FE1_FLD_MASK
#define	MSR_IR	MSR_IR_FLD_MASK
#define	MSR_DR	MSR_DR_FLD_MASK

#if !defined(_ASM)
typedef int	psw_t;
#endif	/* !defined(_ASM) */

/*
 * MSR bits that may be changed by user processes.
 */
#define	MSR_USER_BITS \
	(MSR_PR_FLD_MASK | MSR_FE0_FLD_MASK | MSR_FE1_FLD_MASK)
#define	MSR_USER_BITS_MASK \
	(MSR_FP_FLD_MASK)

/*
 * PSL_USER - initial value of MSR for user thread.
 * This is used in generic code also.
 */
#if !defined(_ASM)
#include <sys/ppc_instr.h>
#define	PSL_USER ((mfmsr() & ~MSR_USER_BITS_MASK) | MSR_USER_BITS)
#endif	/* !defined(_ASM) */

/*
 * Macros to decode MSR.
 *
 */
#define	USERMODE(msr)	(((msr) & MSR_PR) != 0)

#include <sys/spl.h>

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PSW_H */
