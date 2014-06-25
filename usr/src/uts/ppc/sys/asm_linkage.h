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

/* 2.6_leveraged #pragma ident	"@(#)asm_linkage.h	1.17	95/11/13 SMI" */

#ifndef _SYS_ASM_LINKAGE_H
#define	_SYS_ASM_LINKAGE_H

#pragma ident	"%Z%%M%	%I%	%E% CP"

#include <sys/stack.h>
#include <sys/trap.h>

#ifdef	__cplusplus
extern "C" {
#endif


#ifdef _ASM /* The remainder of this file is only for assembly files */

/*

#if !defined(__GNUC_AS__)

#error "Should be using GNU for PPC"

#endif
*/

/*
 * C pointers are different sizes between PPC32 and PPC64.
 * These constants can be used to compute offsets into pointer arrays.
 */
#if defined(__powerpc64)
#define	CLONGSHIFT	3
#define	CLONGSIZE	8
#define	CLONGMASK	7
#elif defined(__powerpc)
#define	CLONGSHIFT	2
#define	CLONGSIZE	4
#define	CLONGMASK	3
#endif

/*
 * Since we know we're either ILP32 or LP64 ..
 */
#define	CPTRSHIFT	CLONGSHIFT
#define	CPTRSIZE	CLONGSIZE
#define	CPTRMASK	CLONGMASK

#if CPTRSIZE != (1 << CPTRSHIFT) || CLONGSIZE != (1 << CLONGSHIFT)
#error	"inconsistent shift constants"
#endif

#if CPTRMASK != (CPTRSIZE - 1) || CLONGMASK != (CLONGSIZE - 1)
#error	"inconsistent mask constants"
#endif

#define	ASM_ENTRY_ALIGN	16

/*
 * Sign extend a 16-bit hex value to make it appropriate for use in a
 * lis instruction.
 *
 * For example,
 *	lis r1, 0xabcd
 * causes a warning, because the assembler cannot tell whether the 64-bit
 * value of 0xffffffffabcd0000 or 0x00000000abcd0000 is intended.
 * The former is possible, but the latter is not.  Using
 *	lis r1, 0xffffffffffffabcd
 * is awkward (and still not correct), so we use
 *	lis r1, EXT16(0xabcd)
 * instead.
 *
 * This is defined so as to work on 32-bit and 64-bit assemblers.
 */
#define	EXT16(x)	(((0 - (1 & ((x) >> 15))) & ~0xffff) | ((x) & 0xffff))

/*
 * Symbolic section definitions.
 * XXX left in from 2.6 
 */
#define	RODATA	".rodata"

/*
 * Macro to define weak symbol aliases. These are similar to the ANSI-C
 *      #pragma weak name = _name
 * except a compiler can determine type. The assembler must be told. Hence,
 * the second parameter must be the type of the symbol (i.e.: function,...)
 */

#define	ANSI_PRAGMA_WEAK(sym, stype)    \
	.weak	sym; \
	.type sym, @stype; \
/* CSTYLED */ \
sym	= _ ## sym

/*
 * Like ANSI_PRAGMA_WEAK(), but for unrelated names, as in:
 *	#pragma weak sym1 = sym2
 */
#define	ANSI_PRAGMA_WEAK2(sym1, sym2, stype)	\
	.weak	sym1; \
	.type sym1, @stype; \
sym1	= sym2


/*
 * A couple of macro definitions for creating/restoring
 * a minimal PowerPC stack frame.
 *
 */
#define	MINSTACK_SETUP \
	mflr	r0; \
	stwu	r1, -SA(MINFRAME)(r1); \
	stw	r0, (SA(MINFRAME)+4)(r1)

#define	MINSTACK_RESTORE \
	lwz	r0, +(SA(MINFRAME)+4)(r1); \
	addi	r1, r1, +SA(MINFRAME+4); \
	mtlr	r0

/*
 * A couple of macros for position-independent function references.
 */
#if defined(PIC)

#define	POTENTIAL_FAR_CALL(x)	bl	x ## @plt
#define	POTENTIAL_FAR_BRANCH(x)	b	x ## @plt

#else	/* !defined(PIC) */

#define	POTENTIAL_FAR_CALL(x)	bl	x
#define	POTENTIAL_FAR_BRANCH(x)	b	x

#endif	/* defined(PIC) */

/*
 * A couple of macros for position-independent data references.
 */

/*
 * This macro returns a pointer to the specified data item in the
 * desired register.  Returns pointer to requested data item in second
 * parameter, "regX".
 */
#if defined(PIC)

#define	POTENTIAL_NONLOCAL_DATA_REF(x, regX, GOTreg) \
	PIC_SETUP(); \
	mflr	GOTreg; \
	addis	regX, GOTreg, x@got@ha; \
	lwz	regX, x@got@l(regX)

#else	/* !defined(PIC) */

#define	POTENTIAL_NONLOCAL_DATA_REF(x, regX, GOTreg) \
	lis	regX, x@ha; \
	la	regX, x@l(regX)

#endif	/* defined(PIC) */

/*
 * profiling causes definitions of the MCOUNT and RTMCOUNT
 * particular to the type
 */
#ifdef GPROF

#error "Use DTRACE Instead"

#endif /* defined(GPROF) */

#ifdef PROF

#error "Use DTRACE Instead"

#endif /* defined(PROF) */

/*
 * if we are not profiling, MCOUNT should be defined to nothing
 */
#if !defined(PROF) && !defined(GPROF)
#define	MCOUNT(x)
#endif /* !defined(PROF) && !defined(GPROF) */

#define	RTMCOUNT(x)	MCOUNT(x)

/*
 * ENTRY provides the standard procedure entry code and an easy way to
 * insert the calls to mcount for profiling. ENTRY_NP is identical, but
 * never calls mcount.
 */
#define	ENTRY(x) \
	.text; \
	.global	x; \
	.align	2; \
	.type	x, @function; \
x:	MCOUNT(x)

#define	ENTRY_NP(x) \
	.text; \
	.global	x; \
	.align	2; \
	.type	x, @function; \
x:

#define	RTENTRY(x) \
	.text; \
	.global	x; \
	.align	2; \
	.type	x, @function; \
x:	RTMCOUNT(x)

/*
 * ENTRY2 is identical to ENTRY but provides two labels for the entry point.
 */
#define	ENTRY2(x, y) \
	.text; \
	.global	x, y; \
	.align	2; \
	.type	x, @function; \
	.type	y, @function; \
	/* CSTYLED */ \
x: ; \
y:	MCOUNT(x)

#define	ENTRY_NP2(x, y) \
	.text; \
	.global	x, y; \
	.align	2; \
	.type	x, @function; \
	.type	y, @function; \
	/* CSTYLED */ \
x: ; \
y:


/*
 * ALTENTRY provides for additional entry points.
 */
#define	ALTENTRY(x) \
	.global	x; \
	.type	x, @function; \
x:

/*
 * DGDEF and DGDEF2 provide global data declarations.
 */
#define	DGDEF2(name, sz) \
	.data; \
	.global name; \
	.type	name, @object; \
	.size	name, sz; \
name:

#define	DGDEF3(name, sz, algn) \
	.data; \
	.align	algn; \
	.globl	name; \
	.type	name, @object; \
	.size	name, sz; \
name:

#define	DGDEF(name)	DGDEF2(name, 4)

/*
 * SET_SIZE trails a function and set the size for the ELF symbol table.
 */
#define	SET_SIZE(x) \
	.size	x, (.-x)

#endif /* _ASM */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_ASM_LINKAGE_H */
