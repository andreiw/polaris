/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
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

/*
 * Copyright 2006 Cyril Plisko <cyril.plisko@mountall.com>
 * All rights reserved. Use is subject to license terms.
 */

#ifndef _SYS_MACHPARAM_H
#define	_SYS_MACHPARAM_H

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(_ASM)
#define	ADDRESS_C(c)	c ## ul
#else	/* _ASM */
#define	ADDRESS_C(c)	(c)
#endif	/* _ASM */

/*
 * PowerPC fundamental machine-dependent constants
 */
#define	NCPU		1	/* XXX Should be revised later */

#define	MAXNODES	4

/* supported page sizes */
#define	MMU_PAGE_SIZES  2

/*
 * MMU_PAGES* describes the physical page size used by the mapping hardware.
 * PAGES* describes the logical page size used by the system.
 */

#define	MMU_PAGESHIFT   12
#define	MMU_PAGESIZE	(1 << (MMU_PAGESHIFT))
#define	MMU_PAGEOFFSET	((MMU_PAGESIZE) - 1) /* Mask of address bits in page */
#define	MMU_PAGEMASK	(~(MMU_PAGEOFFSET))

/* All of the above, for logical */
#define	PAGESHIFT	12
#define	PAGESIZE	(1 << (PAGESHIFT))
#define	PAGEOFFSET	((PAGESIZE) - 1)
#define	PAGEMASK	(~(PAGEOFFSET))

/*
 * DEFAULT KERNEL THREAD stack size.
 */

#define	DEFAULTSTKSZ	(2 * PAGESIZE)

/*
 * DEFAULT initial thread stack size.
 */
#define	T0STKSZ		(2 * DEFAULTSTKSZ)

/* XXX - this value will be made to fit dynamically at a later point, right now this is a bogus number but needed for building  */

#define	PCIISA_VBASE	0xF3000000

/*
 * KERNELBASE is the virtual address at which the kernel segments start
 * in all contexts.
 * On 32-bit PowerPC systems, ABI specifies KERNELBASE is 0xE0000000
 */

#define	KERNELBASE	ADDRESS_C(0xE0000000)

/*
 * Define upper limit on user address space
 */
#define	USERLIMIT	KERNELBASE

#if defined(__powerpc64)

#error "PowerPC 64 is not supported yet"

#elif defined(__powerpc)


/*
 * Define upper limit on user address space
 */
#define	USERLIMIT	KERNELBASE
#define	USERLIMIT32	USERLIMIT

/*
 * SYSBASE is the virtual address which
 * the kernel allocated memory mapping starts in all contexts.
 */
#define	SYSBASE		ADDRESS_C(0xE7000000)
#define	SYSLIMIT	ADDRESS_C(0xEF000000)

#endif /* __powerpc */

#if !defined(_KADB) && !defined(_KMDB)
extern uintptr_t kernelbase, segkmap_start, segmapsize;
#endif

/*
 * ARGSBASE is the base virtual address of the range which
 * the kernel uses to map the arguments for exec
 */
#define	ARGSBASE	0xE5000000	/* XXX PPC WAG - need more thoughts */

/*
 * Reserve space for modules
 */
#define	MODTEXT_RESV (1024 * 1024 * 2)
#define	MODDATA_RESV (1024 * 300)

/*
 * heap has a chunk of HEAPTEXT_SIZE size allocated out of it for
 * the module text
 */
#define	HEAPTEXT_SIZE	(64 * 1024 * 1024)	/* 64 MB */


#ifdef __cplusplus
}
#endif

#endif /* _SYS_MACHPARAM_H */
