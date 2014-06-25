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

#ifndef	_SYS_STRUCT_REGS_H
#define	_SYS_STRUCT_REGS_H

#pragma ident	"%Z%%M%	%I%	%E% SMI"

/*
 * Definitions of PPC general-purpose registers
 * Pure definitions.  For use by reg.h and regset.h
 * Defined here to eliminate redundant definitions
 * that used to be in reg.h and regset.h
 */

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * This is a template used by trap() and syscall()
 * to find saved copies of the registers on the stack.
 *
 * From 32-bit PowerPC psABI ucontext.h
 */

struct regs {
	greg_t	r_r0;		/* GPRs 0 - 31 */
	greg_t	r_r1;
	greg_t	r_r2;
	greg_t	r_r3;
	greg_t	r_r4;
	greg_t	r_r5;
	greg_t	r_r6;
	greg_t	r_r7;
	greg_t	r_r8;
	greg_t	r_r9;
	greg_t	r_r10;
	greg_t	r_r11;
	greg_t	r_r12;
	greg_t	r_r13;
	greg_t	r_r14;
	greg_t	r_r15;
	greg_t	r_r16;
	greg_t	r_r17;
	greg_t	r_r18;
	greg_t	r_r19;
	greg_t	r_r20;
	greg_t	r_r21;
	greg_t	r_r22;
	greg_t	r_r23;
	greg_t	r_r24;
	greg_t	r_r25;
	greg_t	r_r26;
	greg_t	r_r27;
	greg_t	r_r28;
	greg_t	r_r29;
	greg_t	r_r30;
	greg_t	r_r31;
	greg_t	r_cr;		/* Condition Register */
	greg_t	r_lr;		/* Link Register */
	greg_t	r_pc;		/* User PC (Copy of SRR0) */
	greg_t	r_msr;		/* saved MSR (Copy of SRR1) */
	greg_t	r_ctr;		/* Count Register */
	greg_t	r_xer;		/* Integer Exception Register */
	greg_t	r_mq;		/* MQ Register (601 only) */
};

#define	r_sp	r_r1		/* user stack pointer */
#define	r_toc	r_r2		/* user TOC */
#define	r_ps	r_msr
#define	r_srr0	r_pc
#define	r_srr1	r_msr
#define	r_ret0	r_r3		/* return value register 0 */
#define	r_ret1	r_r4		/* return value register 1 */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_STRUCT_REGS_H */
