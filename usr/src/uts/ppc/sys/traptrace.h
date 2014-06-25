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

/* 2.6_leveraged #pragma ident	"@(#)traptrace.h	1.2	95/09/05 SMI" */

#ifndef _SYS_TRAPTRACE_H
#define	_SYS_TRAPTRACE_H

#pragma ident	"%Z%%M%	%I%	%E% CP"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Trap tracing.  If TRAPTRACE is defined, every trap records info
 * in a circular buffer.  Define TRAPTRACE in Makefile.chrp.
 *
 * Trap trace records are 8 words, consisting of the trap type, %msr, %pc, %sp,
 * THREAD_REG, and up to three other words.
 *
 * Auxilliary entries (not of just a trap), have obvious non-trap-type values
 * in the first word.
 */
#define	TRAP_ENT_TT	0		/* trap/interrupt/exception type */
#define	TRAP_ENT_MSR	4		/* msr before trap or at trace time */
#define	TRAP_ENT_PC	8		/* pc */
#define	TRAP_ENT_SP	0x0c		/* stack pointer */
#define	TRAP_ENT_R2	0x10		/* thread pointer */
#define	TRAP_ENT_X1	0x14		/* extra type-specific info */
#define	TRAP_ENT_X2	0x18		/* extra type-specific info */
#define	TRAP_ENT_X3	0x1c		/* extra type-specific info */
#define	TRAP_ENT_SIZE	32
#define	TRAP_TSIZE	(TRAP_ENT_SIZE*256)

/*
 * Trap tracing buffer header.
 */

/*
 * Example buffer header in locore.s:
 *
 * trap_trace_ctlp:
 *	.word	trap_trace_ctl		! pointer to trap_trace_ctl area
 *					! ptr may be set to special trace RAM
 *
 * trap_trace_ctl:
 * 	.word	trap_tr0		! next	CPU 0
 * 	.word	trap_tr0		! first
 * 	.word	trap_tr0 + TRAP_TSIZE	! limit
 * 	.word	0			! junk for alignment of prom dump
 *
 * 	.word	trap_tr1		! next	CPU 1
 * 	.word	trap_tr1		! first
 * 	.word	trap_tr1 + TRAP_TSIZE	! limit
 * 	.word	0			! junk for alignment of prom dump
 *
 * 	.word	trap_tr2		! next	CPU 2
 * 	.word	trap_tr2		! first
 * 	.word	trap_tr2 + TRAP_TSIZE	! limit
 * 	.word	0			! junk for alignment of prom dump
 *
 * 	.word	trap_tr3		! next	CPU 3
 * 	.word	trap_tr3		! first
 * 	.word	trap_tr3 + TRAP_TSIZE	! limit
 * 	.word	0			! junk for alignment of prom dump
 * 	.align	16
 *
 * Offsets of words in trap_trace_ctl:
 */
#define	TRAPTR_NEXT	0		/* next trace entry pointer */
#define	TRAPTR_FIRST	4		/* start of buffer */
#define	TRAPTR_LIMIT	8		/* pointer past end of buffer */

#define	TRAPTR_SIZE_SHIFT	4	/* shift count for CPU indexing */

#ifdef	_ASM

/*
 * TRACE_PTR(ptr, scr1) - get trap trace entry pointer.
 *	ptr is the register to receive the trace pointer.
 *	scr1 is a different register to be used as scratch.
 */
#ifdef MP
#define	TRACE_PTR(ptr, scr1)			\
	CPU_INDEX(scr1);			\
	sli	scr1, scr1, TRAPTR_SIZE_SHIFT;	\
	lis	ptr, trap_trace_ctlp@ha; 	\
	lwz	ptr, trap_trace_ctlp@l(ptr);	\
	lwzx	ptr, ptr, scr1
#else /* MP */
#define	TRACE_PTR(ptr, scr1)			\
	lis	scr1, trap_trace_ctlp@ha; 	\
	lwz	ptr, trap_trace_ctlp@l(scr1);	\
	lwz	ptr, TRAPTR_NEXT(ptr)
#endif /* MP */

/*
 * TRACE_NEXT(ptr, scr1, scr2) - advance the trap trace pointer.
 *	ptr is the register holding the current trace pointer (from TRACE_PTR).
 *	scr1, and scr2 are scratch registers (different from ptr).
 */
#ifdef MP
#define	TRACE_NEXT(ptr, scr1, scr2)		\
	CPU_INDEX(scr1);			\
	sli	scr1, src1, TRAPTR_SIZE_SHIFT;	\
	lis	scr2, trap_trace_ctlp@ha; 	\
	lwz	scr2, trap_trace_ctlp@l(scr2);	\
	add	scr1, scr1, scr2;		\
	addi	ptr, ptr, TRAP_ENT_SIZE;	\
	lwz	scr2, TRAPTR_LIMIT(scr1);	\
	cmplw	ptr, scr2;			\
	/* CSTYLED */				\
	blt+	.+8; /* note relative branch */ \
	lwz	ptr, TRAPTR_FIRST(scr1);	\
	stw	ptr, TRAPTR_NEXT(scr1)
#else /* MP */
#define	TRACE_NEXT(ptr, scr1, scr2)		\
	lis	scr2, trap_trace_ctlp@ha; 	\
	lwz	scr1, trap_trace_ctlp@l(scr2);	\
	addi	ptr, ptr, TRAP_ENT_SIZE;	\
	lwz	scr2, TRAPTR_LIMIT(scr1);	\
	cmplw	ptr, scr2;			\
	/* CSTYLED */				\
	blt+	.+8; /* note relative branch */ \
	lwz	ptr, TRAPTR_FIRST(scr1);	\
	stw	ptr, TRAPTR_NEXT(scr1)
#endif /* MP */

/*
 * Trace macro for underflow or overflow trap handler
 */
#ifdef TRAPTRACE

#define	TRACE_TRAP(code, addr, scr1, scr2, scr3) \
	TRACE_PTR(scr1, scr2);			\
	lis	scr2, code;			\
	stw	scr2, TRAP_ENT_TT(scr1);	\
	mfmsr	scr2;				\
	stw	scr2, TRAP_ENT_MSR(scr1);	\
	lis	scr2, 0;			\
	stw	scr2, TRAP_ENT_PC(scr1);	\
	stw	addr, TRAP_ENT_SP(scr1);	\
	stw	r2, TRAP_ENT_R2(scr1);		\
	TRACE_NEXT(scr1, scr2, scr3)


#else /* !TRAPTRACE */

#define	TRACE_TRAP(code, addr, scr1, scr2, scr3)

#endif	/* TRAPTRACE */

#endif	/* _ASM */

#if defined(TRAPTRACE)

#ifdef XXX_IMPLEMENT_TRAPTRACE_FREEZE

extern int trap_trace_freeze;

#define	TRAPTRACE_FREEZE	trap_trace_freeze = 1;
#define	TRAPTRACE_UNFREEZE	trap_trace_freeze = 0;

#else
#define	TRAPTRACE_FREEZE
#define	TRAPTRACE_UNFREEZE
#endif

#endif /* defined(TRAPTRACE) */


/*
 * Trap trace codes used in place of a trap type value when more than one
 * entry is made by a trap.  The general scheme is that the trap-type is
 * in the same position as in the trap type, and the low-order bits indicate
 * which precise entry is being made.
 */
#define	TT_SC_RET	0xc01	/* system call normal return */
#define	TT_SC_POST	0xc02	/* system call return after post_syscall */

#define	TT_SYS_RTT	0x6666	/* return from trap */
#define	TT_SYS_RTTU	0x7777	/* return from trap to user */

#define	TT_INTR_ENT	-1	/* interrupt entry */
#define	TT_INTR_RET	-2	/* interrupt return */
#define	TT_INTR_RET2	-3	/* interrupt return */
#define	TT_INTR_EXIT	0x8888	/* interrupt thread exit (no pinned thread) */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_TRAPTRACE_H */
