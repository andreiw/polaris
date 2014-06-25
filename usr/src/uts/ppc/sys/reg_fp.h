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

#ifndef	_SYS_REG_FP_H
#define	_SYS_REG_FP_H

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * PowerPC floating-point processor definitions
 */

/*
 * values that go into fp_version
 * NOTE: on the 60x series, this should always be FP_HW, because the
 * CPU contains an integrated Integer Unit, Branch Processor, and
 * Floating Point Unit.
 */

#if !defined(_XPG4_2) || defined(__EXTENSIONS__)

#define	FP_NO   0	/* no fp chip, no emulator (no fp support) */
#define	FP_HW   1	/* fp processor present bit */

/*
 * masks for PowerPC FPSCR (Floating Point Status and Control Register)
 */
/*
 * Floating point exception summary bits
 */
#define	FP_EXCEPTN_SUMM	   0x80000000	/* FX - exception summary	*/
#define	FP_ENAB_EX_SUMM	   0x40000000	/* FEX-enabled exception summary */
#define	FP_INV_OP_EX_SUMM  0x20000000	/* VX-invalid op except'n summary */

/*
 * Floating point exception bits
 */
#define	FP_OVERFLOW	   0x10000000	/* OX - overflow exception bit	*/
#define	FP_UNDERFLOW	   0x08000000	/* UX - underflow exception bit */
#define	FP_ZERODIVIDE	   0x04000000	/* ZX - zerodivide except'n bit */
#define	FP_INEXACT	   0x02000000	/* XX - inexact exception bit	*/

/*
 * Floating point invalid operation exception bits
 */
#define	FP_INV_OP_SNaN	   0x01000000	/* VXSNaN - signalled NaN	*/
#define	FP_INV_OP_ISI	   0x00800000	/* VXISI - infinity-infinity	*/
#define	FP_INV_OP_IDI	   0x00400000	/* VXIDI - infinity/infinity	*/
#define	FP_INV_OP_ZDZ	   0x00200000	/* VXZDZ - 0/0			*/
#define	FP_INV_OP_IMZ	   0x00100000	/* VXIMZ - infinity*0		*/
#define	FP_INV_OP_VC	   0x00080000	/* VXVC - invalid compare	*/
#define	FP_INV_OP_SOFT	   0x00000400	/* VXSOFT - software request	*/
#define	FP_INV_OP_SQRT	   0x00000200	/* VXSQRT - invalid square root	*/
#define	FP_INV_OP_CVI	   0x00000100	/* VXCVI - invalid integer convert */

/*
 * Floating point status bits
 */
#define	FP_FRAC_ROUND	   0x00040000	/* FR - fraction rounded	*/
#define	FP_FRAC_INEXACT	   0X00020000	/* FI - fraction inexact	*/

/*
 * Floating point result flags (FPRF group)
 */
#define	FP_RESULT_CLASS	   0x00010000	/* C-result class descriptor	*/
/*
 * Floating point condition codes (FPRF:FPCC)
 */
#define	FP_LT_OR_NEG	   0x00008000	/* - normalized number		*/
#define	FP_GT_OR_POS	   0X00004000	/* + normalized number		*/
#define	FP_EQ_OR_ZERO	   0X00002200	/* + zero			*/
#define	FP_UN_OR_NaN	   0X00001000	/* unordered or Not a Number	*/

/*
 * Floating point exception enable bits
 */
#define	FP_INV_OP_ENAB	   0x00000080	/* VE-invalid operation enable	*/
#define	FP_OVRFLW_ENAB	   0x00000040	/* OE-overflow exception enable	*/
#define	FP_UNDRFLW_ENAB	   0x00000020	/* UE-underflow exception enable */
#define	FP_ZERODIV_ENAB	   0x00000010	/* ZE-zero divide exception enable */
#define	FP_INEXACT_ENAB	   0x00000008	/* XE-inexact exception enable	*/

/*
 * NOTE: This is an implementation-dependent bit, and is not used
 * on the MPU601.  If set, the results of floating point operations
 * may not conform to the IEEE standard; i.e., approximate results.
 */
#define	FP_NON_IEEE_MODE   0x00000004	/* NI - non-IEEE mode bit	*/

/*
 * Floating point rounding control bits (RN)
 */
#define	FP_ROUND_CTL1	   0x00000002
#define	FP_ROUND_CTL2	   0x00000001

/*
 * FPSCR Rounding Control Options
 *	00 Round to nearest
 *	01 Round toward zero
 *	10 Round toward +infinity
 *	11 Round toward -infinity
 */
#define	FP_ROUND_NEAREST   0x00000000	/* 00 - round to nearest	*/
#define	FP_ROUND_ZERO	   0x00000001	/* 01 - round toward zero	*/
#define	FP_ROUND_PLUS_INF  0x00000002	/* 10 - round toward +infinity	*/
#define	FP_ROUND_MINUS_INF 0x00000003	/* 11 - round toward -infinity	*/

/* Initial value of FPSCR as per ABI document */
#define	FPSCR_HW_INIT 0

#ifndef _ASM
extern int fp_version;			/* kind of fp support		*/
extern int fpu_exists;			/* FPU hw exists		*/
#endif

#endif /* !defined(_XPG4_2) || defined(__EXTENSIONS__) */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_REG_FP_H */
