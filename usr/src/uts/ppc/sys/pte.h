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
 * Copyright (c) 2006 by Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef _SYS_PTE_H
#define	_SYS_PTE_H

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#ifndef _ASM
#include <sys/types.h>
#include <sys/pte0.h>
#include <sys/pte1.h>
#include <vm/page.h>	/* Import page_numtopp_nolock() */
#endif /* _ASM */

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Definitions for the PowerPC MMU.
 *
 * The definitions are valid only for 32-bit implementations
 * of PowerPC architecture.
 */

#if !defined(_ASM) && defined(_KERNEL)

typedef struct pte {
	uint_t ptew[2];
} pte_t;

// typedef pte_t hwpte_t;

/*
 * PTEWORD0 - word number of the first word in the PTE that contains VSID.
 * PTEWORD1 - word number of the second word in the PTE that contains PPN.
 *
 * Byte offsets to access byte or word within PTE structure.
 * PTE structure: (BIG ENDIAN format used as reference)
 *              0-------1-------2-------3-------
 *              v.........vsid...........h...api
 *              4-------5-------6-------7-------
 *              ppn.............000.R.C.WIMG.0.PP
 */
#ifdef _BIG_ENDIAN
#define	PTEWORD0	0
#define	PTEWORD1	1
#else
/* On Little-Endian the words and bytes are reversed */
#define	PTEWORD0	1
#define	PTEWORD1	0
#endif

/*
 * Macros.
 */
/* WIMG bits; c=1 cached, c=0 uncached, g (guarded bit - 0 or 1) */
#define	WIMG(c, g) ((c) ? 2 : (6 | (g)))
#define	VADDR_TO_API(a)	(((uintptr_t)(a) >> 22) & 0x3F)
#define	VSID(range, a)	(((uint_t)(range) << 4) | ((uint_t)(a) >> 28))

/* PTE bit field values */
#define	PTE_VALID	1
#define	PTE_INVALID	0

/* PTE WIMG value bit masks */
#define	WIMG_WRITE_THRU		0x8
#define	WIMG_CACHE_DIS		0x4
#define	WIMG_MEM_COHERENT	0x2
#define	WIMG_GUARDED		0x1

// XXX #define	MMU_INVALID_PTE		((u_longlong_t)0)
// XXX #define	MMU_INVALID_SPTE	((uint_t)0)

static __inline__ uint_t
ptep_get_v(pte_t *ptep)
{
	return (PTE0_GET_V(ptep->ptew[PTEWORD0]));
}

static __inline__ uint_t
ptep_get_vsid(pte_t *ptep)
{
	return (PTE0_GET_VSID(ptep->ptew[PTEWORD0]));
}

static __inline__ uint_t
ptep_get_h(pte_t *ptep)
{
	return (PTE0_GET_H(ptep->ptew[PTEWORD0]));
}

static __inline__ uint_t
ptep_get_api(pte_t *ptep)
{
	return (PTE0_GET_API(ptep->ptew[PTEWORD0]));
}

static __inline__ uint_t
ptep_get_rpn(pte_t *ptep)
{
	return (PTE1_GET_RPN(ptep->ptew[PTEWORD1]));
}

static __inline__ uint_t
ptep_get_xpn(pte_t *ptep)
{
	return (PTE1_GET_XPN(ptep->ptew[PTEWORD1]));
}

static __inline__ uint_t
ptep_get_r(pte_t *ptep)
{
	return (PTE1_GET_R(ptep->ptew[PTEWORD1]));
}

static __inline__ uint_t
ptep_get_c(pte_t *ptep)
{
	return (PTE1_GET_C(ptep->ptew[PTEWORD1]));
}

static __inline__ uint_t
ptep_get_wimg(pte_t *ptep)
{
	return (PTE1_GET_WIMG(ptep->ptew[PTEWORD1]));
}

static __inline__ uint_t
ptep_get_x(pte_t *ptep)
{
	return (PTE1_GET_X(ptep->ptew[PTEWORD1]));
}

static __inline__ uint_t
ptep_get_pp(pte_t *ptep)
{
	return (PTE1_GET_PP(ptep->ptew[PTEWORD1]));
}


static __inline__ void
ptep_set_v(pte_t *ptep, uint_t fld)
{
	ptep->ptew[PTEWORD0] = PTE0_SET_V(ptep->ptew[PTEWORD0], fld);
}

static __inline__ void
ptep_set_vsid(pte_t *ptep, uint_t fld)
{
	ptep->ptew[PTEWORD0] = PTE0_SET_VSID(ptep->ptew[PTEWORD0], fld);
}

static __inline__ void
ptep_set_h(pte_t *ptep, uint_t fld)
{
	ptep->ptew[PTEWORD0] = PTE0_SET_H(ptep->ptew[PTEWORD0], fld);
}

static __inline__ void
ptep_set_api(pte_t *ptep, uint_t fld)
{
	ptep->ptew[PTEWORD0] = PTE0_SET_API(ptep->ptew[PTEWORD0], fld);
}

static __inline__ void
ptep_set_rpn(pte_t *ptep, uint_t fld)
{
	ptep->ptew[PTEWORD1] = PTE1_SET_RPN(ptep->ptew[PTEWORD1], fld);
}

static __inline__ void
ptep_set_xpn(pte_t *ptep, uint_t fld)
{
	ptep->ptew[PTEWORD1] = PTE1_SET_XPN(ptep->ptew[PTEWORD1], fld);
}

static __inline__ void
ptep_set_r(pte_t *ptep, uint_t fld)
{
	ptep->ptew[PTEWORD1] = PTE1_SET_R(ptep->ptew[PTEWORD1], fld);
}

static __inline__ void
ptep_set_c(pte_t *ptep, uint_t fld)
{
	ptep->ptew[PTEWORD1] = PTE1_SET_C(ptep->ptew[PTEWORD1], fld);
}

static __inline__ void
ptep_set_wimg(pte_t *ptep, uint_t fld)
{
	ptep->ptew[PTEWORD1] = PTE1_SET_WIMG(ptep->ptew[PTEWORD1], fld);
}

static __inline__ void
ptep_set_x(pte_t *ptep, uint_t fld)
{
	ptep->ptew[PTEWORD1] = PTE1_SET_X(ptep->ptew[PTEWORD1], fld);
}

static __inline__ void
ptep_set_pp(pte_t *ptep, uint_t fld)
{
	ptep->ptew[PTEWORD1] = PTE1_SET_PP(ptep->ptew[PTEWORD1], fld);
}

static __inline__ uint_t
ptep_isvalid(pte_t *ptep)
{
	return ((ptep->ptew[PTEWORD0] & PTE0_V_FLD_MASK) != 0);
}

static __inline__ uint_t
pte1_get_rm(uint_t pte1)
{
#if (PTE1_R_SHIFT == PTE1_C_SHIFT + 1)
	return ((pte1 >> PTE1_C_SHIFT) & 0x3);
#else
	return ((PTE1_GET_R(pte1) << 1) | PTE1_GET_C(pte1));
#endif
}

#endif /* !_ASM  && _KERNEL */


/*
 * Definitions for Access Permissions (page or block).
 * The values represent the pp bits in PTE,
 * assuming that the key values of Ks and Ku are same as MSR[PR] bit
 * (i.e Ku=1 and Ks=0).
 *
 * Note:
 *	On PPC, kernel pages can not be write protected
 *	without giving user read access.
 *
 * Names are taken from SPARC Reference MMU code,
 * but considering execute and read permissions equivalent.
 */

#define	MMU_STD_SRWX		0
#define	MMU_STD_SRWXURX		1
#define	MMU_STD_SRWXURWX	2
#define	MMU_STD_SRXURX		3

#define	MMU_STD_PAGESHIFT	12
#define	MMU_STD_PAGESIZE	(1 << MMU_STD_PAGESHIFT)
#define	MMU_STD_PAGEMASK	(~((MMU_STD_PAGESIZE) - 1)

#ifndef _ASM

/*
 * Macros/functions for reading portions of a PTE.
 */

/*
 * Mask to compare the complete pfn between 2 PTEs.
 * The bits that make up the pfn are scrambled, but just for comparison
 * we do not care, and so we need not do the work of constructing
 * a proper pfn.
 */

#define	PTE1_PFN_FLD_MASK \
	(PTE1_RPN_FLD_MASK | PTE1_XPN_FLD_MASK | PTE1_X_FLD_MASK)

#if defined(PTE_EXTENDED_ADDRESSING)

static __inline__ pfn_t
pte1_to_pfn(uint_t pte1)
{
	uint_t rpn;

	rpn = pte1_get_rpn(pte1);

	if (HID0_GET_XAEN(hid0)) {
		uint_t xpn = pte1_get_xpn(pte1);
		uint_t x = pte1_get_x(pte1);

		return ((xpn << 21) | (x << 20) | rpn);
	}

	return (rpn);
}

#else

static __inline__ pfn_t
pte1_to_pfn(uint_t pte1)
{
	return (PTE1_GET_RPN(pte1));
}

#endif /* defined(PTE_EXTENDED_ADDRESSING) */

static __inline__ pfn_t
ptep_to_pfn(pte_t *pte)
{
	if (ptep_isvalid(pte))
		return (pte1_to_pfn(pte->ptew[PTEWORD1]));
	else
		return (PFN_INVALID);
}

static __inline__ page_t *
ptep_to_pp(pte_t *pte)
{
	pfn_t pfn;

	pfn = ptep_to_pfn(pte);
	if (pfn == PFN_INVALID)
		return (NULL);
	return (page_numtopp_nolock(pfn));
}


#define	NPTEPERPTEG	8			/* PTEs in PTEG */
#define	NPTEPERPTEGP	(2 * NPTEPERPTEG)	/* PTEs in PTEG pair */

/*
 * A hardware PTE group contains 8 PTEs
 */
typedef pte_t pte_group_t[NPTEPERPTEG];
typedef pte_group_t pte_group_pair_t[2];

#endif /* _ASM */

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_PTE_H */
