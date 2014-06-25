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
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 *
 * Copyright 2006 Sun Microsystems Laboratories.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * VM - Hardware Address Translation management.
 *
 * This file describes the contents of the PowerPC MMU (ppcmmu)
 * specific hat data structures and the ppcmmu specific hat procedures.
 * The machine independent interface is described in <vm/hat.h>.
 *
 * The definitions assume 32bit implementation of PowerPC.
 */

#ifndef	_VM_HAT_PPCMMU_H
#define	_VM_HAT_PPCMMU_H

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#include <sys/t_lock.h>
#include <sys/types.h>
#include <sys/mmu.h>
#include <sys/pte.h>
#include <sys/ppc_instr.h>
#include <vm/hat.h>
#include <vm/seg.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Mark values that ought to be obtained at configuration time,
 * either derived from cpu identification, hardware state, or
 * PROM properties, or something.  Just not magic numbers.
 */

#define	XXX_GET_FROM_CONFIGURATION(n) (n)

/*
 * Mark magic numbers for future reference.
 * These are different from XXX_GET_FROM_CONFIGURATION().
 * The notation needs to be reformed in some way to make
 * it unnecessary to use the magic number.
 */
#define	XXX_MAGIC(n) (n)

#define	VA_POISON ((caddr_t)(-1))

#define	LOG2P2(n) (31 - cntlzw(n))

/*
 * PPC MMU-specific flags for page_t
 */
#define	P_PNC	0x8		/* non-caching is permanent bit */
#define	P_TNC	0x10		/* non-caching is temporary bit */

#define	PP_GETRM(pp, rmmask)	((pp)->p_nrm & rmmask)

#define	PP_GENERIC_ATTR(pp)	((pp)->p_nrm & (P_MOD | P_REF | P_RO))
#define	PP_ISMOD(pp)		((pp)->p_nrm & P_MOD)
#define	PP_ISREF(pp)		((pp)->p_nrm & P_REF)
#define	PP_ISNC(pp)		((pp)->p_nrm & (P_PNC|P_TNC))
#define	PP_ISPNC(pp)		((pp)->p_nrm & P_PNC)
#define	PP_ISTNC(pp)		((pp)->p_nrm & P_TNC)
#define	PP_ISRO(pp)		((pp)->p_nrm & P_RO)

#define	PP_SETMOD(pp)		((pp)->p_nrm |= P_MOD)
#define	PP_SETREF(pp)		((pp)->p_nrm |= P_REF)
#define	PP_SETREFMOD(pp)	((pp)->p_nrm |= (P_REF|P_MOD))
#define	PP_SETPNC(pp)		((pp)->p_nrm |= P_PNC)
#define	PP_SETTNC(pp)		((pp)->p_nrm |= P_TNC)
#define	PP_SETRO(pp) 		((pp)->p_nrm |= P_RO)
#define	PP_SETREFRO(pp)		((pp)->p_nrm |= (P_REF|P_RO))

#define	PP_CLRMOD(pp)		((pp)->p_nrm &= ~P_MOD)
#define	PP_CLRREF(pp)		((pp)->p_nrm &= ~P_REF)
#define	PP_CLRREFMOD(pp)	((pp)->p_nrm &= ~(P_REF|P_MOD))
#define	PP_CLRPNC(pp)		((pp)->p_nrm &= ~P_PNC)
#define	PP_CLRTNC(pp)		((pp)->p_nrm &= ~P_TNC)
#define	PP_CLRRO(pp)		((pp)->p_nrm &= ~P_RO)

#define	HAT_OP_NULL	0
#define	HAT_OP_LOAD	1
#define	HAT_OP_UNLOCK	2
#define	HAT_OP_UNLOAD	3
#define	HAT_OP_CHGATTR	4
#define	HAT_OP_CHGPROT	5
#define	HAT_OP_SYNC	6

/*
 * Some MMU-specific constants.
 */
#define	H_PRIMARY	0	/* H bit value for primary PTEG entry */
#define	H_SECONDARY	1	/* H bit value for secondary PTEG entry */

/*
 *
 * Documentation
 * -------------
 * HASH_VSIDMASK, HASH_PAGEINDEX_MASK:
 *	MPCFPE32B, page 7-41,7-42, Page Table Hashing Functions
 *
 * PTEGSIZE, PTEOFFSET, PTESHIFT:
 *	MPCFPE32B, page 7-38, 7-39, section 7.7.1, Page Table Definition
 *
 * APISHIFT, APIMASK:
 *	MPCFPE32B, page 7-44,
 *	Figure 7-22, Generation of Addresses for Page Tables
 *
 */

#define	HASH_VSIDMASK		0x7FFFF		/* == (1 << 19) - 1 */
#define	HASH_PAGEINDEX_MASK	0xFFFF

#define	PTEGSIZE	(sizeof (pte_group_pair_t))	/* PTEG size in bytes */
#define	PTEOFFSET	(PTEGSIZE - 1)	/* mask for PTE offset within PTEG */
#define	PTESHIFT	3		/* PTE index within the PTEG */

#define	APISHIFT	22		/* API field in logical address */
#define	APIMASK		0x3F		/* mask for API value, 6 bits */

/*
 * Flags for hme_impl field in hme structure.
 */
#define	HME_BUSY	1	/* hme is being modified elsewhere */
#define	HME_STOLEN	2	/* hme was stolen last time in the group */

/*
 * Flags to pass to ppcmmu_pteunload().
 */
#define	PPCMMU_NOSYNC	0x00
#define	PPCMMU_RMSYNC	0x01
#define	PPCMMU_INVSYNC	0x02
#define	PPCMMU_RMSTAT	0x04
#define	PPCMMU_NOFLUSH	0x08

/*
 * Flags to pass to hat_pagecachectl
 */
#define	HAT_CACHE	0x0
#define	HAT_UNCACHE	0x1
#define	HAT_TMPNC	0x2

#define	VSIDRANGE_INVALID	0		/* invalid VSID range */
#define	MAX_VSIDRANGES		0x100000	/* max vsid ranges available 2^20 */
#define	VSIDRANGE_SHIFT		4		/* Each range covers 16 VSIDs */
/*
 * VSID range id used for kernel address space.
 */
#define	KERNEL_VSID_RANGE	1	/* VSIDs 16-31 */


/* Default number of VSID ranges used */
#ifdef HAT_DEBUG
#define	DEFAULT_VSIDRANGES	0x400
#else
#define	DEFAULT_VSIDRANGES	(MAX_VSIDRANGES/2)
#endif

/* Definitions for lock count field(s) in ptegp_lockmap word */
#ifdef HAT_DEBUG
#define	LKCNT_HASHED		1	/* pte lock count is in hash table */
#else
#define	LKCNT_HASHED		3	/* pte lock count is in hash table */
#endif
#define	LKCNT_UPPER_SHIFT	16	/* for upper PTEG in ptegp_lockmap */
#define	LKCNT_LOWER_SHIFT	0	/* for lower PTEG in ptegp_lockmap */
#define	LKCNT_MASK		0x3	/* lock count mask */
#define	LKCNT_SHIFT		2	/* shift for lock count bits */
#define	LKCNT_NBITS		2	/* no. of bits for lock count field */

/*
 * Flags to control some operations on BAT registers
 */
#define	BATS_INSTR 0x1		/* IBATs only */
#define	BATS_DATA  0x2		/* DBATs only */
#define	BATS_ALL   0x3		/* Both IBATs and DBATs */
#define	BATS_SUPPORTED_FLAGS 0x3

#if !defined(_ASM)


typedef struct as as_t;

struct hat {
	uint_t	hat_secret;	/* Something harmless */
};

extern uint_t hat_boot_probe(uintptr_t *, size_t *, pfn_t *, uint_t *);
extern void *nucleus_alloc(size_t, int, char *, char *);
extern void *nucleus_zalloc(size_t, int, char *, char *);
extern void ppcmmu_getpte(caddr_t, pte_t *);


#if defined(HAT_PPCMMU_IMPL) || defined(MACH_PPCMMU_IMPL)

#define	IS_KERNEL_HAT(hat) \
	((hat) == &khat)

#define	HAT_AS_LOCK_HELD(hat) \
	AS_LOCK_HELD((hat)->hat_as, &(hat)->hat_as->a_lock)

#define	USER_HAT_AS_LOCK_HELD(hat) \
	(IS_KERNEL_HAT(hat) || HAT_AS_LOCK_HELD(hat))

/*
 * The HME, Hat Mapping Entry.
 * The ppcmmu-dependent translation on the mapping list for a page
 */

struct hme;
typedef struct hme hme_t;

struct hme {
	// XXX next and prev might be better at a different offset
	// XXX because other components are accessed more frequently.
	// XXX Then again, we might be able to greatly reduce navigation
	// XXX overhead, with something like PCEs or clustering.
	hme_t *hme_next;	/* next hme */
	hme_t *hme_prev;	/* prev hme */

#if defined(HAT_DEBUG)
	// XXX This ought not to be needed.
	// XXX All code has been converted to use
	// XXX pp = ptep_to_pp(pte), instead of hme->hme_page.
	// XXX hme_page is now retained only for debugging,
	// XXX mostly ASSERT(pp == hme->hme_page).
	page_t *hme_page;		/* what page this maps */
#endif

	// XXX Dynamically allocate hats.
	// XXX Use another method to navigate from hme to hat.
	uint_t	hme_hat : 16;		/* index into hats */

	uint_t	hme_impl : 8;		/* implementation hint */
	uint_t	hme_notused : 2;	/* extra bits */
	uint_t	hme_prot : 2;		/* protection */
	uint_t	hme_noconsist : 1;	/* mapping can't be kept consistent */
	uint_t	hme_ncpref: 1;		/* consistency resolution preference */
	uint_t	hme_nosync : 1;		/* ghost unload flag */
	uint_t	hme_valid : 1;		/* hme loaded flag */
};


/*
 * Per PTE-Group-Pair (PTEGP) structure. Since the mapping/unmapping is done
 * within a PTEG pair this structure contains the information about the ptes
 * within the PTEG pair.
 *
 *	ptegp_mutex
 *		Changing mappings in the PTEG pair requires this lock. And
 *		it protects all the fields in this structure.
 *
 *	ptegp_validmap
 *		Each bit indicates if the corresponding PTE is valid.
 *		Note:	LSB corresponds to the lowest numbered PTE in the PTEG
 *			pair.
 *
 *	ptegp_lockmap
 *		Two bits per PTE are used to indicate lock count and when the
 *		lock count exceeds 2 then the count for that pte is maintained
 *		in a hash table. The values for each lock field is:
 *			0 	lockcnt=0
 *			1	lockcnt=1
 *			2	lockcnt=2
 *			3	lockcnt=get_ptelock(pte)
 *		Note:	The LSB field corresponds to the lowest numbered PTE
 *			in the PTEG pair.
 *
 */

typedef struct ptegp {
	kmutex_t	ptegp_mutex;
	ushort_t	ptegp_validmap;
	ushort_t	ptegp_unused;		/* unused */
	uint_t		ptegp_lockmap;
} ptegp_t;

/*
 * Software structure definition for BAT information.
 */
struct batinfo {
	ushort_t	bat_type;	/* Bat type, IBAT_T text, DBAT_T data */
	ushort_t	bat_valid;	/* Valid flag 		*/
	caddr_t		bat_vaddr;	/* virtual address	*/
	caddr_t		bat_paddr;	/* physical address	*/
	size_t		bat_size;	/* block size in bytes	*/
	uint_t		bat_vprot;	/* virtual protection */
};

typedef struct batinfo batinfo_t;

struct bat_table {
	batinfo_t	*bt_array;
	uint_t		bt_type;
	uint_t		bt_size;
	char		*bt_desc;
};

typedef struct bat_table bat_table_t;

/* Low-level functions */
extern uint_t	mmu_delete_pte(pte_t *, caddr_t);
extern uint_t	mmu_pte_reset_rmbits(pte_t *, caddr_t);
extern void	mmu_update_pte_prot(pte_t *, uint_t, caddr_t);
extern void	mmu_update_pte_wimg(pte_t *, uint_t, caddr_t);
extern void	mmu_update_pte(pte_t *, pte_t *, uint_t, caddr_t);
extern void	mmu_segload(uint_t);
extern void	mmu_cache_flushpage(caddr_t);

ptegp_t *pte_to_ptegp(pte_t *p);
void ptelock(ptegp_t *, pte_t *);

/*
 * High-order 4 bits select one of 16 segment registers.
 * See MPCFPE32B, page 7-28, Figure 7-13, Page Address Translation Overview.
 */
static __inline__ uint_t
va_to_sr(caddr_t vaddr)
{
	return ((uintptr_t)vaddr >> 28);
}

/*
 * A fully-qualified virtual address is a {VSID, va} pair.
 * Compute the primary hash functions of a given
 * fully-qualified virtual address (fqva).
 *
 */

static __inline__ uint_t
fqva_to_hash1(uint_t vsid, caddr_t vaddr)
{
	uintptr_t vpn = (uintptr_t)vaddr >> MMU_PAGESHIFT;

	return ((vsid & HASH_VSIDMASK) ^ (vpn & HASH_PAGEINDEX_MASK));
}

/*
 * hash_get_primary(h)
 *	Given the primary hash function value,
 *	compute the address of the primary PTEG.
 */
#define	hash_get_primary(h) \
	(pte_t *)((((uint_t)(h) & hash_pteg_mask) << 6) + (uint_t)ptes)
/*
 * hash_get_secondary(h)
 *	Given the primary hash function value compute the address of the
 *	secondary PTEG.
 */
#define	hash_get_secondary(pri_hash) \
	(pte_t *)((hash_pteg_mask << 6) ^ (uint_t)pri_hash)

/*
 * This data is used in machine-specific places.
 */
extern	hme_t *hments, *ehments;
extern	pte_t *ptes;		/* ptes	Page Table start */
extern	pte_t *eptes;		/* Page Table end */
extern	pte_t *mptes;		/* upper half of Page Table */
extern	ptegp_t *ptegps;	/* Per-PTEG-pair structures */
extern	ptegp_t *eptegps;	/* end of Per-PTEG-pair structures */
extern	uint_t nptegp;		/* number of PTEG-pairs */
extern	uint_t nptegmask;	/* mask for the PTEG number */
extern	uint_t hash_pteg_mask;	/* mask used for computing PTEG offset */
extern	int dcache_blocksize;
extern	int cache_blockmask;
extern	batinfo_t bats[];	/* Bat information table */

static __inline__ uint_t
pte_in_pagetable(pte_t *pte)
{
	return (pte >= ptes && pte < eptes);
}

static __inline__ uint_t
hme_in_hmetable(hme_t *hme)
{
	return (hme >= hments && hme < ehments);
}

static __inline__ pte_t *
hme_to_pte(hme_t *hme)
{
	ASSERT(hme_in_hmetable(hme));
	return (&ptes[hme - hments]);
}

static __inline__ hme_t *
pte_to_hme(pte_t *pte)
{
	ASSERT(pte_in_pagetable(pte));
	return (&hments[pte - ptes]);
}

#endif /* defined(HAT_PPCMMU_IMPL) || defined(MACH_PPCMMU_IMPL) */

#if defined(HAT_PPCMMU_IMPL)

typedef struct hat xhat_t;	/* Public  hat (eXternal) */
typedef struct ppcmmu hat_t;	/* Private hat - implementation only */

struct ppcmmu {
	hat_t		*hat_next;	/* for address space list */
	as_t		*hat_as;	/* as this hat provides mapping for */
	uint_t		hat_vsidr;	/* VSID range */
	uint_t		hat_freeing;
#if defined(HAT_DEBUG)
	uint_t		hat_classtag;
	uint_t		hat_seq;
#endif
	kmutex_t	hat_mutex;	/* protects hat, hatops */
	uint32_t	hat_rss;	/* approx # of pages used by as */
	short		hat_rmstat;	/* turn on/off getting statistics */
};

/*
 *
 * hat_vsidr:
 *	Specifies the start of vsid range for this address space.
 *	The value represents the top 20 bits
 *	of the (24 bit) VSID field in segment register.
 *	It is -1 if no valid VSID range is assigned to this hat.
 *
 */


/*
 * stats for ppcmmu
 */
struct vmhatstat {
	kstat_named_t	vh_pteoverflow;		/* pte overflow count */
	kstat_named_t	vh_pteload;		/* calls to ppcmmu_pteload */
	kstat_named_t	vh_mlist_enter;		/* calls to mlist_lock_enter */
	kstat_named_t	vh_mlist_enter_wait;	/* had to do a cv_wait */
	kstat_named_t	vh_mlist_exit;		/* calls to mlist_lock_exit */
	kstat_named_t	vh_mlist_exit_broadcast; /* had to do cv_broadcast */
	kstat_named_t	vh_vsid_gc_wakeups;	/* wakeups to ppcmmu_vsid_gc */
	kstat_named_t	vh_hash_ptelock;	/* #of pte lock counts hashed */
};

#if defined(HAT_DEBUG)

#define	VMHATSTAT_INCR(elem) (++(vmhatstat.elem.value.ul))
#define	MLIST_ENTER_STAT() VMHATSTAT_INCR(vh_mlist_enter)
#define	MLIST_WAIT_STAT() VMHATSTAT_INCR(vh_mlist_enter_wait)
#define	MLIST_EXIT_STAT() VMHATSTAT_INCR(vh_mlist_exit)
#define	MLIST_BROADCAST_STAT() VMHATSTAT_INCR(vh_mlist_exit_broadcast)

#else

#define	VMHATSTAT_INCR(elem)
#define	MLIST_ENTER_STAT()
#define	MLIST_WAIT_STAT()
#define	MLIST_EXIT_STAT()
#define	MLIST_BROADCAST_STAT()

#endif /* defined(HAT_DEBUG) */

/*
 * structure for vsid range allocation set.
 */
struct vsid_alloc_set {
	int	vs_nvsid_ranges;		/* no. of free vsid ranges */
	int	vs_vsid_range_id;		/* vsid range id */
	struct vsid_alloc_set	*vs_next;	/* pointer to the next set */
};

/*
 * Macros
 */
#define	vsid_valid(range) ((range) != VSIDRANGE_INVALID)

/* Lockflag to ppcmmu_ptefind() */
#define	PTEGP_LOCK	1	/* ptegp_mutex needed */
#define	PTEGP_NOLOCK	0	/* ptegp_mutex not needed */

extern int pf_is_memory(pfn_t pf);

#define	MMU_INVALID_PTE	((u_longlong_t)0)

#if defined(HAT_DEBUG)

#define	IFDEBUG(stmt) stmt
#define	DPRINTF prom_printf
#define	HERE prom_printf("\n===> %s, line %u\n", __FILE__, __LINE__)
#define	HERE_FN(func) prom_printf("\n===> %s, line %u, %s\n", __FILE__, __LINE__, func)

/*
 * Some useful tracing macros
 */

#define	HATIN(r, h, a, l) \
	if (hattrace) prom_printf("->%s hat=%p, adr=%p, len=%lx\n", #r, h, a, l)
#define	HATOUT(r, h, a) \
	if (hattrace) prom_printf("<-%s hat=%p, adr=%p\n", #r, h, a)


/*
 * Various objects allocated by the hat layer can contain
 * an extra class tag, which is just an arbitrary 32-bit
 * value embedded in the data structure, and can be tested
 * for correctness.  It serves no other purpose.
 * The tag does not exist in non-debug kernels.
 * Note: this means that the size of data structures will
 * definitely change between debug and non-debug builds.
 */

#define	SET_CLASS_TAG(dst, val) (dst = val)
#define	CLASS_HAT 0xF00F

#else /* !defined(HAT_DEBUG) */

#define	IFDEBUG(stmt)

/*
 * DPRINTF is valid only for HAT_DEBUG builds.
 * When HAT_DEBUG is not defined, resolve DPRINTF in a way
 * that will result in an obvious error.
 */
#define	DPRINTF no_debug_printf
#define	HERE

#define	HATIN(r, h, a, l)
#define	HATOUT(r, h, a)

#define	SET_CLASS_TAG(dst, val)

#endif /* defined(HAT_DEBUG) */

/*
 * These routines are all MMU-specific interfaces to the ppcmmu routines.
 * These routines are called from machine specific places to do the
 * dirty work for things like boot initialization, mapin/mapout and
 * first level fault handling.
 */

ptegp_t *hme_to_ptegp(hme_t *p);

static __inline__ uintptr_t
align2page(uintptr_t vaddr)
{
	return (vaddr & MMU_PAGEMASK);
}

static __inline__ uintptr_t
page_phase(uintptr_t vaddr)
{
	return (vaddr & MMU_PAGEOFFSET);
}

static __inline__ uint_t
is_page_aligned(uintptr_t vaddr)
{
	return (page_phase(vaddr) == 0);
}

#endif /* defined(HAT_PPCMMU_IMPL) */

#endif /* !defined(_ASM) */

#ifdef	__cplusplus
}
#endif

#endif	/* _VM_HAT_PPCMMU_H */
