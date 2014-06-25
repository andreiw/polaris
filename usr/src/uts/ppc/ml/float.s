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

	.file	"float.s"
	.file	"%M%"

#pragma ident "@(#)float.s	1.10	96/04/01 SMI"

/*
 * PowerPC floating point support routines
 */

#include <sys/asm_linkage.h>
#include <sys/psw.h>
#include <sys/reg.h>
#include <sys/reg_fpr.h>

#if defined(lint) || defined(__lint)

#include <sys/types.h>
#include <sys/thread.h>
#include <sys/fpu/fpusystm.h>

int fpu_exists = 1;
int fpu_version = FP_HW;

#else	/* ! defined(lint) */

	.data
	.align  2
	.globl fpu_exists
	.globl fp_version			! FP_NO, FP_HW
	.globl initial_fpu_state		! initial FP state for an lwp

fpu_exists:
	.long   1

fp_version:
	.long   FP_HW

fpu_init_reg:
	.long	0

	.align	3
	/* Initial FPU state - fpu structure initialization */
initial_fpu_state:
	.long	-1, -1		! %f0 = NaN
	.long	-1, -1		! %f1 = NaN
	.long	-1, -1		! %f2 = NaN
	.long	-1, -1		! %f3 = NaN
	.long	-1, -1		! %f4 = NaN
	.long	-1, -1		! %f5 = NaN
	.long	-1, -1		! %f6 = NaN
	.long	-1, -1		! %f7 = NaN
	.long	-1, -1		! %f8 = NaN
	.long	-1, -1		! %f9 = NaN
	.long	-1, -1		! %f10 = NaN
	.long	-1, -1		! %f11 = NaN
	.long	-1, -1		! %f12 = NaN
	.long	-1, -1		! %f13 = NaN
	.long	-1, -1		! %f14 = NaN
	.long	-1, -1		! %f15 = NaN
	.long	-1, -1		! %f16 = NaN
	.long	-1, -1		! %f17 = NaN
	.long	-1, -1		! %f18 = NaN
	.long	-1, -1		! %f19 = NaN
	.long	-1, -1		! %f20 = NaN
	.long	-1, -1		! %f21 = NaN
	.long	-1, -1		! %f22 = NaN
	.long	-1, -1		! %f23 = NaN
	.long	-1, -1		! %f24 = NaN
	.long	-1, -1		! %f25 = NaN
	.long	-1, -1		! %f26 = NaN
	.long	-1, -1		! %f27 = NaN
	.long	-1, -1		! %f28 = NaN
	.long	-1, -1		! %f29 = NaN
	.long	-1, -1		! %f30 = NaN
	.long	-1, -1		! %f31 = NaN
	.long	FPSCR_HW_INIT	! %fpscr = FPSCR_HW_INIT (ABI specified)
	.long	1		! fpu_valid = 1

	.text
	.align  2

#endif	/* defined(lint) */


/*
 * fpu_probe(void)
 *
 * trivial check for presence of floating point processor - always present
 * on a PowerPC MPU601!
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
fpu_probe(void)
{}

#else	/* ! defined(lint) */

	ENTRY_NP(fpu_probe)
	blr
	SET_SIZE(fpu_probe)

#endif	/* defined(lint) */


/*
 * fpu_save(struct fpu *fp)
 *
 * Store the floating point state and disable the floating point unit.
 * Note that structure alignment in this implementation assumes the
 * alignment of its most strictly aligned member; i.e., each member
 * is aligned on the same boundary as the component with the largest
 * alignment.
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
fpu_save(struct fpu *fp)
{}

#else	/* ! defined(lint) */

	ENTRY_NP(fpu_save)

	!mfmsr	r4
	!ori	r4, r4, MSR_FP
	!mtmsr	r4			! enable FPU
	stfd	f31, 248(r3)
	li	r4,	0x0001		! fpu_valid
	mffs	f31			! get FPSCR snapshot
	stfd	f0,	0(r3)
	stfd	f1,	8(r3)
	stfd	f2,	16(r3)
	stfd	f3,	24(r3)
	stfd	f4,	32(r3)
	stfd	f5,	40(r3)
	stfd	f6,	48(r3)
	stfd	f7,	56(r3)
	stfd	f8,	64(r3)
	stfd	f9,	72(r3)
	stfd	f10,	80(r3)
	stfd	f11,	88(r3)
	stfd	f12,	96(r3)
	stfd	f13,	104(r3)
	stfd	f14,	112(r3)
	stfd	f15,	120(r3)
	stfd	f16,	128(r3)
	stfd	f17,	136(r3)
	stfd	f18,	144(r3)
	stfd	f19,	152(r3)
	stfd	f20,	160(r3)
	stfd	f21,	168(r3)
	stfd	f22,	176(r3)
	stfd	f23,	184(r3)
	stfd	f24,	192(r3)
	stfd	f25,	200(r3)
	stfd	f26,	208(r3)
	stfd	f27,	216(r3)
	stfd	f28,	224(r3)
	stfd	f29,	232(r3)
	stfd	f30,	240(r3)
	stfd	f31,	256(r3) 	! save FPSCR
	stw	r4,	260(r3)
	lfd	f31,	248(r3)	! to keep the FP hw state intact
	blr

	SET_SIZE(fpu_save)

#endif	/* defined(lint) */


/*
 * fpu_restore(struct fpu *fp)
 *
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
fpu_restore(struct fpu *fp)
{}

#else	/* ! defined(lint) */

	ENTRY_NP(fpu_restore)

	!mfmsr	r4
	!ori	r4, r4, MSR_FP
	!mtmsr	r4			! enable FPU
	lfd	f31,	256(r3)	! get the saved FPSCR value
	li	r4,	0x0000		! reset fpu_valid
	lfd	f0,	0(r3)
	lfd	f1,	8(r3)
	lfd	f2,	16(r3)
	lfd	f3,	24(r3)
	lfd	f4,	32(r3)
	lfd	f5,	40(r3)
	lfd	f6,	48(r3)
	lfd	f7,	56(r3)
	lfd	f8,	64(r3)
	lfd	f9,	72(r3)
	lfd	f10,	80(r3)
	lfd	f11,	88(r3)
	lfd	f12,	96(r3)
	lfd	f13,	104(r3)
	lfd	f14,	112(r3)
	lfd	f15,	120(r3)
	lfd	f16,	128(r3)
	mtfsf	0xff,	f31		! restore FPSCR
	lfd	f17,	136(r3)
	lfd	f18,	144(r3)
	lfd	f19,	152(r3)
	lfd	f20,	160(r3)
	lfd	f21,	168(r3)
	lfd	f22,	176(r3)
	lfd	f23,	184(r3)
	lfd	f24,	192(r3)
	lfd	f25,	200(r3)
	lfd	f26,	208(r3)
	lfd	f27,	216(r3)
	lfd	f28,	224(r3)
	lfd	f29,	232(r3)
	lfd	f30,	240(r3)
	lfd	f31,	248(r3)
	stw	r4,	260(r3)	! fp soft state no longer valid
	blr

	SET_SIZE(fpu_restore)

#endif	/* defined(lint) */

/*
 * fpu_hw_init(void)
 *
 * Initialize the fpu hardware.  Called at time of creating a FP context
 * for the thread.
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
fpu_hw_init(void)
{}

#else	/* ! defined(lint) */

	.data
	.align	3

fpscr_init_mask:
	.long	FPSCR_HW_INIT			! defined in ABI
	.long	0
fpreg_NaN:
	.long	-1
	.long	-1
	.text

	ENTRY_NP(fpu_hw_init)

	lis	r3, fpreg_NaN@ha
	lis	r4, fpscr_init_mask@ha
	la	r3, fpreg_NaN@l(r3)
	la	r4, fpscr_init_mask@l(r4)
	lfd	f1, 0(r4)
	mtfsf	0xff, f1			! initialize FPSCR
	!
	! initialize fp regs with NaN.
	!
	lfd	f0, 0(r3)
	fmr	f1, f0
	fmr	f2, f0
	fmr	f3, f0
	fmr	f4, f0
	fmr	f5, f0
	fmr	f6, f0
	fmr	f7, f0
	fmr	f8, f0
	fmr	f9, f0
	fmr	f10, f0
	fmr	f11, f0
	fmr	f12, f0
	fmr	f13, f0
	fmr	f14, f0
	fmr	f15, f0
	fmr	f16, f0
	fmr	f17, f0
	fmr	f18, f0
	fmr	f19, f0
	fmr	f20, f0
	fmr	f21, f0
	fmr	f22, f0
	fmr	f23, f0
	fmr	f24, f0
	fmr	f25, f0
	fmr	f26, f0
	fmr	f27, f0
	fmr	f28, f0
	fmr	f29, f0
	fmr	f30, f0
	fmr	f31, f0
	blr

	SET_SIZE(fpu_hw_init)

#endif	/* defined(lint) */

/*
 * fperr_reset(void)
 *
 *	clear FPU exception state. Disable the FP exceptions (clear the
 *	exception enable bits in FPSCR).
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
u_int
fperr_reset(void)
{
	return (0);
}

#else	/* ! defined(lint) */

	ENTRY_NP(fperr_reset)
	subi	r1, r1, SA(16)	! temp space for fpscr
	stfd	f0, 8(r1)		! save f0
	mffs	f0			! get the current fpscr
	stfd	f0, 0(r1)
	lwz	r3, 0(r1)
	lis	r4, 0x1ff8		! exception sticky bit mask
	ori	r4, r4, 0x0700
	andc	r5, r3, r4		! clear the sticky bits from fpscr
	stw	r5, 0(r1)
	lfd	f0, 0(r1)
	mtfsf	0xff, f0		! restore FPSCR
	lfd	f0, 8(r1)		! restore f0
	addi	r1, r1, SA(16)
	blr
	SET_SIZE(fperr_reset)

#endif	/* defined(lint) */
