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

#ifndef _SYS_PPC_INSTR_H
#define	_SYS_PPC_INSTR_H

#pragma ident	"%Z%%M%	%I%	%E% SMI"

/*
 * Inline definitions of PowerPC instructions.
 *
 * These functions allow access to PowerPC instructions
 * as inline C functions.  There is almost no abstraction.
 * There is a one-to-one correspondence between functions
 * in this header file and individual PowerPC machine
 * instructions.  The only exception is that some functions
 * have a foo_sync variant, where the foo instruction is
 * immediately followed by a sync instruction.  This is
 * because we want the foo and the sync defined in the same
 * asm() block, to avoid having the compiler move instructions
 * around.
 *
 * Other PPC related subroutines build on these instructions.
 * See, for example, usr/src/uts/ppc/sys/ppc_subr.h for more
 * complex functions that are still pretty small and should be
 * inline, but are more that just single machine instructions.
 * And see usr/src/uts/ppc/os/ppc_subr.c for some PPC-specific
 * subroutines that are a bit large for inlining, or that need
 * to be in memory, because some code uses the function's address.
 *
 * Functions are defined in alphabetical order.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>

/*
 * cntlzw - Count Leading Zeros Word
 */
static __inline__ uint_t
cntlzw(uint_t bits)
{
#if !defined(lint)
	uint_t count;

	__asm__(
	"	cntlzw	%[count],%[bits]"
	: [count] "=r" (count)
	: [bits] "r" (bits)
	/* */);
	return (count);
#endif /* !defined(lint) */
}

/*
 * dcbst - Data Cache Block Store
 */
static __inline__ void
dcbst(caddr_t addr)
{
#if !defined(lint)
	__asm__(
	"	dcbst	0,%[addr]"
	: /* No outputs */
	: [addr] "r" (addr)
	/* */);
#endif /* !defined(lint) */
}

/*
 * dcbst2 - Data Cache Block Store - 2 address form
 */
static __inline__ void
dcbst2(caddr_t addr1, caddr_t addr2)
{
#if !defined(lint)
	__asm__(
	"	dcbst	%[addr1],%[addr2]"
	: /* No outputs */
	: [addr1] "r" (addr1), [addr2] "r" (addr2)
	/* */);
#endif /* !defined(lint) */
}

/*
 * eieio - Enforce In-Order Execution of I/O
 */
static __inline__ void
eieio(void)
{
#if !defined(lint)
	__asm__ __volatile__(
	"	eieio"
	: /* No outputs */
	: /* No inputs */
	/* */);
#endif /* !defined(lint) */
}

/*
 * icbi - Instruction Cache Block Invalidate
 */
static __inline__ void
icbi(caddr_t addr)
{
#if !defined(lint)
	__asm__(
	"	icbi	0,%[addr]"
	: /* No outputs */
	: [addr] "r" (addr)
	/* */);
#endif /* !defined(lint) */
}

/*
 * icbi2 - Instruction Cache Block Invalidate - 2 address form
 */
static __inline__ void
icbi2(caddr_t addr1, caddr_t addr2)
{
#if !defined(lint)
	__asm__(
	"	icbi	%[addr1],%[addr2]"
	: /* No outputs */
	: [addr1] "r" (addr1), [addr2] "r" (addr2)
	/* */);
#endif /* !defined(lint) */
}

/*
 * isync - Instruction Synchonize
 */
static __inline__ void
isync(void)
{
#if !defined(lint)
	__asm__ __volatile__(
	"	isync"
	: /* No outputs */
	: /* No inputs */
	/* */);
#endif /* !defined(lint) */
}

/*
 * mflr - Move From Link Register
 */
static __inline__ uint_t
mflr(void)
{
#if !defined(lint)
	uint_t val;

	__asm__(
	"	mflr	%[val]"
	: [val] "=r" (val)
	: /* No inputs */
	/* */);
	return (val);
#endif /* !defined(lint) */
}

/*
 * mfmsr - Move From Machine State Register
 */
static __inline__ uint_t
mfmsr(void)
{
#if !defined(lint)
	uint_t val;

	__asm__(
	"	mfmsr	%[val]"
	: [val] "=r" (val)
	: /* No inputs */
	/* */);
	return (val);
#endif /* !defined(lint) */
}

/*
 * mfsdr1 - Move From Special-Purpose Register SDR1
 */
static __inline__ uint_t
mfsdr1(void)
{
#if !defined(lint)
	uint_t val;

	__asm__(
	"	mfsdr1	%[val]"
	: [val] "=r" (val)
	: /* No inputs */
	/* */);
	return (val);
#endif /* !defined(lint) */
}

/*
 * mfspr - Move From Special-Purpose Register
 */
static __inline__ uint_t
mfspr(const uint_t spr)
{
#if !defined(lint)
	uint_t val;

	__asm__(
	"	mfspr	%[val],%[spr]"
	: [val] "=r" (val)
	: [spr] "i" (spr)
	/* */);
	return (val);
#endif /* !defined(lint) */
}

/*
 * mfsr - Move From Segment Register
 */
static __inline__ uint_t
mfsr(const uint_t segn)
{
#if !defined(lint)
	uint_t segval;

	__asm__(
	"	mfsr	%[segval],%[segn]"
	: [segval] "=r" (segval)
	: [segn] "i" (segn)
	/* */);
	return (segval);
#endif /* !defined(lint) */
}

/*
 * mfsrin - Move From Segment Register Indirect
 */
static __inline__ uint_t
mfsrin(caddr_t segn)
{
#if !defined(lint)
	uint_t segval;

	__asm__(
	"	mfsrin	%[segval],%[segn]"
	: [segval] "=r" (segval)
	: [segn] "r" (segn)
	/* */);
	return (segval);
#endif /* !defined(lint) */
}

/*
 * mtmsr - Move To Machine State Register
 */
static __inline__ void
mtmsr(uint_t val)
{
#if !defined(lint)
	__asm__(
	"	mtmsr	%[val]"
	: /* No outputs */
	: [val] "r" (val)
	/* */);
#endif /* !defined(lint) */
}

/*
 * mtsdr1 - Move To Special-Purpose Register SDR1
 */
static __inline__ void
mtsdr1(uint_t val)
{
#if !defined(lint)
	__asm__(
	"	mtsdr1	%[val]"
	: /* No outputs */
	: [val] "r" (val)
	/* */);
#endif /* !defined(lint) */
}

/*
 * mtspr - Move To Special-Purpose Register
 *
 * Note:
 *   No registers are declared in the clobber list.
 *   What gets clobbered depends on the specified SPR.
 *   We cannot tell which, statically.
 *   Do not use mtspr() in a way that interferes with
 *   any SPR that the C compiler schedules.
 *   E.g.: XER, LR, CTR.
 *   See MPCFPE32B, page 8-137.
 */
static __inline__ void
mtspr(const uint_t spr, uint_t val)
{
#if !defined(lint)
	__asm__(
	"	mtspr	%[spr],%[val]"
	: /* No outputs */
	: [spr] "i" (spr), [val] "r" (val)
	/* */);
#endif /* !defined(lint) */
}

/*
 * mtsr - Move To Segment Register
 */
static __inline__ void
mtsr(const uint_t segn, uint_t segval)
{
#if !defined(lint)
	__asm__(
	"	mtsr	%[segn],%[segval]"
	: /* No outputs */
	: [segn] "i" (segn), [segval] "r" (segval)
	/* */);
#endif /* !defined(lint) */
}

/*
 * mtsr_sync - Move To Segment Register; then Synchronize
 */
static __inline__ void
mtsr_sync(const uint_t segn, uint_t segval)
{
#if !defined(lint)
	__asm__(
	"	mtsr	%[segn],%[segval]\n"
	"	sync"
	: /* No outputs */
	: [segn] "i" (segn), [segval] "r" (segval)
	/* */);
#endif /* !defined(lint) */
}

/*
 * mtsrin - Move To Segment Register Indirect
 */
static __inline__ void
mtsrin(uint_t segval, caddr_t segn)
{
#if !defined(lint)
	__asm__(
	"	mtsrin	%[segval],%[segn]"
	: /* No outputs */
	: [segval] "r" (segval), [segn] "r" (segn)
	/* */);
#endif /* !defined(lint) */
}

/*
 * mtsrin_sync - Move To Segment Register Indirect; then Synchronize
 */
static __inline__ void
mtsrin_sync(uint_t segval, caddr_t segn)
{
#if !defined(lint)
	__asm__(
	"	mtsrin	%[segval],%[segn]\n"
	"	sync"
	: /* No outputs */
	: [segval] "r" (segval), [segn] "r" (segn)
	/* */);
#endif /* !defined(lint) */
}

/*
 * rfi - Return From Interrupt
 */

static __inline__ void
rfi(void)
{
#if !defined(lint)
	__asm__ __volatile__(
	"	rfi"
	: /* No outputs */
	: /* No inputs */
	/* */);
#endif /* !defined(lint) */
}

/*
 * sync - Synchonize
 *
 * sync() conflicts with the declaration of sync()
 * in usr/src/uts/common/sys/vfs.h  which synchonizes
 * a filesystem.
 *
 * We use ppc_sync(), instead.
 */
static __inline__ void
ppc_sync(void)
{
#if !defined(lint)
	__asm__ __volatile__(
	"	sync"
	: /* No outputs */
	: /* No inputs */
	/* */);
#endif /* !defined(lint) */
}

/*
 * tlbie - Translation Lookaside Buffer Invalidate Entry
 */
static __inline__ void
tlbie(caddr_t addr)
{
#if !defined(lint)
	__asm__(
	"	tlbie	%[addr]\n"
	: /* No outputs */
	: [addr] "r" (addr)
	/* */);
#endif /* !defined(lint) */
}

#ifdef __cplusplus
}
#endif

#endif /* _SYS_PPC_INSTR_H */
