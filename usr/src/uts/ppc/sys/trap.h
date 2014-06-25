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

#ifndef _SYS_TRAP_H
#define	_SYS_TRAP_H

#pragma ident	"@(#)trap.h	1.7	94/12/22 SMI"
#pragma ident	"%Z%%M%	%I%	%E% SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Trap type values (vector offsets).
 */
#define	T_RESET		0x0100	/* reset 			*/
#define	T_MACH_CHECK	0x0200	/* machine check		*/
#define	T_DATA_FAULT	0x0300	/* data access			*/
#define	T_TEXT_FAULT	0x0400	/* instruction access		*/
#define	T_INTERRUPT	0x0500	/* external interrupt		*/
#define	T_ALIGNMENT	0x0600	/* alignment			*/
#define	T_PGM_CHECK	0x0700	/* program check		*/
#define	T_NO_FPU	0x0800	/* no FPU available		*/
#define	T_DECR		0x0900	/* Decrementer trap		*/
#define	T_IO_ERROR	0x0A00	/* I/O ERROR (MPC601 only)	*/
#define	T_SYSCALL	0x0C00	/* system call (sc instruction)	*/
#define	T_SINGLE_STEP	0x0D00	/* trace mode (PowerPC)		*/
#define	T_FP_ASSIST	0x0E00	/* floating-point assist (PowerPC) */
#define	T_PERF_MI	0x0F00	/* Performance monitor interrupt (MP604) */
#define	T_NO_AV		0x0F20	/* AltiVec Unavailable Exception */
#define	T_TLB_IMISS	0x1000	/* instruction translation miss (MP603) */
#define	T_TLB_DLOADMISS	0x1100	/* data load translation miss (MP603) */
#define	T_TLB_DSTOREMISS 0x1200	/* data store translation miss (MP603) */
#define	T_IABR		0x1300	/* instruction address breakpoint (PowerPC) */
#define	T_SYS_MGMT	0x1400	/* system management interrupt (PowerPC) */
#define	T_AV_ASSIST	0x1600	/* AltiVec assist */
#define	T_EXEC_MODE	0x2000	/* run mode exceptions (MPC601)	*/
#define	T_MAX		T_EXEC_MODE	/* Maximim known trap type */

/*
 * Pseudo (asynchronous) traps.
 */
#define	T_AST		0x4000

/*
 * Opcodes for 'trap' type instructions. XXXPPC
 */
#define	BPT_INST	0x0fe00000	/* breakpoint trap instruction */

/*
 * Fast syscall codes
 */
#define	SC_GETLWPFPU	1
#define	SC_GETHRTIME	2
#define	SC_GETHRVTIME	3
#define	SC_GETHRESTIME	4

/*
 * Data Access Exception - DSISR bit masks.
 */
#define	DSISR_INVAL	0x40000000	/* Set for invalid translations */
#define	DSISR_PROT	0x08000000	/* Set for protection error */
#define	DSISR_STORE	0x02000000	/* Set for write operation */
#define	DSISR_DATA_BKPT 0x00400000	/* Set for HW data break points */

/*
 * Instruction Access Exception - SRR1 bit masks at the time of exception.
 */
#define	SRR1_INVAL	0x40000000	/* Set for invalid mapping */
#define	SRR1_PROT	0x08000000	/* Set for protection violation */

/*
 * Program Check exception - SRR1 bit masks at the time of exception.
 */
#define	SRR1_FP_EN	0x00100000	/* Set for FP enabled exception */
#define	SRR1_ILL_INST	0x00080000	/* Set for illegal instruction */
#define	SRR1_PRIV_INST	0x00040000	/* Set for privileged instruction */
#define	SRR1_TRAP	0x00020000	/* Set for trap program exception */
#define	SRR1_SRR0_VALID	0x00010000	/* SRR0 contains address of: */
					/* 0-inst causing the exception; */
					/* 1-a subsequent instruction */

#define	SRR1_PCHK_MASK \
		(SRR1_FP_EN | SRR1_ILL_INST | SRR1_PRIV_INST | SRR1_TRAP)

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_TRAP_H */
