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

#pragma ident	"%Z%%M%	%I%	%E% SMI"

/*
 * VM - Hardware Address Translation (HAT) management for ppc.
 *
 * This file implements the machine specific hardware translation
 * needed by the VM system.  The machine independent interface is
 * described in <usr/src/uts/common/vm/hat.h>, while the machine
 * dependent interface and data structures are described in
 * <usr/src/uts/ppc/vm/hat_ppcmmu.h>.
 *
 * Nearly all the details of how the hardware is managed should not be
 * visible outside this layer except for miscellaneous machine specific
 * functions (e.g. mapin/mapout) that work in conjunction with this code.
 *
 * Other than a small number of machine specific places, the hat data
 * structures seen by the higher levels in the VM system are opaque
 * and are only operated on by the hat routines.  Each address space
 * contains a pointer to hat_t and a page contains an opaque
 * pointer which is used by the HAT code to hold a list of active
 * translations to that page.
 *
 * Routines used only inside of ppc/vm start with hati_ for HAT Internal.
 *
 * Summary of this implmentation:
 *     32-bit only; no support for 64-bit PowerPC models
 *     No support for Intimate Shared Memory (ISM)
 *
 */

#define	HAT_PPCMMU_IMPL
#define	HAT_DEBUG defined(DEBUG)

#include <vm/lint.h>

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/mman.h>
#include <sys/sysmacros.h>
#include <sys/machparam.h>
#include <sys/machsystm.h>
#include <sys/vtrace.h>
#include <sys/kmem.h>
#include <sys/pte.h>
#include <sys/mmu.h>
#include <sys/cmn_err.h>
#include <sys/cpu.h>
#include <sys/cpuvar.h>
#include <sys/var.h>
#include <sys/debug.h>
#include <sys/archsystm.h>
#include <sys/ppc_instr.h>
#include <sys/spr.h>
#include <sys/batu.h>
#include <sys/batl.h>
#include <sys/cpu_feature.h>
#include <sys/hid0.h>
#include <vm/hat.h>
#include <vm/hat_ppcmmu.h>
#include <vm/as.h>
#include <vm/seg.h>
#include <vm/page.h>
#include <vm/seg_kp.h>
#include <vm/seg_kpm.h>
#include <vm/rm.h>
#include <vm/mach_page.h>

/*
 * Public functions and data referenced by this hat.
 */
extern struct seg *segkmap;

extern uint_t cpu_features;
extern uint_t hid0;
extern uint_t hid1;

/*
 * Semi-private data
 */
pte_t		*ptes, *eptes;		/* the PTE array */
pte_t		*mptes;			/* the upper half of PTE array */
hme_t		*hments, *ehments;	/* the array of HMEs */
ptegp_t		*ptegps, *eptegps;	/* PTEGPs array */
uint_t		nptegp;			/* number of PTEG-pairs */
uint_t		nptegmask;		/* mask for the PTEG number */
uint_t		hash_pteg_mask;		/* mask used in computing PTEG offset */
int		cache_blockmask;

static hat_t khat;

/*
 * Private data.
 */

/*
 * PTELOCK counts: Since a PTE can be locked multiple times we need to
 * maintain a lock count for each PTE.  On PPC, the PTEs of the same address
 * space are not grouped like in other architectures (e.g x86, srmmu) so we
 * need a seperate counter for each PTE.  The occurence of multiple locks
 * on a PTE is not common, and so we are using a two level scheme to minimize
 * memory for the ptelock counters.  We use 2 bits per PTE in the ptegp
 * structure which keeps a lock count of up to 2 and an indication
 * if it exceeds 2.  For lock counts greater than 2 we use a small hash table
 * to maintain the lock count for those PTEs.
 */

/* Structure of hash table entry for keeping PTE lock count values */
typedef struct ptelock_hash {
	uintptr_t	tag;		/* address of hw pte */
	uint_t		lockcnt;	/* lock count for this pte */
} ptelock_hash_t;

ptelock_hash_t *ptelock_hashtab; /* hash table for PTE lock counts */
int ptelock_hashtab_size; /* # of entries in ptelock_hashtab[] */
lock_t ptelock_hash_lock; /* lock to protect ptelock_hashtab[] */
uint_t ptelock_hash_mask;
#if defined(HAT_DEBUG)
int ptetohash_ratio = 2; /* ratio of 1:2 PTEs for ptelock hash table */
#else
int ptetohash_ratio = 64; /* ratio of 1:64 PTEs for ptelock hash table */
#endif /* defined(HAT_DEBUG) */
static int ptelock_hash_ops(pte_t *, int, int);

/* Values for flags argument in ptelock_hash_ops() */
#define	HASH_CREATE	1	/* Create a hash entry for the PTE */
#define	HASH_DELETE	2	/* Delete the hash entry for the PTE */
#define	HASH_CHANGE	4	/* Change to the lock count of the PTE */

#define	create_ptelock(pte, cnt) 	ptelock_hash_ops(pte, cnt, HASH_CREATE)
#define	get_ptelock(pte)		ptelock_hash_ops(pte, 0, HASH_CHANGE)
#define	change_ptelock(pte, incr)	ptelock_hash_ops(pte, incr, HASH_CHANGE)
#define	delete_ptelock(pte)		ptelock_hash_ops(pte, 0, HASH_DELETE)

/*
 * stats for ppcmmu
 *	vh_ptegoverflow
 *		Number of PTEG overflows.
 *	vh_vsid_gc_wakeups
 *		How many times the vsid_gc thread is invoked.
 */
struct vmhatstat vmhatstat = {
	{ "vh_ptegoverflow",		KSTAT_DATA_ULONG },
	{ "vh_pteload",			KSTAT_DATA_ULONG },
	{ "vh_mlist_enter",		KSTAT_DATA_ULONG },
	{ "vh_mlist_enter_wait",	KSTAT_DATA_ULONG },
	{ "vh_mlist_exit",		KSTAT_DATA_ULONG },
	{ "vh_mlist_exit_broadcast",	KSTAT_DATA_ULONG },
	{ "vh_vsid_gc_wakeups",		KSTAT_DATA_ULONG },
	{ "vh_hash_ptelock",		KSTAT_DATA_ULONG },
};

/*
 * kstat data
 */
kstat_named_t *vmhatstat_ptr = (kstat_named_t *)&vmhatstat;
ulong_t vmhatstat_ndata = sizeof (vmhatstat) / sizeof (kstat_named_t);

/*
 * Global data
 */

/* Default number of vsid ranges to be used */
uint_t ppcmmu_max_vsidranges = DEFAULT_VSIDRANGES;

/*
 * vsid_bitmap[]
 *	Each bit specifies if the corresponding vsid-range-id is in use.
 *	ppcmmu_alloc() sets the bit in the bit map and ppcmmu_free()
 *	resets it.
 */
uint_t *vsid_bitmap;	/* bitmap for active vsid-range-ids */
struct vsid_alloc_set *vsid_alloc_head; /* head of the free list */

#define	NBINT	(sizeof (uint_t) * NBBY)	/* no. of bits per uint_t */
#define	SET_VSID_RANGE_INUSE(id) \
	(vsid_bitmap[(id) / NBINT] |= (1 << ((id) % NBINT)))
#define	CLEAR_VSID_RANGE_INUSE(id) \
	(vsid_bitmap[(id) / NBINT] &= ~(1 << ((id) % NBINT)))
#define	IS_VSID_RANGE_INUSE(id) \
	(vsid_bitmap[(id) / NBINT] & (1 << ((id) % NBINT)))

/*
 * External data
 */
extern as_t kas;			/* kernel's address space */

/*
 * PPCMMU has several levels of locking.  The locks must be acquired
 * in the correct order to prevent deadlocks.  The locks required to
 * change/add mappings in a PTEG should be acquired in the order:
 *	mlist_lock (if there is a page structure associated with the page).
 *	hat_mutex
 *	ptegp_mutex (per PTEG pair mutex).
 *
 * Page mapping lists are locked with the per-page inuse bit, which
 * is manipulated by ppcmmu_mlist_enter() and ppcmmu_mlist_exit().
 * This bit must be held to look at or change a page's p_mapping list.
 *
 * The ppcmmu_page_lock array of mutexes protect all pages' p_nrm fields.
 * They also protect the inuse and wanted bits.
 * To make life more interesting, the p_nrm bits may be played with without
 * holding the appropriate ppcmmu_page_lock if and only if the particular
 * page is not mapped.  This is typically done in page_create() after a
 * call to page_hashout().
 *
 * The global vsid-range-id resources are mutexed by ppcmmu_res_lock.
 * Any lock can be held when locking ppcmmu_res_lock.
 *
 * The per PTEGP structure mutex protects all the mappings
 * in the group pair and all the fields in the ptegp structure.
 * Any lock can be held when locking this mutex.
 */
#define	SPL_SHIFT	7
#define	SPL_TABLE_SIZE	(1 << SPL_SHIFT)	/* Must be a power of 2 */
#define	SPL_MASK	(SPL_TABLE_SIZE - 1)
#define	SPL_INDEX(pp)	((((uintptr_t)(pp)) >> SPL_SHIFT) & SPL_MASK)
#define	SPL_HASH(pp)	&ppcmmu_page_lock[SPL_INDEX(pp)]

static kmutex_t	ppcmmu_page_lock[SPL_TABLE_SIZE];

#define	MML_SHIFT	7
#define	MML_TABLE_SIZE	(1 << MML_SHIFT)	/* Must be a power of 2 */
#define	MML_MASK	(MML_TABLE_SIZE - 1)
#define	MML_INDEX(pp)	((((uintptr_t)(pp)) >> MML_SHIFT) & MML_MASK)
#define	MML_HASH(pp)	&ppcmmu_mlist_lock[MML_INDEX(pp)]

static kmutex_t	ppcmmu_mlist_lock[MML_TABLE_SIZE];

static kmutex_t	ppcmmu_res_lock;

/*
 * condition variable on which the vsid garbage collector thread (vsid_gc)
 * waits for work.
 */
kcondvar_t ppcmmu_cv;

/* condition variable on which someone waits for a free vsid range */
kcondvar_t ppcmmu_vsid_cv;
int ppcmmu_vsid_wanted; /* flag to indicate waiters */


void		ppcmmu_chgprot(hat_t *, caddr_t, uint_t, uint_t);
void		ppcmmu_unload(hat_t *, caddr_t, uint_t, int);
void		ppcmmu_vsid_gc(void);
void		ppcmmu_getpte(caddr_t, pte_t *);

/* Local functions */

static void	ppcmmu_init2(void);
static void	ppcmmu_alloc(hat_t *);
static void	ppcmmu_setup(hat_t *, int);
static void	ppcmmu_free(hat_t *);
static void	ppcmmu_memload(hat_t *, caddr_t, page_t *, uint_t, int);
static void	ppcmmu_devload(hat_t *, caddr_t, devpage_t *, pfn_t, uint_t, int);
static void	ppcmmu_mempte(hat_t *, page_t *, uint_t, pte_t *, caddr_t);
static void	ppcmmu_unlock(hat_t *, caddr_t, uint_t);
static int	ppcmmu_probe(hat_t *, caddr_t);
static void	ppcmmu_pageunload(page_t *, hme_t *);
static void	ppcmmu_pagesync(hat_t *, page_t *, hme_t *, uint_t);
static void	ppcmmu_lock_init(void);
static pfn_t	ppcmmu_getpfnum(hat_t *, caddr_t);
static pfn_t	ppcmmu_getkpfnum(caddr_t);
static void ppcmmu_pteload(hat_t *, caddr_t, page_t *, pte_t *, int, uint_t);
static void	ppcmmu_pteunload(ptegp_t *, pte_t *, page_t *, hme_t *, int);
static void	ppcmmu_ptesync(ptegp_t *, pte_t *, hme_t *, int);
static pte_t	*find_pte(pte_t *, uint_t, uint_t, uint_t);
static pte_t	*steal_pte(hat_t *, pte_t *, ptegp_t *);
static pte_t	*find_free_pte(hat_t *, pte_t *, ptegp_t *, page_t *);
static pte_t	*ppcmmu_ptefind(hat_t *, caddr_t, uint_t);
static uint_t	ppcmmu_vtop_prot(caddr_t, uint_t);
static uint_t	ppcmmu_ptov_prot(uint_t);
static caddr_t	pte_to_vaddr(pte_t *);
static void	pteunlock(ptegp_t *, pte_t *);
static void	build_vsid_freelist(void);
static void	ppcmmu_cache_flushpage(hat_t *, caddr_t);
static void	ppcmmu_page_clrwrt(page_t *);

/* ppcmmu locking operations */
static kmutex_t	*ppcmmu_page_enter(page_t *);
static void	ppcmmu_page_exit(kmutex_t *);
static kmutex_t	*ppcmmu_mlist_mtx(page_t *);
static kmutex_t	*ppcmmu_mlist_enter(page_t *);
static void	ppcmmu_mlist_exit(kmutex_t *);
static int	ppcmmu_mlist_held(page_t *);

static void	hat_freehat(hat_t *);

#if defined(HAT_DEBUG)

static uint_t   check_hat(hat_t *);
static uint_t   check_as_lock(hat_t *, uint_t, uint_t);
static uint_t   check_va_range(hat_t *, caddr_t, size_t);
static uint_t   check_load_consist(page_t *, pfn_t, uint_t);
static void	print_batu(uint_t);
static void	print_batl(uint_t);
static void	print_delay();
static void	print_nl();

#endif /* defined(HAT_DEBUG) */

hat_t 		*hat_gethat(void);

kmutex_t	hat_res_mutex;		/* protect global freehat list */
kmutex_t	hat_kill_procs_lock;	/* for killing process on memerr */

hat_t	*hats;
hat_t	*hatsNHATS;
hat_t	*hatfree = (hat_t *)NULL;

int		nhats;
kcondvar_t	hat_kill_procs_cv;
int		hat_inited = 0;
static int	hattrace = 0;


/*
 * The next set of routines implements the machine
 * dependent hat interface described in <vm/hat.h>
 */

static void
hat_construct(hat_t *hat, as_t *as, uint_t vsid_range)
{
	hat->hat_next = NULL;
	hat->hat_as = as;
	hat->hat_vsidr = vsid_range;
	hat->hat_freeing = 0;
	mutex_init(&hat->hat_mutex, "hat_mutex", MUTEX_DEFAULT, NULL);
	hat->hat_rss = 0;
	hat->hat_rmstat = 0;
}

/*
 * Call the init routines for PPC hat.
 */
void
hat_init(void)
{
	HERE_FN("hat_init/ppcmmu_lock_init");
	ppcmmu_lock_init();

	HERE_FN("hat_init/ppcmmu_init2");
	ppcmmu_init2();

	hat_construct(&khat, &kas, KERNEL_VSID_RANGE);
	kas.a_hat = (xhat_t *)&khat;
}

void
hat_init_userland(void)
{
	hat_t *hat;
	size_t hats_size;

	/* XXX Allocate hats dynamically; use kmem_cache_create, et al. */

	/*
	 * Allocate MMU independent hat data structures.
	 */
	nhats = v.v_proc + (v.v_proc / 2);
	prom_printf("hat_init_userland: nhats=%d\n", nhats);
	hats_size = sizeof (hat_t) * nhats;
	hats = (hat_t *)nucleus_zalloc(hats_size, 1, "hat_init", "hats[]");
	hatsNHATS = hats + nhats;

	for (hat = hatsNHATS - 1; hat >= hats; --hat) {
		mutex_init(&hat->hat_mutex, "hat_mutex", MUTEX_DEFAULT, NULL);
		hat_freehat(hat);
	}
}

/*
 * Switch to a new active hat, maintaining bit masks to track active CPUs.
 */
void
hat_switch(xhat_t *xhat)
{
	panic("Unimplemented function: hat_switch.\n");
}

/*
 * Called to ensure that all pagetables are in the system dump
 */
void
hat_dump(void)
{
}

/*
 * Allocate a hat structure.
 * Called when an address space first uses a hat.
 * Links allocated hat onto the hat list for the address space (as->a_hat)
 */
xhat_t *
hat_alloc(as_t *as)
{
	hat_t *hat;

	hat =  hat_gethat();
	if (hat == NULL)
		panic("hat_alloc - no hats");
	ASSERT(AS_WRITE_HELD(as, &as->a_lock));
	hat_construct(hat, as, VSIDRANGE_INVALID);
	ppcmmu_alloc(hat);
	return ((xhat_t *)hat);
}

/*
 * Hat_setup, makes an address space context the current active one;
 * uses the default hat, calls the setup routine for the PPC MMU.
 */
void
hat_setup(xhat_t *xhat, int allocflag)
{
	hat_t *hat = (hat_t *)xhat;

	hat_enter(xhat);
	ppcmmu_setup(hat, allocflag);
#if defined(XXX_OBSOLETE)
	curthread->t_mmuctx = 0;
#endif /* defined(XXX_OBSOLETE) */
	hat_exit(xhat);
}

/*
 * Free all the translation resources for the specified address space.
 * Called from as_free when an address space is being destroyed.
 */
void
hat_free_start(xhat_t *xhat)
{
	hat_t *hat = (hat_t *)xhat;

	/* Free everything, called from as_free() */
	if (hat->hat_rmstat)
		hat_freestat(hat->hat_as, NULL);
	ppcmmu_free(hat);
}


void
hat_free_end(xhat_t *xhat)
{
	hat_t *hat = (hat_t *)xhat;

	hat->hat_as = NULL;
	hat_freehat(hat);
}

/*
 * Duplicate the translations of an as into another newas
 *
 * As with SRMMU implementation, we do not duplicate any translations
 * when we are duplicating the address space.  Rather, we let the
 * forked process fault in the mappings that the parent process had.
 */

/* ARGSUSED */
int
hat_dup(xhat_t *xhat, xhat_t *xnewhat, caddr_t vaddr, size_t len, uint_t flag)
{
	hat_t *hat = (hat_t *)xhat;
	hat_t *newhat = (hat_t *)xnewhat;

	ASSERT((flag == 0) || (flag == HAT_DUP_ALL) || (flag == HAT_DUP_COW));

	if (flag == HAT_DUP_COW) {
		panic("hat_dup: HAT_DUP_COW not supported");
		}
	return (0);
}

/*
 * Set up any translation structures, for the specified address space,
 * that are needed or preferred when the process is being swapped in.
 */
/* ARGSUSED */
void
hat_swapin(xhat_t *xhat)
{
	hat_t *hat = (hat_t *)xhat;
	as_t *as = hat->hat_as;

	ASSERT(AS_LOCK_HELD(as, &as->a_lock));
	/* Do nothing.  We let everything fault back in. */
}

/*
 * Free all of the translation resources, for the specified address space,
 * that can be freed while the process is swapped out.  Called from as_swapout.
 * Also, free up the ctx that this process was using.
 */
void
hat_swapout(xhat_t *xhat)
{
	hat_t *hat = (hat_t *)xhat;
	as_t *as = hat->hat_as;

	ASSERT(AS_LOCK_HELD(as, &as->a_lock));
	// XXX Now, we do nothing.
	// XXX Later, we may want to actually walk the page table
	// XXX and purge translations for a given VSID range.
	// XXX We would not have to do TLB shoot-down,
	// XXX because we do not reuse VSID ranges.
	// XXX But, it might be helpful to just free up the PTEs
	// XXX occupying space in the page table,
	// XXX and free up HMEs.
}

/*
 * hat_memload() - load a translation to the given page struct
 *
 * Flags for hat_memload/hat_devload/hat_*attr.
 *
 * 	HAT_LOAD	Default flags to load a translation to the page.
 *
 * 	HAT_LOAD_LOCK	Lock down mapping resources; hat_map(), hat_memload(),
 *			and hat_devload().
 *
 *	HAT_LOAD_NOCONSIST Do not add mapping to page_t mapping list.
 *			sets PT_NOCONSIST (soft bit)
 *
 *	HAT_LOAD_SHARE	A flag to hat_memload() to indicate h/w page tables
 *			that map some user pages (not kas) is shared by more
 *			than one process (eg. ISM).
 *
 *	HAT_LOAD_REMAP	Reload a valid pte with a different page frame.
 *
 *	HAT_NO_KALLOC	Do not kmem_alloc while creating the mapping; at this
 *			point, it's setting up mapping to allocate internal
 *			hat layer data structures.  This flag forces hat layer
 *			to tap its reserves in order to prevent infinite
 *			recursion.
 *
 * The following is a protection attribute (like PROT_READ, etc.)
 *
 *	HAT_NOSYNC	set PT_NOSYNC (soft bit) - this mapping's ref/mod bits
 *			are never cleared.
 *
 * Installing new valid PTE's and creation of the mapping list
 * entry are controlled under the same lock.  It's derived from the
 * page_t being mapped.
 *
 * hat_memload():
 *   Make a mapping at vaddr to map page pp with protection prot.
 *
 * No locking at this level.  Hat_memload and hat_devload
 * are wrappers for <mmu>_pteload, where <mmu> is instance of
 * a hat module like the sun mmu.  <mmu>_pteload is called from
 * machine dependent code directly.  The locking (acquiring
 * the per hat hat_mutex) that is done in <mmu>_pteload allows
 * both the hat layer and the machine dependent code to work properly.
 */

static uint_t supported_memload_flags =
	HAT_LOAD | HAT_LOAD_LOCK | HAT_LOAD_ADV | HAT_LOAD_NOCONSIST |
	HAT_LOAD_SHARE | HAT_NO_KALLOC | HAT_LOAD_REMAP | HAT_LOAD_TEXT;

/*
 * PowerPC does not support HAT_LOAD_CONTIG
 */

void
hat_memload(xhat_t *xhat, caddr_t vaddr, page_t *pp, uint_t attr, uint_t flags)
{
	hat_t *hat = (hat_t *)xhat;
	uintptr_t va = (uintptr_t)vaddr;

	HATIN(hat_memload, hat, vaddr, (size_t)MMU_PAGESIZE); // XXX __inline__
	ASSERT(check_va_range(hat, vaddr, MMU_PAGESIZE));
	ASSERT(USER_HAT_AS_LOCK_HELD(hat));
	ASSERT((flags & supported_memload_flags) == flags);
	ASSERT(!PP_ISFREE(pp));
	ppcmmu_memload(hat, vaddr, pp, attr, flags);
	HATOUT(hat_memload, hat, vaddr); // XXX __inline__
}

/*
 * void hat_devload(hat, addr, len, pf, attr, flags)
 *	load/lock the given page frame number
 *
 * Advisory ordering attributes. Apply only to device mappings.
 *
 * HAT_STRICTORDER: the CPU must issue the references in order, as the
 *	programmer specified.  This is the default.
 * HAT_UNORDERED_OK: the CPU may reorder the references (this is all kinds
 *	of reordering; store or load with store or load).
 * HAT_MERGING_OK: merging and batching: the CPU may merge individual stores
 *	to consecutive locations (for example, turn two consecutive byte
 *	stores into one halfword store), and it may batch individual loads
 *	(for example, turn two consecutive byte loads into one halfword load).
 *	This also implies re-ordering.
 * HAT_LOADCACHING_OK: the CPU may cache the data it fetches and reuse it
 *	until another store occurs.  The default is to fetch new data
 *	on every load.  This also implies merging.
 * HAT_STORECACHING_OK: the CPU may keep the data in the cache and push it to
 *	the device (perhaps with other data) at a later time.  The default is
 *	to push the data right away.  This also implies load caching.
 *
 * Equivalent of hat_memload(), but can be used for device memory where
 * there are no page_t's and we support additional flags (write merging, etc).
 * Note that we can have large page mappings with this interface.
 */

int supported_devload_flags = HAT_LOAD | HAT_LOAD_LOCK |
	HAT_LOAD_NOCONSIST | HAT_STRICTORDER | HAT_UNORDERED_OK |
	HAT_MERGING_OK | HAT_LOADCACHING_OK | HAT_STORECACHING_OK;

/*
 * Cons up a PTE using the device's pf bits and protection
 * prot to load into the hardware for address vaddr; treat as minflt.
 */
void
hat_devload(xhat_t *xhat, caddr_t vaddr, size_t len, pfn_t pfn, uint_t attr, int flags)
{
	hat_t *hat = (hat_t *)xhat;
	devpage_t *dp = NULL;

	ASSERT(check_va_range(hat, vaddr, len));
	ASSERT(USER_HAT_AS_LOCK_HELD(hat));
	ASSERT((flags & supported_devload_flags) == flags);

	while (len) {
		/*
		 * If it's a memory page find its pp
		 */
		if (!(flags & HAT_LOAD_NOCONSIST) && pf_is_memory(pfn)) {
			dp = (devpage_t *)page_numtopp_nolock(pfn);
			if (dp == NULL)
				flags |= HAT_LOAD_NOCONSIST;
		} else {
			dp = NULL;
		}
		ppcmmu_devload(hat, vaddr, dp, pfn, attr, flags);
		++pfn;
		vaddr += MMU_PAGESIZE;
		len -= MMU_PAGESIZE;
	}
}

/*
 * Set up range of mappings for array of pp's.
 */
void
hat_memload_array(xhat_t *xhat, caddr_t vaddr, size_t len, page_t **ppa, uint_t attr, uint_t flags)
{
	hat_t *hat = (hat_t *)xhat;
	uintptr_t va = (uintptr_t)vaddr;
	caddr_t eaddr = vaddr + len;

	HATIN(hat_memload_array, hat, vaddr, len);
	ASSERT(check_va_range(hat, vaddr, len));
	ASSERT(USER_HAT_AS_LOCK_HELD(hat));
	ASSERT((flags & supported_memload_flags) == flags);
	for (; vaddr < eaddr; vaddr += PAGESIZE, ++ppa) {
		page_t *pp = *ppa;
		ASSERT(!PP_ISFREE(pp));
		ppcmmu_memload(hat, vaddr, pp, attr, flags);
	}
	HATOUT(hat_memload_array, hat, vaddr);
}

/*
 * Release one hardware address translation lock on the given address range.
 */
void
hat_unlock(xhat_t *xhat, caddr_t vaddr, size_t len)
{
	hat_t *hat = (hat_t *)xhat;

	ASSERT(check_va_range(hat, vaddr, len));
	ASSERT(USER_HAT_AS_LOCK_HELD(hat));
	ppcmmu_unlock(hat, vaddr, len);
}

/*
 * hat_chgprot is a deprecated hat call.  New segment drivers
 * should store all attributes and use hat_*attr calls.
 *
 * Change the protections in the virtual address range
 * given to the specified virtual protection.  If vprot is ~PROT_WRITE,
 * then remove write permission, leaving the other
 * permissions unchanged.  If vprot is ~PROT_USER, remove user permissions.
 *
 */
void
hat_chgprot(xhat_t *xhat, caddr_t vaddr, size_t len, uint_t vprot)
{
	hat_t *hat = (hat_t *)xhat;

	ASSERT(check_va_range(hat, vaddr, len));
	ASSERT(HAT_AS_LOCK_HELD(hat));
	ppcmmu_chgprot(hat, vaddr, len, vprot);
}

/*
 * Enables more attributes on specified address range (ie. logical OR)
 */
void
hat_setattr(xhat_t *xhat, caddr_t vaddr, size_t len, uint_t attr)
{
	hat_t *hat = (hat_t *)xhat;

	ASSERT(check_va_range(hat, vaddr, len));
	ASSERT(HAT_AS_LOCK_HELD(hat));
	ppcmmu_chgprot(hat, vaddr, len, attr);
}

/*
 * Assigns attributes to the specified address range.
 * All the attributes are specified.
 */
void
hat_chgattr(xhat_t *xhat, caddr_t vaddr, size_t len, uint_t attr)
{
	hat_t *hat = (hat_t *)xhat;

	ASSERT(check_va_range(hat, vaddr, len));
	ASSERT(HAT_AS_LOCK_HELD(hat));
	ASSERT((attr & ~HAT_PROT_MASK) != 0);
	ppcmmu_chgprot(hat, vaddr, len, attr);
}

/*
 * Remove attributes on the specified address range (ie. logical NAND)
 */
void
hat_clrattr(xhat_t *xhat, caddr_t vaddr, size_t len, uint_t attr)
{
	hat_t *hat = (hat_t *)xhat;

	ASSERT(check_va_range(hat, vaddr, len));
	ASSERT(HAT_AS_LOCK_HELD(hat));
	if (attr & PROT_USER)
		ppcmmu_chgprot(hat, vaddr, len, ~PROT_USER);

	if (attr & PROT_WRITE)
		ppcmmu_chgprot(hat, vaddr, len, ~PROT_WRITE);
}

/*
 * Unload all the mappings in the range [vaddr..vaddr+len).
 * vaddr and len must be MMU_PAGESIZE aligned.
 */
void
hat_unload(xhat_t *xhat, caddr_t vaddr, size_t len, uint_t flags)
{
	hat_t *hat = (hat_t *)xhat;

	ASSERT(check_va_range(hat, vaddr, len));
	ASSERT((flags & HAT_UNLOAD_OTHER) || USER_HAT_AS_LOCK_HELD(hat));
	ppcmmu_unload(hat, vaddr, len, flags);
}

/*
 * Unload a given range of addresses (has optional callback)
 *
 * Flags:
 * define	HAT_UNLOAD		0x00
 * define	HAT_UNLOAD_NOSYNC	0x02
 * define	HAT_UNLOAD_UNLOCK	0x04
 * define	HAT_UNLOAD_OTHER	0x08 - not used
 * define	HAT_UNLOAD_UNMAP	0x10 - same as HAT_UNLOAD
 */
#define	MAX_UNLOAD_CNT (8)
void
hat_unload_callback(
	xhat_t		*xhat,
	caddr_t		vaddr,
	size_t		len,
	uint_t		flags,
	hat_callback_t	*cb)
{
	hat_t *hat = (hat_t *)xhat;
	uintptr_t va = (uintptr_t)vaddr;
	uintptr_t eva = va + len;

	HATIN(hat_unload_callback, hat, vaddr, len);
	ASSERT(check_va_range(hat, vaddr, len));
	panic("Unimplemented function: hat_unload_callback.\n");

#if defined(XXX_Unimplemented)
	uintptr_t	va = (uintptr_t)vaddr;
	uintptr_t	eva = va + len;
	htable_t	*ht = NULL;
	uint_t		entry;
	uintptr_t	contig_va = (uintptr_t)-1L;
	range_info_t	r[MAX_UNLOAD_CNT];
	uint_t		r_cnt = 0;
	x86pte_t	old_pte;

	HATIN(hat_unload_callback, hat, vaddr, len);
	ASSERT(hat == kas.a_hat || eva <= kernelbase);
	ASSERT(is_page_aligned(va));
	ASSERT(is_page_aligned(eva));

	while (va < eva) {
		old_pte = htable_walk(hat, &ht, &va, eva);
		if (ht == NULL)
			break;

		ASSERT(!IN_VA_HOLE(va));

		if (va < (uintptr_t)vaddr)
			panic("hat_unload_callback(): unmap inside large page");

		/*
		 * We'll do the call backs for contiguous ranges
		 */
		if (va != contig_va ||
		    (r_cnt > 0 && r[r_cnt - 1].rng_level != ht->ht_level)) {
			if (r_cnt == MAX_UNLOAD_CNT) {
				handle_ranges(cb, r_cnt, r);
				r_cnt = 0;
			}
			r[r_cnt].rng_va = va;
			r[r_cnt].rng_cnt = 0;
			r[r_cnt].rng_level = ht->ht_level;
			++r_cnt;
		}

		/*
		 * Unload one mapping from the page tables.
		 */
		entry = htable_va2entry(va, ht);
		hat_pte_unmap(ht, entry, flags, old_pte, NULL);

		ASSERT(ht->ht_level <= mmu.max_page_level);
		vaddr += LEVEL_SIZE(ht->ht_level);
		contig_va = va;
		++r[r_cnt - 1].rng_cnt;
	}
	if (ht)
		htable_release(ht);

	/*
	 * handle last range for callbacks
	 */
	if (r_cnt > 0)
		handle_ranges(cb, r_cnt, r);

	HATOUT(hat_unload_callback, hat, vaddr);
#endif /* defined(XXX_Unimplemented) */
}

/*
 * Synchronize all the mappings in the range [vaddr..vaddr+len).
 */
void
hat_sync(xhat_t *xhat, caddr_t vaddr, size_t len, uint_t clearflag)
{
	hat_t *hat = (hat_t *)xhat;
	pte_t	*pte;
	ptegp_t	*ptegp;
	hme_t	*hme;
	caddr_t	eaddr = vaddr + len;
	uint_t sync_flags = clearflag ? PPCMMU_RMSYNC : PPCMMU_RMSTAT;

	HATIN(hat_sync, hat, vaddr, len);
	ASSERT(check_va_range(hat, vaddr, len));
#if 0
	ASSERT(USER_HAT_AS_LOCK_HELD(hat));
#endif
	mutex_enter(&hat->hat_mutex);
	for (; vaddr < eaddr; vaddr += MMU_PAGESIZE) {
		pte = ppcmmu_ptefind(hat, vaddr, PTEGP_LOCK);
		if (pte == NULL)
			continue;
		ptegp = pte_to_ptegp(pte);
		hme = pte_to_hme(pte);
		ppcmmu_ptesync(ptegp, pte, hme, sync_flags);
		mutex_exit(&ptegp->ptegp_mutex);
	}
	mutex_exit(&hat->hat_mutex);
	HATOUT(hat_sync, hat, vaddr);
}

/*
 * Remove all mappings to page 'pp'.
 */
/* ARGSUSED */
int
hat_pageunload(page_t *pp, uint_t forceflag)
{
	hme_t *hme;
	kmutex_t *pml;

	ASSERT(PAGE_EXCL(pp));

	pml = ppcmmu_mlist_enter(pp);
	while ((hme = (hme_t *)pp->p_mapping) != NULL)
		ppcmmu_pageunload(pp, hme);

	ASSERT(pp->p_mapping == NULL);
	ppcmmu_mlist_exit(pml);

	return (0);
}

/*
 * Synchronize software page_t with hardware ref/change bits.
 * Resets the hardware PTE ref/change bits.
 */
uint_t
hat_pagesync(page_t *pp, uint_t clearflag)
{
	hme_t *hme;
	hat_t *hat;
	kmutex_t *pml;

	ASSERT(PAGE_LOCKED(pp) || panicstr);

	pml = ppcmmu_mlist_enter(pp);
	for (hme = (hme_t *)pp->p_mapping; hme; hme = hme->hme_next) {
		hat = &hats[hme->hme_hat];
		ppcmmu_pagesync(hat, pp, hme, clearflag & ~HAT_SYNC_STOPON_RM);
		/*
		 * If clearflag is HAT_DONTZERO, break out as soon
		 * as the "ref" or "mod" is set.
		 */
		if ((clearflag & ~HAT_SYNC_STOPON_RM) == HAT_SYNC_DONTZERO &&
		    ((clearflag & HAT_SYNC_STOPON_MOD) && PP_ISMOD(pp)) ||
		    ((clearflag & HAT_SYNC_STOPON_REF) && PP_ISREF(pp)))
			break;
	}
	ppcmmu_mlist_exit(pml);
	return (PP_GENERIC_ATTR(pp));
}

/*
 * Returns a page frame number for a given user virtual address.
 * Returns -1 to indicate an invalid mapping
 *
 * We would like to ASSERT(AS_LOCK_HELD(as, &as->a_lock));
 * but we can't because the IOMMU driver will call this
 * routine at interrupt time and it can't grab the as lock
 * or it will deadlock: A thread could have the as lock
 * and be waiting for I/O.  The I/O can't complete
 * because the interrupt thread is blocked trying to grab
 * the as lock.
 */

pfn_t
hat_getpfnum(xhat_t *xhat, caddr_t vaddr)
{
	hat_t *hat = (hat_t *)xhat;

	if (IS_KERNEL_HAT(hat))
		return (ppcmmu_getkpfnum(vaddr));
	else
		return (ppcmmu_getpfnum(hat, vaddr));
}

/*
 * Return the number of mappings to a particular page.
 * This number is an approximation of the number of
 * processes sharing the page.
 */
ulong_t
hat_page_getshare(page_t *pp)
{
	return (pp->p_share);
}

/* ARGSUSED */
ssize_t
hat_getpagesize(xhat_t *xhat, caddr_t vaddr)
{
	hat_t *hat = (hat_t *)xhat;

	ASSERT(check_va_range(hat, vaddr, MMU_PAGESIZE));
	if (ppcmmu_probe(hat, vaddr))
		return (MMU_PAGESIZE);	/* always the same size */
	else
		return (0);
}

/*
 * hat_probe returns 1 if the translation for the address 'vaddr' is
 * loaded, zero otherwise.
 *
 * hat_probe should be used only for advisorary purposes because it may
 * occasionally return the wrong value.  The implementation must guarantee
 * that returning the wrong value is a very rare event.  hat_probe is used
 * to implement optimizations in the segment drivers.
 *
 * hat_probe doesn't acquire hat_mutex.
 */

int
hat_probe(xhat_t *xhat, caddr_t vaddr)
{
	hat_t *hat = (hat_t *)xhat;

	return (ppcmmu_probe(hat, vaddr));
}

/*
 * For compatability with AT&T and later optimizations
 */
/* ARGSUSED */
void
hat_map(xhat_t *xhat, caddr_t vaddr, size_t len, uint_t flags)
{
	hat_t *hat = (hat_t *)xhat;

	ASSERT(check_va_range(hat, vaddr, len));
	panic("Unexpected call to hat_map().\n");
}

/*
 * uint_t hat_getattr(hat, vaddr, *attr)
 *	returns attr for <hat,vaddr> in *attr.  returns 0 if there was a
 *	mapping and *attr is valid, nonzero if there was no mapping and
 *	*attr is not valid.
 */
uint_t
hat_getattr(xhat_t *xhat, caddr_t vaddr, uint_t *attr)
{
	hat_t *hat = (hat_t *)xhat;

	ASSERT(check_va_range(hat, vaddr, MMU_PAGESIZE));
	panic("Unimplemented function: hat_getattr.\n");
#if defined(XXX_Unimplemented)
	uintptr_t	va = align2page((uintptr_t)vaddr);
	htable_t	*ht = NULL;
	x86pte_t	pte;

	ASSERT(hat == kas.a_hat || va < kernelbase);

	if (IN_VA_HOLE(va))
		return ((uint_t)-1);

	ht = htable_getpte(hat, va, NULL, &pte, MAX_PAGE_LEVEL);
	if (ht == NULL)
		return ((uint_t)-1);

	if (!PTE_ISVALID(pte) || !PTE_ISPAGE(pte, ht->ht_level)) {
		htable_release(ht);
		return ((uint_t)-1);
	}

	*attr = PROT_READ;
	if (PTE_GET(pte, PT_WRITABLE))
		*attr |= PROT_WRITE;
	if (PTE_GET(pte, PT_USER))
		*attr |= PROT_USER;
	if (!PTE_GET(pte, mmu.pt_nx))
		*attr |= PROT_EXEC;
	if (PTE_GET(pte, PT_NOSYNC))
		*attr |= HAT_NOSYNC;
	htable_release(ht);
	return (0);
#endif /* defined(XXX_Unimplemented) */
}

void
hat_page_setattr(page_t *pp, uint_t flag)
{
	vnode_t		*vp = pp->p_vnode;
	kmutex_t	*vphm = NULL;
	page_t		**listp;
	kmutex_t	*pmtx;

	ASSERT(!(flag & ~(P_MOD | P_REF | P_RO)));

	if (PP_GETRM(pp, flag) == flag) {
		/* Attribute already set */
		return;
	}

	if ((flag & P_MOD) != 0 && vp != NULL && IS_VMODSORT(vp)) {
		vphm = page_vnode_mutex(vp);
		mutex_enter(vphm);
	}

	pmtx = ppcmmu_page_enter(pp);
	pp->p_nrm |= flag;
	ppcmmu_page_exit(pmtx);

	if (vphm != NULL) {

		/*
		 * Some File Systems examine v_pages for NULL w/o
		 * grabbing the vphm mutex.  Must not let it become NULL
		 * when pp is the only page on the list.
		 */
		if (pp->p_vpnext != pp) {
			page_vpsub(&vp->v_pages, pp);
			if (vp->v_pages != NULL)
				listp = &vp->v_pages->p_vpprev->p_vpnext;
			else
				listp = &vp->v_pages;
			page_vpadd(listp, pp);
		}
		mutex_exit(vphm);
	}
}

void
hat_page_clrattr(page_t *pp, uint_t flag)
{
	vnode_t		*vp = pp->p_vnode;
	kmutex_t	*vphm = NULL;
	kmutex_t	*pmtx;

	ASSERT(!(flag & ~(P_MOD | P_REF | P_RO)));

	/*
	 * for vnode with a sorted v_pages list, we need to change
	 * the attributes and the v_pages list together under page_vnode_mutex.
	 */
	if ((flag & P_MOD) != 0 && vp != NULL && IS_VMODSORT(vp)) {
		vphm = page_vnode_mutex(vp);
		mutex_enter(vphm);
	}

	pmtx = ppcmmu_page_enter(pp);
	pp->p_nrm &= ~flag;
	ppcmmu_page_exit(pmtx);

	if (vphm != NULL) {

		/*
		 * Some File Systems examine v_pages for NULL w/o
		 * grabbing the vphm mutex. Must not let it become NULL when
		 * pp is the only page on the list.
		 */
		if (pp->p_vpnext != pp) {
			page_vpsub(&vp->v_pages, pp);
			page_vpadd(&vp->v_pages, pp);
		}
		mutex_exit(vphm);

		/*
		 * VMODSORT works by removing write permissions and getting
		 * a fault when a page is made dirty. At this point
		 * we need to remove write permission from all mappings
		 * to this page.
		 */
		ppcmmu_page_clrwrt(pp);
	}
}

uint_t
hat_page_getattr(page_t *pp, uint_t flag)
{
	ASSERT(!(flag & ~(P_MOD | P_REF | P_RO)));
	return (pp->p_nrm & flag);
}

/*
 * Copy top level mapping elements (L1 ptes or whatever)
 * that map from saddr to (saddr + len) in sas
 * to top level mapping elements from daddr in das.
 *
 * Hat_share()/unshare() return an (non-zero) error
 * when saddr and daddr are not properly aligned.
 *
 * The top level mapping element determines the alignment
 * requirement for saddr and daddr, depending on different
 * architectures.
 *
 * When hat_share()/unshare() are not supported,
 * HATOP_SHARE()/UNSHARE() return 0
 */
/* ARGSUSED */
int
hat_share(xhat_t *xhat, caddr_t vaddr, xhat_t *ism_hatid, caddr_t sptaddr,
		size_t size, uint_t ismszc)
{
	hat_t *hat = (hat_t *)xhat;

	ASSERT(check_va_range(hat, vaddr, size));
	return (0);
}

/*
 * Invalidate top level mapping elements in as,
 * starting from vaddr to (vaddr + size).
 */
/* ARGSUSED */
void
hat_unshare(xhat_t *xhat, caddr_t vaddr, size_t size, uint_t ismszc)
{
	hat_t *hat = (hat_t *)xhat;

	ASSERT(check_va_range(hat, vaddr, size));
}

/*
 * Get a hat structure from the freelist
 */
hat_t *
hat_gethat(void)
{
	hat_t *hat;

	mutex_enter(&hat_res_mutex);
	if ((hat = hatfree) == NULL)	/* "shouldn't happen" */
		panic("hat_gethat - out of hats");

	hatfree = hat->hat_next;
	hat->hat_next = NULL;

	mutex_exit(&hat_res_mutex);
	return (hat);
}

static void
hat_freehat(hat_t *hat)
{
	int i;

	mutex_enter(&hat->hat_mutex);

	mutex_enter(&hat_res_mutex);
	hat->hat_as = (as_t *)NULL;
	hat->hat_vsidr = 0;
	hat->hat_freeing = 0;
	mutex_exit(&hat->hat_mutex);

	hat->hat_next = hatfree;
	hatfree = hat;
	mutex_exit(&hat_res_mutex);
}

/*
 * Enter a hme on the mapping list for page pp
 */
void
hme_add(hme_t *hme, page_t *pp)
{
	ASSERT(ppcmmu_mlist_held(pp));

	hme->hme_prev = NULL;
	hme->hme_next = pp->p_mapping;
	IFDEBUG(hme->hme_page = pp);
	if (pp->p_mapping) {
		((hme_t *)pp->p_mapping)->hme_prev = hme;
		ASSERT(pp->p_share > 0);
	} else  {
		ASSERT(pp->p_share == 0);
	}
	pp->p_mapping = hme;
	pp->p_share++;
}

/*
 * remove a hme from the mapping list for page pp
 */
void
hme_sub(hme_t *hme, page_t *pp)
{
	ASSERT(ppcmmu_mlist_held(pp));
	ASSERT(hme->hme_page == pp);

	if (pp->p_mapping == NULL)
		panic("hme_sub - no mappings");

	ASSERT(pp->p_share > 0);
	--(pp->p_share);

	if (hme->hme_prev) {
		ASSERT(pp->p_mapping != hme);
		ASSERT(hme->hme_prev->hme_page == pp);
		hme->hme_prev->hme_next = hme->hme_next;
	} else {
		ASSERT(pp->p_mapping == hme);
		pp->p_mapping = hme->hme_next;
		ASSERT((pp->p_mapping == NULL) ?
		    (pp->p_share == 0) : 1);
	}

	if (hme->hme_next) {
		ASSERT(hme->hme_next->hme_page == pp);
		hme->hme_next->hme_prev = hme->hme_prev;
	}

	/*
	 * zero out the entry
	 */
	hme->hme_next = NULL;
	hme->hme_prev = NULL;
	hme->hme_hat = NULL;
	IFDEBUG(hme->hme_page = NULL);
}

void
hat_enter(xhat_t *xhat)
{
	hat_t *hat = (hat_t *)xhat;

	mutex_enter(&hat->hat_mutex);
}

void
hat_exit(xhat_t *xhat)
{
	hat_t *hat = (hat_t *)xhat;

	mutex_exit(&hat->hat_mutex);
}

/*
 * Yield the memory claim requirement for an address space.
 *
 * This is currently implemented as the number of active hardware
 * translations that have page structures.  Therefore, it can
 * underestimate the traditional resident set size, eg, if the
 * physical page is present and the hardware translation is missing;
 * and it can overestimate the rss, eg, if there are active
 * translations to a frame buffer with page structs.
 * Also, it does not take sharing into account.
 */
size_t
hat_get_mapped_size(xhat_t *xhat)
{
	hat_t *hat = (hat_t *)xhat;

	if (hat == NULL)
		return (0);
	return ((size_t)ptob(hat->hat_rss));
}

#if defined(XXX_EXPERIMENTAL)

#define	ASCHUNK	64
/*
 * Kill process(es) that use the given page.  Used for parity recovery.
 * If we encounter the kernel's address space, give up (return -1).
 * Otherwise, we return 0.
 */
/* ARGSUSED */
int
hat_kill_procs(page_t *pp, caddr_t vaddr)
{
	struct sf_hment *hme;
	hat_t		*hat;
	as_t		*as;
	proc_t		*p;
	as_t		*as_array[ASCHUNK];
	kmutex_t	*pml;
	int		loop = 0;
	int		opid = -1;
	int		i;

	pml = ppcmmu_mlist_enter(pp);
again:
	if (pp->p_mapping) {
		bzero((caddr_t)&as_array[0], ASCHUNK * sizeof (int));
		for (i = 0; i < ASCHUNK; ++i) {
			hme = (struct sf_hment *)pp->p_mapping;
			hat = &hats[hme->hme_hat];
			as = hat->sfmmu_as;

			/*
			 * If the address space is the kernel's, then fail.
			 * The only thing to do with corrupted kernel memory
			 * is die.  The caller is expected to panic if this
			 * is true.
			 */
			if (as == &kas) {
				ppcmmu_mlist_exit(pml);
				printf("parity error: ");
				printf("kernel address space\n");
				return (-1);
			}
			as_array[i] = as;

			if (hme->hme_next)
				hme = hme->hme_next;
			else
				break;
		}
	}

	for (i = 0; i < ASCHUNK; ++i) {

		as = as_array[i];
		if (as == NULL)
			break;

		/*
		 * Note that more than one process can share the
		 * same address space, if vfork() was used to create it.
		 * This means that we have to look through the entire
		 * process table and not stop at the first match.
		 */
		mutex_enter(&pidlock);
		for (p = practive; p; p = p->p_next) {
			k_siginfo_t siginfo;

			if (p->p_as == as) {
				/* limit messages about same process */
				if (opid != p->p_pid) {
					printf("parity error: killing pid %d\n",
					    (int)p->p_pid);
					opid =  p->p_pid;
					uprintf("pid %d killed: parity error\n",
					    (int)p->p_pid);
				}

				bzero((caddr_t)&siginfo, sizeof (siginfo));
				siginfo.si_addr = vaddr;
				siginfo.si_signo = SIGBUS;
				/*
				 * the following code should probably be
				 * something from siginfo.h
				 */
				siginfo.si_code = FC_HWERR;

				mutex_enter(&p->p_lock);
				sigaddq(p, NULL, &siginfo, KM_NOSLEEP);
				mutex_exit(&p->p_lock);
			}
		}
		mutex_exit(&pidlock);
	}

	/*
	 * Wait for previously signaled processes to die off,
	 * thus removing their mappings from the mapping list.
	 * XXX - change me to cv_timed_wait.
	 */
	(void) timeout(hat_kill_procs_wakeup, (caddr_t)&hat_kill_procs_cv, hz);

	mutex_enter(&hat_kill_procs_lock);
	cv_wait(&hat_kill_procs_cv, &hat_kill_procs_lock);
	mutex_exit(&hat_kill_procs_lock);

	/*
	 * More than ASCHUNK mappings on the list for the page,
	 * loop to kill off the rest of them.  This will terminate
	 * with failure if there are more than ASCHUNK*20 mappings
	 * or a process will not die.
	 */
	if (pp->p_mapping) {
		++loop;
		if (loop > 20) {
			ppcmmu_mlist_exit(pml);
			return (-1);
		}
		goto again;
	}
	ppcmmu_mlist_exit(pml);
	return (0);
}

#endif /* defined(XXX_EXPERIMENTAL) */

/*
 * Initialize locking for the hat layer, called early during boot.
 */
static void
ppcmmu_lock_init(void)
{
	uint_t i;

	mutex_init(&ppcmmu_res_lock, NULL, MUTEX_DEFAULT, NULL);

	for (i = 0; i < SPL_TABLE_SIZE; ++i) {
		mutex_init(&ppcmmu_page_lock[i], NULL, MUTEX_DEFAULT, NULL);
	}

	for (i = 0; i < MML_TABLE_SIZE; ++i) {
		mutex_init(&ppcmmu_mlist_lock[i], NULL, MUTEX_DEFAULT, NULL);
	}
}

/*
 * Initialize the hardware address translation structures.
 * Called after the vm structures have been allocated
 * and mapped in.
 *
 * The page table must be allocated by this time, we need to allocate
 * hments array, ptegps array and vsid-range allocation list and
 * bit map for vsid-range-inuse.
 *
 * It is assumed that ppcmmu_init() has already been called.
 * So, the page table has been sized, allocated and initialized.
 */

static void
ppcmmu_init2(void)
{
	size_t sz;
	uint_t i;

	/* The kernel page table should be allocated if we are here */
	ASSERT(nptegp != 0);

	/* Allocate hments array */
	sz = sizeof (hme_t) * nptegp * NPTEPERPTEGP;
	hments = (hme_t *)nucleus_zalloc(sz, 1, "ppcmmu_init2", "hments[]");
	ehments = hments + nptegp * NPTEPERPTEGP;

	/* Allocate ptegps array */
	sz = sizeof (ptegp_t) * nptegp;
	ptegps = (ptegp_t *)nucleus_zalloc(sz, 1, "ppcmmu_init2", "ptegps[]");
	eptegps = ptegps + nptegp;
	/* Initialize per ptegp mutexes */
	for (i = 0; i < nptegp; ++i) {
		mutex_init(&ptegps[i].ptegp_mutex, NULL, MUTEX_DEFAULT, NULL);
	}

	if (ppcmmu_max_vsidranges > MAX_VSIDRANGES)
		ppcmmu_max_vsidranges = MAX_VSIDRANGES;

	/* Allocate the bit map for vsid-range-inuse */
	i = (ppcmmu_max_vsidranges + (NBINT - 1)) / NBINT;
	sz = i * NBPW;
	vsid_bitmap = (uint_t *)nucleus_zalloc(sz, 1, "ppcmmu_init2", "vsid_bitmap[]");
	/*
	 * Note:
	 * Assign the vsid range KERNEL_VSID_RANGE for the kernel use.
	 * We assume that the boot has used the same vsid-range-id
	 * when loading segment registers.  So we just mark it as used
	 * and fix the vsid free list.  We also mark vsid range id 0
	 * as used to make it an invalid vsid range for allocation.
	 */
	SET_VSID_RANGE_INUSE(0);
	SET_VSID_RANGE_INUSE(KERNEL_VSID_RANGE);
#if 0
	build_vsid_freelist(); /* build the initial vsid_alloc_set list */
#endif

	/*
	 * Allocate hash table for ptelock counts.  The current allocation
	 * is based on the page table size with a ratio of 1:64 PTEs.
	 * Since we keep counter for up to the lock count of 2 in the ptegp
	 * structure itself, this hash table should get used for fewer
	 * locked pages (e.g. segkmap segment pages, pages from user threads
	 * doing physio on the same page, etc.).
	 */
	ptelock_hashtab_size = (nptegp * NPTEPERPTEGP) / ptetohash_ratio;
	sz = sizeof (ptelock_hash_t) * ptelock_hashtab_size;
	ptelock_hashtab = (ptelock_hash_t *)nucleus_zalloc(sz, 1, "ppcmmu_init2", "ptelock_hashtab[]");
	ptelock_hash_mask = ptelock_hashtab_size - 1;
}

/*
 * Allocate ppcmmu specific hat data for this address space.
 * The only resource that needs to be allocated is
 * vsid-range-id for this address space.
 */
static void
ppcmmu_alloc(hat_t *hat)
{
	struct vsid_alloc_set *p;
	static int vsid_gc_initialized = 0;

	if (vsid_valid(hat->hat_vsidr))
		return;

	mutex_enter(&ppcmmu_res_lock);

	p = vsid_alloc_head;
	if (p != NULL) {
		ASSERT(p->vs_nvsid_ranges != 0);
		hat->hat_vsidr = p->vs_vsid_range_id;
		ASSERT(!IS_VSID_RANGE_INUSE(p->vs_vsid_range_id));
		SET_VSID_RANGE_INUSE(p->vs_vsid_range_id);
		--(p->vs_nvsid_ranges);
		++(p->vs_vsid_range_id);
		/*
		 * If this set is empty then free the set structure.
		 */
		if (p->vs_nvsid_ranges == 0) {
			vsid_alloc_head = p->vs_next;
			kmem_free(p, sizeof (struct vsid_alloc_set));
		}
	} else {
		/* No free VSID range to use, let the thread fault on it. */
		hat->hat_vsidr = (uint_t)VSIDRANGE_INVALID;
		++ppcmmu_vsid_wanted;
	}

	/*
	 * If we allocated the last vsid_range then wakeup the kernel thread
	 * ppcmmu_vsid_gc().
	 */
	if (vsid_alloc_head == NULL) {
		if (vsid_gc_initialized)
			cv_broadcast(&ppcmmu_cv);
		else {
			/*
			 * Create the vsid_gc thread and run it.
			 * XXX PPC check the thread priority?
			 */
			if (thread_create(NULL, NULL, ppcmmu_vsid_gc,
			    NULL, 0, &p0, TS_RUN, minclsyspri) == NULL) {
				panic("ppcmmu_alloc: thread_create");
			}
			vsid_gc_initialized = 1;
		}
		VMHATSTAT_INCR(vh_vsid_gc_wakeups);
	}
	mutex_exit(&ppcmmu_res_lock);
}

/*
 * Free all the translation resources for the specified address space.
 * Called from as_free when an address space is being destroyed.
 *
 * On PowerPC the VSID range is unique and by invalidating its use the
 * translations are ineffective. The kernel thread ppcmmu_vsid_gc()
 * unloads the stale translations before the VSID range can be reused.
 */
void
ppcmmu_free(hat_t *hat)
{
	mutex_enter(&hat->hat_mutex);
	mutex_enter(&ppcmmu_res_lock);
	ASSERT(vsid_valid(hat->hat_vsidr));
	CLEAR_VSID_RANGE_INUSE(hat->hat_vsidr);
	hat->hat_freeing = 1;	/* mark that ppcmmu_free() is called */
	mutex_exit(&ppcmmu_res_lock);
	mutex_exit(&hat->hat_mutex);
}

/*
 * Set up vaddr to map to page pp with protection prot.
 */
/* ARGSUSED */
void
ppcmmu_memload(hat_t *hat, caddr_t vaddr, page_t *pp, uint_t attr, int flags)
{
	kmutex_t *pml;
	pte_t apte;

	ppcmmu_mempte(hat, pp, attr & HAT_PROT_MASK, &apte, vaddr);
	pml = ppcmmu_mlist_enter(pp);
	mutex_enter(&hat->hat_mutex);
	ppcmmu_pteload(hat, vaddr, pp, &apte, flags, attr);
	mutex_exit(&hat->hat_mutex);
	ppcmmu_mlist_exit(pml);
}

/*
 * Memory:     WIM = 001, G = 0, WIMG = 0x2
 * Not memory: WIM = 011, G = 1, WIMG = 0x7
 * See MPCFPE32B, page 5-18, 5-19.
 * WIM = 001:
 *	Data (or instructions) may be cached.
 *	A load or store operation whose target hits in the cache
 *	may use that entry in the cache.
 *	The processor enforces memory coherency for access it initiates.
 * WIM = 011:
 *	Caching is inhibited.
 *	The access is performed to memory, bypassing the cache.
 *	The processor enforces memory coherency for access it initiates.
 */

static __inline__ uint_t
pfn_to_wimg(pfn_t pfn)
{
	if (pf_is_memory(pfn))
		return (0x2);
	return (0x7);
}

/*
 * Cons up a PTE using the device's pf bits and protection
 * prot to load into the hardware for address vaddr; treat as minflt.
 *
 * Note: hat_devload can be called to map real memory (e.g.
 * /dev/kmem) and even though hat_devload will determine pf is
 * for memory, it will be unable to get a shared lock on the
 * page (because someone else has it exclusively) and will
 * pass dp = NULL.  If ppcmmu_pteload() doesn't get a non-NULL
 * page pointer it can't cache memory.  Since all memory must
 * always be cached for PPC, we call pf_is_memory() to cover
 * this case.
 */

/* ARGSUSED */
void
ppcmmu_devload(hat_t *hat, caddr_t vaddr, devpage_t *dp, pfn_t pfn, uint_t attr, int flags)
{
	kmutex_t *pml;
	pte_t pte;
	uint_t vsid = VSID(hat->hat_vsidr, vaddr);
	uint_t api = VADDR_TO_API(vaddr);
	uint_t wimg = pfn_to_wimg(pfn);
	uint_t vprot = attr & HAT_PROT_MASK;
	uint_t pte_pp = ppcmmu_vtop_prot(vaddr, vprot);

	if (pfn > PTE1_RPN_VAL_MASK) {
		panic("ppcmmu_devload: pfn=%lx is too big (> %lx)",
			pfn, (pfn_t)PTE1_RPN_VAL_MASK);
	}

	// v, vsid, h, api
	pte.ptew[PTEWORD0] = PTE0_NEW(PTE_VALID, vsid, 0, api);
	// rpn, xpn, r, c, wimg, x, pp
	pte.ptew[PTEWORD1] = PTE1_NEW(pfn, 0, 0, 0, wimg, 0, pte_pp);

	if (dp != NULL)
		pml = ppcmmu_mlist_enter(dp);
	mutex_enter(&hat->hat_mutex);
	ppcmmu_pteload(hat, vaddr, (page_t *)dp, &pte, flags, attr);
	mutex_exit(&hat->hat_mutex);
	if (dp != NULL)
		ppcmmu_mlist_exit(pml);
}

#if defined(XXX_EXPERIMENTAL)
/*
 * Set up translations to a range of virtual addresses
 * backed by physically contiguous memory.
 *
 * Note: We can use this for BAT mappings.
 */
void
ppcmmu_contig_memload(hat_t *hat, caddr_t vaddr, page_t *pp, uint_t attr, int flags, uint_t len)
{
	kmutex_t *pml;
	pte_t apte;

	ASSERT(check_va_range(hat, vaddr, len));
	while (len) {
		ppcmmu_mempte(hat, pp, attr & HAT_PROT_MASK, &apte, vaddr);
		pml = ppcmmu_mlist_enter(pp);
		mutex_enter(&hat->hat_mutex);
		ppcmmu_pteload(hat, vaddr, pp, &apte, flags, attr);
		mutex_exit(&hat->hat_mutex);
		ppcmmu_mlist_exit(pml);
		pp = page_next(pp);
		vaddr += MMU_PAGESIZE;
		len -= MMU_PAGESIZE;
	}
}

#endif /* defined(XXX_EXPERIMENTAL) */

/*
 * Release one hardware address translation lock on the given address.
 * This means clearing the lock bit(s) for the translation(s) in the PTEGP(s).
 */
static void
ppcmmu_unlock(hat_t *hat, caddr_t vaddr, uint_t len)
{
	pte_t *pte;
	ptegp_t *ptegp;
	caddr_t a;

	ASSERT(check_va_range(hat, vaddr, len));
	mutex_enter(&hat->hat_mutex);
	for (a = vaddr; a < vaddr + len; a += MMU_PAGESIZE) {

		pte = ppcmmu_ptefind(hat, a, PTEGP_LOCK);
		if (pte == NULL) {
			mutex_exit(&hat->hat_mutex);
			panic("ppcmmu_unlock: null pte");
		}
		/*
		 * Decrement the lock count for this mapping.
		 */
		ptegp = pte_to_ptegp(pte);
		pteunlock(ptegp, pte);
		mutex_exit(&ptegp->ptegp_mutex);
	}
	mutex_exit(&hat->hat_mutex);
}

/*
 * Change the protections in the virtual address range given to the
 * specified virtual protection.
 *
 * If vprot is ~PROT_WRITE, then remove write permission,
 * leaving the other permissions unchanged.
 * If vprot is ~PROT_USER, remove user permissions.
 *
 * Note: This function needs changes to support BAT mappings.
 */

void
ppcmmu_chgprot(hat_t *hat, caddr_t vaddr, uint_t len, uint_t vprot)
{
	pte_t *pte;
	uint_t pte_prot;
	uint_t pprot;
	caddr_t a, ea;
	ptegp_t *ptegp;
	int newprot = 0;

	ASSERT(vsid_valid(hat->hat_vsidr));

	mutex_enter(&hat->hat_mutex);

	/*
	 * Convert the virtual protections to physical ones.  We can do
	 * this once with the first address because the kernel won't be
	 * in the same segment with the user, so it will always be
	 * one or the other for the entire length.  If vprot is ~PROT_WRITE,
	 * turn off write permission.
	 */
	if (vprot != (~PROT_USER) && vprot != (~PROT_WRITE) && vprot != 0)
		pprot = ppcmmu_vtop_prot(vaddr, vprot);

	for (a = vaddr, ea = vaddr + len; a < ea; a += MMU_PAGESIZE) {
		/* Find the PTE and get the PTEGP mutex */
		if ((pte = ppcmmu_ptefind(hat, a, PTEGP_LOCK)) == NULL)
			continue;
		ptegp = pte_to_ptegp(pte);
		pte_prot = ptep_get_pp(pte);
		if (vprot == (~PROT_WRITE)) {
			switch (pte_prot) {
			case MMU_STD_SRWXURWX:
				pprot = MMU_STD_SRXURX;
				newprot = 1;
				break;
			case MMU_STD_SRWX:
				/*
				 * On PPC, we can't write protect kernel
				 * without giving read access to user.
				 */
				newprot = 0;
				break;
			}
		} else if (vprot == (~PROT_USER)) {
			switch (pte_prot) {
			case MMU_STD_SRWXURWX:
			case MMU_STD_SRXURX:
			case MMU_STD_SRWXURX:
				pprot = MMU_STD_SRWX;
				newprot = 1;
				break;
			}
		} else if (pte_prot != pprot)
			newprot = 1;
		if (newprot) {
			mmu_update_pte_prot(pte, pprot, a);
			newprot = 0;
		}
		/*
		 * On the machines with split I/D caches,
		 * we need to flush the I-cache for this page.
		 * The algorithm used to determine flushing:
		 *	if new protection specifies PROT_EXEC then
		 *		if page is marked 'not-flushed' then
		 *			do 'dcbst' and 'icbi' on the page.
		 *			mark the page as 'flushed'
		 *	if new protection specifies PROT_WRITE then
		 *		mark the page as 'not-flushed'
		 */
		if (!unified_cache) {
			page_t *pp;

			pp = ptep_to_pp(pte);
			ASSERT(pte_to_hme(pte)->hme_page == pp);
			if (pp) {
				if ((vprot & PROT_EXEC) &&
					!PAGE_IS_FLUSHED(pp)) {
					ppcmmu_cache_flushpage(hat, a);
					SET_PAGE_FLUSHED(pp);
				}
				if (vprot & PROT_WRITE)
					CLR_PAGE_FLUSHED(pp);
			}
		}
		mutex_exit(&ptegp->ptegp_mutex);
	}

	mutex_exit(&hat->hat_mutex);
}

/*
 * Unload all the mappings in the range [vaddr..vaddr+len).
 * vaddr and len must be MMU_PAGESIZE aligned.
 *
 * Algorithm:
 *	Get hat mutex.
 *	Loop:
 *		Search for PTE and get the ptegp mutex if found.
 *		Mark hme as BUSY (to avoid hme stealing).
 *		If pp is not NULL then:
 *			Release the ptegp mutex.
 *			Get the mlist lock.
 *			Get the ptegp_mutex lock.
 *		Call ppcmmu_pteunload() to unload the mapping.
 *		Release the ptegp_mutex lock.
 *		If pp is not NULL then release mlist lock.
 *	Release hat mutex.
 *
 */
void
ppcmmu_unload(hat_t *hat, caddr_t vaddr, uint_t len, int flags)
{
	pte_t *pte;
	caddr_t a, ea;
	ptegp_t *ptegp;
	hme_t *hme;
	int as_freeing = 0;
	page_t *pp;
	uint_t vsidrange;

	if (flags & HAT_UNLOAD_UNMAP)
		flags = (flags & ~HAT_UNLOAD_UNMAP) | HAT_UNLOAD;

	as_freeing = hat->hat_freeing; /* ppcmmu_free() called for this as? */
	vsidrange = hat->hat_vsidr;
	mutex_enter(&hat->hat_mutex);

	for (a = vaddr, ea = vaddr + len; a < ea; a += MMU_PAGESIZE) {
		/* Find the PTE and get the PTEGP mutex */
		pte = ppcmmu_ptefind(hat, a, PTEGP_LOCK);
		if (pte == NULL)
			continue;
		ptegp = pte_to_ptegp(pte);
		hme = pte_to_hme(pte);
		hme->hme_impl |= HME_BUSY;

		if ((flags & HAT_UNLOAD_UNLOCK) && (ptep_isvalid(pte))) {
			pteunlock(ptegp, pte);
		}
		/*
		 * If we are trying to unload mapping(s) for an 'as' for
		 * which ppcmmu_free() is already done then make sure we
		 * are looking at the PTE that belongs to this 'as'.
		 */
		if (as_freeing && (hat != &hats[hme->hme_hat])) {
			mutex_exit(&ptegp->ptegp_mutex);
			continue;
		}
		pp = hme->hme_page;
		flags |= hme->hme_nosync ? PPCMMU_NOSYNC : PPCMMU_RMSYNC;
		if (pp) {
			kmutex_t *pml;

			/*
			 * Acquire the locks in the correct order.
			 */
			mutex_exit(&ptegp->ptegp_mutex);
			mutex_exit(&hat->hat_mutex);
			pml = ppcmmu_mlist_enter(pp);
			mutex_enter(&hat->hat_mutex);
			mutex_enter(&ptegp->ptegp_mutex);
			/*
			 * Make sure the mapping hasn't been removed/replaced
			 * while we were trying to get the locks.
			 */
			if (ptep_isvalid(pte) && ((ptep_get_vsid(pte) >>
				VSIDRANGE_SHIFT) == vsidrange) &&
				(!as_freeing || (hat == &hats[hme->hme_hat])))
				ppcmmu_pteunload(ptegp, pte, pp, hme, flags);
			mutex_exit(&ptegp->ptegp_mutex);
			ppcmmu_mlist_exit(pml);
		} else {
			ppcmmu_pteunload(ptegp, pte, pp, hme, flags);
			mutex_exit(&ptegp->ptegp_mutex);
		}
	}

	mutex_exit(&hat->hat_mutex);
}

/*
 * Unload a hardware translation that maps page `pp'.
 */
static void
ppcmmu_pageunload(page_t *pp, hme_t *hme)
{
	pte_t *pte;
	hat_t *hat;
	ptegp_t *ptegp;

	ASSERT(ppcmmu_mlist_held(pp));

	hat = &hats[hme->hme_hat];
	mutex_enter(&hat->hat_mutex);
	hme->hme_impl |= HME_BUSY;
	ptegp = hme_to_ptegp(hme);
	pte = hme_to_pte(hme);
	mutex_enter(&ptegp->ptegp_mutex);
	ppcmmu_pteunload(ptegp, pte, pp, hme, PPCMMU_RMSYNC);
	mutex_exit(&ptegp->ptegp_mutex);
	mutex_exit(&hat->hat_mutex);
}


/*
 * Get all the hardware dependent attributes for a page_t.
 */
static void
ppcmmu_pagesync(hat_t *hat, page_t *pp, hme_t *hme, uint_t clearflag)
{
	pte_t *pte;
	ptegp_t *ptegp;
	uint_t sync_flags = clearflag ? PPCMMU_RMSYNC : PPCMMU_RMSTAT;

	ASSERT(ppcmmu_mlist_held(pp));

	mutex_enter(&hat->hat_mutex);
	pte = hme_to_pte(hme);
	ptegp = pte_to_ptegp(pte);
	/*
	 * If the mapping is stale, then unload the mapping.
	 * A mapping can go stale if ppcmmu_free() didn't unload it,
	 * but the page scanner is checking it.
	 */
	mutex_enter(&ptegp->ptegp_mutex);
	if (!IS_VSID_RANGE_INUSE(hat->hat_vsidr))
		ppcmmu_pteunload(ptegp, pte, pp, hme, sync_flags);
	else
		ppcmmu_ptesync(ptegp, pte, hme, sync_flags);
	mutex_exit(&ptegp->ptegp_mutex);
	mutex_exit(&hat->hat_mutex);
}

/*
 * Returns a page frame number for a given kernel virtual address.
 *
 * Return -1 to indicate an invalid mapping (needed by kvtoppid)
 */
static pfn_t
ppcmmu_getkpfnum(caddr_t vaddr)
{
	batinfo_t *bat;
	extern batinfo_t *ppcmmu_find_bat(caddr_t);

#if defined(XXX_OBSOLETE_SOLARIS)
	struct spte *spte;

	/*
	 * Use kmap_ptes a la usr/src/uts/i86pc/vm/hat_i86.c in on11.
	 * Sysbase, Syslimit and Sysmap are obsolete
	 */
	if (vaddr >= Sysbase && vaddr < Syslimit) {
		spte = &Sysmap[mmu_btop(vaddr - Sysbase)];
		if (spte_valid(spte))
			return (spte->spte_ppn);
	}
#endif /* defined(XXX_OBSOLETE_SOLARIS) */

	/* Search the BAT mappings first */
	bat = ppcmmu_find_bat(vaddr);
	if (bat)
		return ((uintptr_t)(bat->bat_paddr +
			(vaddr - bat->bat_vaddr)) >> MMU_PAGESHIFT);
	else
		/* No BAT mapping found, search in the Page Table */
		return (ppcmmu_getpfnum(&khat, vaddr));
}

/*
 * ppcmmu_getpfnum(hat, vaddr)
 *	Given the address of an address space returns the physcial page number
 *	of that address.  Returns -1 if no mapping found.
 *
 * Note: Needs work to support BAT mappings.
 */
static pfn_t
ppcmmu_getpfnum(hat_t *hat, caddr_t vaddr)
{
	pte_t *pte;
	uint_t pfn;

	mutex_enter(&hat->hat_mutex);
	pte = ppcmmu_ptefind(hat, vaddr, PTEGP_LOCK);
	if (pte) {
		/* Valid mapping found */
		// XXX Get full pfn, not just rpn field
		// XXX Need more complicated code to construct
		// XXX full pfn from the various extension bits
		pfn = ptep_get_rpn(pte);
		mutex_exit(&pte_to_ptegp(pte)->ptegp_mutex);
	} else {
		pfn = PFN_INVALID;
	}
	mutex_exit(&hat->hat_mutex);
	return (pfn);
}

/*
 * End of machine independent interface routines.
 *
 * The next few routines implement some machine dependent functions
 * needed for the PPCMMU. Note that each hat implementation can define
 * whatever additional interfaces make sense for that machine.
 *
 * Start machine specific interface routines.
 */

/*
 * Called by UNIX during pagefault to insure that the hat data structures are
 * setup for this address space
 *
 * If the address space didn't have a valid vsid-range (VSIDRANGE_INVALID)
 * and allocflag is set then we try to allocate a vsid-range for this
 * address space.  And reload the segment registers for the new VSID.
 */
void
ppcmmu_setup(hat_t *hat, int allocflag)
{
	/*
	 * kas is already set up, it is part of every address space,
	 * so we can stay in whatever context that is currently active.
	 */
	if (IS_KERNEL_HAT(hat))
		return;

	ASSERT(MUTEX_HELD(&hat->hat_mutex));

	if (!vsid_valid(hat->hat_vsidr) && allocflag)
		ppcmmu_alloc(hat);

	kpreempt_disable();
	/* Reload segment registers for this address space */
	mmu_segload(hat->hat_vsidr);
	kpreempt_enable();
}

void
ppcmmu_getpte(caddr_t vaddr, pte_t *rpte)
{
	pte_t *pte;
	struct proc *p;
	hat_t *hat;

	p = curproc;
	hat = p->p_as->a_hat;
	hat_enter(p->p_as->a_hat);
	pte = ppcmmu_ptefind(hat, vaddr, PTEGP_NOLOCK);
	if (pte == NULL)
		*(u_longlong_t *)rpte = MMU_INVALID_PTE;
	else
		*rpte = *pte;
	hat_exit(p->p_as->a_hat);
}

/*
 * Returns a pointer to the PTE if there is a valid mapping
 * for the given fully-qualified virtual address.
 * Also if the 'lockneeded' flag is TRUE,
 * then it returns with the PTEGP mutex held.
 * It returns NULL if no valid mapping exists.
 */

static pte_t *
fqva_ptefind(uint_t vsid, caddr_t vaddr, uint_t lockflag)
{
	pte_t *primary_pteg;
	pte_t *secondary_pteg;
	uint_t api;
	pte_t *pte;
	ptegp_t *ptegp;
	uint_t hash1;

	/* Compute the primary hash value */
	hash1 = fqva_to_hash1(vsid, vaddr);
	api = (((uintptr_t)vaddr >> APISHIFT) & APIMASK);
	primary_pteg = hash_get_primary(hash1);
	ptegp = pte_to_ptegp(primary_pteg);

	mutex_enter(&ptegp->ptegp_mutex);
	/* Search the primary PTEG for the mapping */
	pte = find_pte(primary_pteg, vsid, api, H_PRIMARY);
	if (pte == NULL) {
		/* No mapping in the primary PTEG. Search the secondary PTEG */
		secondary_pteg = hash_get_secondary(primary_pteg);
		pte = find_pte(secondary_pteg, vsid, api, H_SECONDARY);
	}
	if ((pte == NULL) || (lockflag == PTEGP_NOLOCK))
		mutex_exit(&ptegp->ptegp_mutex);
	return (pte);
}

/*
 * Returns a pointer to the PTE if there is a valid mapping
 * for the given {hat, va}.
 * Also if the 'lockneeded' flag is TRUE,
 * then it returns with the PTEGP mutex held.
 * It returns NULL if no valid mapping exists.
 */

static pte_t *
ppcmmu_ptefind(hat_t *hat, caddr_t vaddr, uint_t lockflag)
{
	uint_t vsid;

	ASSERT(MUTEX_HELD(&hat->hat_mutex));

	/*
	 * If the address space doesn't have a valid VSID then return.
	 */
	if (!vsid_valid(hat->hat_vsidr)) {
		return (NULL);
	}

	/* Search the primary PTEG */
	vsid = (hat->hat_vsidr << VSIDRANGE_SHIFT) + va_to_sr(vaddr);

	return (fqva_ptefind(vsid, vaddr, lockflag));
}

/*
 * This routine converts virtual page protections to physical ones.
 */
static uint_t
ppcmmu_vtop_prot(caddr_t vaddr, uint_t vprot)
{
	if ((vprot & PROT_ALL) != vprot) {
		panic("ppcmmu_vtop_prot -- bad vprot %x", vprot);
	}

	if (vprot & PROT_USER) { /* user permission */
		if (vaddr >= (caddr_t)KERNELBASE) {
			panic("user vaddr %p vprot %x in kernel space\n",
				vaddr, vprot);
		}
	}

	switch (vprot) {
	case PROT_READ:
		/*
		 * XXX PPC TEMPORARY WORKAROUND FOR UFS_HOLE PROBLEM.
		 *
		 * If the page is for read-only and it is in the segkmap
		 * segment (used by filesystem code) to map HOLEs in the
		 * file with page of zeros and expect write faults then
		 * the best we can do is to give kernel-read-user-read
		 * protection on this page.
		 */
		if (vaddr >= segkmap->s_base &&
			(vaddr < (segkmap->s_base + segkmap->s_size)))
			return (MMU_STD_SRXURX);
		/*FALLTHROUGH*/
	case 0:
	case PROT_USER:
		/* Best we can do for these is kernel access only */
	case PROT_EXEC:
	case PROT_WRITE:
	case PROT_READ | PROT_EXEC:
	case PROT_WRITE | PROT_EXEC:
	case PROT_READ | PROT_WRITE:
	case PROT_READ | PROT_WRITE | PROT_EXEC:
		return (MMU_STD_SRWX);
	case PROT_EXEC | PROT_USER:
	case PROT_READ | PROT_USER:
	case PROT_READ | PROT_EXEC | PROT_USER:
		return (MMU_STD_SRXURX);
	case PROT_WRITE | PROT_USER:
	case PROT_READ | PROT_WRITE | PROT_USER:
	case PROT_WRITE | PROT_EXEC | PROT_USER:
	case PROT_READ | PROT_WRITE | PROT_EXEC | PROT_USER:
		return (MMU_STD_SRWXURWX);
	default:
		panic("ppcmmu_vtop_prot: bad vprot %x", vprot);
	}
}

static uint_t vprot_table[4] = {
	PROT_READ | PROT_WRITE | PROT_EXEC,		/* MMU_STD_SRWX */
	PROT_READ | PROT_WRITE | PROT_EXEC | PROT_USER,	/* MMU_STD_SRWXURX */
	PROT_READ | PROT_WRITE | PROT_EXEC | PROT_USER, /* MMU_STD_SRWXURWX */
	PROT_READ | PROT_EXEC | PROT_USER		/* MMU_STD_SRXURX */
};

static uint_t
ppcmmu_ptov_prot(uint_t pprot)
{
	if (pprot == 0)
		return (0);

	ASSERT(pprot < 4);
	return (vprot_table[pprot]);
}

/* ARGSUSED */
static int
ppcmmu_probe(hat_t *hat, caddr_t vaddr)
{
	pte_t *pte;

	if (vaddr >= (caddr_t)KERNELBASE)
		return (ppcmmu_getkpfnum(vaddr) == -1 ? 0 : 1);

	mutex_enter(&hat->hat_mutex);
	pte = ppcmmu_ptefind(hat, vaddr, PTEGP_NOLOCK);
	mutex_exit(&hat->hat_mutex);
	return (pte != NULL);
}

/*
 * Construct a PTE for a page.
 */
static void
ppcmmu_mempte(hat_t *hat, page_t *pp, uint_t vprot, pte_t *pte, caddr_t vaddr)
{
	pfn_t pfn;

	ASSERT(PAGE_LOCKED(pp) || panicstr);

	if (pfn > PTE1_RPN_VAL_MASK) {
		panic("ppcmmu_mempte: pfn=%lx is too big (> %lx)",
			pfn, (pfn_t)PTE1_RPN_VAL_MASK);
	}
	// v, vsid, h, api
	pte->ptew[PTEWORD0] = PTE0_NEW(PTE_VALID, VSID(hat->hat_vsidr, vaddr), 0, VADDR_TO_API(vaddr));
	// rpn, xpn, r, c, wimg, x, pp
	pte->ptew[PTEWORD1] = PTE1_NEW(pfn, 0, 0, 0, pfn_to_wimg(pfn), 0, ppcmmu_vtop_prot(vaddr, vprot));
}

/*
 * Loads the PTE for the given address with the given pte.
 * Also sets it up as a mapping for page pp, if there is one.
 * The lock bit in the PTEGP structure is set
 * if the translation is to be locked.
 *
 * Locking:
 *	1. At the entry hat_mutex and mlist lock (if pp != NULL) are
 *	   already held.
 *	2. ptegp mutex is held until we enter the new mapping.
 */
static void
ppcmmu_pteload(hat_t *hat, caddr_t vaddr, page_t *pp, pte_t *new_pte, int flags, uint_t attr)
{
	kmutex_t *pml;
	pte_t *pte;
	pte_t *ppteg;
	pte_t *spteg;
	ptegp_t *ptegp;
	hme_t *hme;
	int remap = 0;
	int hbit;
	uint_t hash1;
	int do_flush = 0;
	uint_t prot = attr & HAT_PROT_MASK;

	ASSERT(MUTEX_HELD(&hat->hat_mutex));
	ASSERT((pp == NULL) || ppcmmu_mlist_held(pp));

	VMHATSTAT_INCR(vh_pteload);

	if (pp)
		pml = ppcmmu_mlist_mtx(pp);

try_again:
	/*
	 * Make sure the vsid-range is setup for the address space.
	 */
	if (!vsid_valid(hat->hat_vsidr)) {
		ppcmmu_alloc(hat);
		if (!vsid_valid(hat->hat_vsidr)) {
			/*
			 * Can't allocate VSID now.
			 * Wait until vsid_gc tells us to try again.
			 */
			if (pp)
				ppcmmu_mlist_exit(pml);
			cv_wait(&ppcmmu_vsid_cv, &hat->hat_mutex);
			if (pp) {
				mutex_exit(&hat->hat_mutex);
				pml = ppcmmu_mlist_enter(pp);
				mutex_enter(&hat->hat_mutex);
			}
			goto try_again;
		}
		ppcmmu_setup(hat, 0);
	}

	ptep_set_vsid(new_pte, VSID(hat->hat_vsidr, vaddr));

	pte = ppcmmu_ptefind(hat, vaddr, PTEGP_LOCK);

	if (pte) {
		hme = pte_to_hme(pte);
		remap = 1;
		ASSERT(hme->hme_valid);
		ptegp = pte_to_ptegp(pte);
	}

	if (pp) {
		// XXX Upgrade WIMG() macro; no mention of mmu601
		ptep_set_wimg(new_pte, WIMG(!PP_ISNC(pp), 1));
		if (remap && ptep_get_rpn(pte) != ptep_get_rpn(new_pte)) {
			mutex_exit(&ptegp->ptegp_mutex);
			mutex_exit(&hat->hat_mutex);
			panic("ppcmmu_pteload: remap page");
		}
		/*
		 * For split I/D caches we may need to flush the I-cache.
		 * Determine if we need to do this according to the
		 * algorithm.
		 *	if this is the first mapping to the page then
		 *		if mapping with PROT_EXEC then
		 *			do_flush = 1.
		 *		else
		 *			mark the page as 'not-flushed'
		 *	Else if mapping PROT_EXEC and page not flushed
		 *	before then
		 *		do_flush = 1
		 */
		if (!unified_cache) {
			if (pp->p_mapping == NULL) {
				if (prot & PROT_EXEC)
					do_flush = 1;
				else
					CLR_PAGE_FLUSHED(pp);
			} else {
				if ((prot & PROT_EXEC) &&
					!(PAGE_IS_FLUSHED(pp)))
					do_flush = 1;
			}
		}
	}

	if (remap == 0) {
		/*
		 * New mapping. Try to find a free slot in primary PTEG.
		 * If no free slot found then try in the secondary PTEG.
		 * If secondary PTEG is also full then try to steal a slot
		 * from the primary PTEG and if that fails try to steal
		 * a slot from secondary PTEG.
		 */
		uint_t vsid;

		vsid = ptep_get_vsid(new_pte);
		hash1 = fqva_to_hash1(vsid, vaddr);
		ppteg = hash_get_primary(hash1); /* primary PTEG address */
		ptegp = pte_to_ptegp(ppteg);
		mutex_enter(&ptegp->ptegp_mutex);
		pte = find_free_pte(hat, ppteg, ptegp, pp);
		if (pte)
			hbit = H_PRIMARY;
		else {
			spteg = hash_get_secondary(ppteg);
			pte = find_free_pte(hat, spteg, ptegp, pp);
			if (pte)
				hbit = H_SECONDARY;
			else {
				/* No free slot in either PTEG, steal one */
				pte = steal_pte(hat, ppteg, ptegp);
				if (pte) {
					hbit = H_PRIMARY;
				} else {
					/* Try to steal from secondary PTEG */
					pte = steal_pte(hat, spteg, ptegp);
					hbit = H_SECONDARY;
				}
				if (!pte) {
					/*
					 * Could not steal a slot.
					 * Panic or try again?
					 */
					mutex_exit(&ptegp->ptegp_mutex);
					mutex_exit(&hat->hat_mutex);
					panic("ppcmmu_pteload: "
						"PTE overflow!!!");
				}
				VMHATSTAT_INCR(vh_pteoverflow);
			}
		}
		ptep_set_h(new_pte, hbit);
		hme = pte_to_hme(pte);
		ASSERT((hme->hme_valid == 0) && (hme->hme_next != hme));
	} else {
		/* Remap.  Copy h bit from existing PTE. */
		ptep_set_h(new_pte, ptep_get_h(pte));
	}

	/* Update the PTE. */
	mmu_update_pte(pte, new_pte, remap, vaddr);

	/* Do the I-cache flush if necessary */
	if (do_flush) {
		ppcmmu_cache_flushpage(hat, vaddr);
		if (prot & PROT_WRITE)
			/*
			 * As long as the page is writable,
			 * mark the page as 'not-flushed'.
			 */
			CLR_PAGE_FLUSHED(pp);
		else
			SET_PAGE_FLUSHED(pp);
	}

	/*
	 * If we need to lock, increment the lock count in the lockmap
	 * and/or update the hash table entry.
	 */
	if (flags & HAT_LOAD_LOCK)
		ptelock(ptegp, pte);

	if (!remap) {
		int mask;

		mask = ((pte >= mptes) ? XXX_MAGIC(0x100) : 1);
		mask <<= ((uintptr_t)pte & PTEOFFSET) >> PTESHIFT;
		ptegp->ptegp_validmap |= mask; /* Update validmap bitmap */
	}

	if (pp != NULL && !remap) {
		hme_add(hme, pp);
		atomic_add_32(&hat->hat_rss, 1);
	}
	if (!remap) {
		hme->hme_hat = hat - hats;
		hme->hme_valid = 1;
	}
	hme->hme_impl &= ~HME_BUSY;

	/*
	 * Note whether this translation will be ghostunloaded or not.
	 */
	hme->hme_nosync = ((attr & HAT_NOSYNC) != 0);

	mutex_exit(&ptegp->ptegp_mutex);
}

/*
 * ------------------------------------------------------------------------
 *		All things related to BATs
 * ------------------------------------------------------------------------
 */

// XXX Probably should be in mmuinfo structure.
//
// XXX Probably should keep track of high water mark of BAT usage
// XXX or a bitmap of BATs in use.
//
// XXX Better yet would be to keep track of BATs in use for
// XXX each large va-region, separately.  Also, segregate BAT
// XXX usage by size, and do linear search for very large mappings
// XXX and index by va-region for any smaller BAT mappings.
// XXX For example, BAT mappings larger than 256M.

/*
 * MAX_IBATS and MAX_DBATS are constants used for capacity planning,
 * and represent the maximum supportable by any known processor,
 * and the most that we know how to address.  The variables,
 * maxibats, maxdbats, maxbats, are "discovered" at run-time, and
 * are the maximum supported by the running processor, the way it
 * is configured at the time of cpu identification and configuration.
 *
 * Configuration of max*bats is done once, during startup.
 * No attempt is ever made to reconfigure, later.
 *
 * They will always be <= the corresponding MAX_* constants.
 */

batinfo_t ibat_array[MAX_IBATS];
batinfo_t dbat_array[MAX_DBATS];

bat_table_t ibat_table = { ibat_array, IBAT_T, 0, "IBAT" };
bat_table_t dbat_table = { dbat_array, DBAT_T, 0, "DBAT" };

uint_t maxibats, maxdbats, maxbats;

/*
 * Configure BAT registers.
 * Determine actual number of IBATs and DBATs available for use,
 * according to processor capabilities and current settings,
 * such as HID0[HIGH_BAT_EN].  If the CPU capabilities indicate
 * that this processor has the potential for more than the basic
 * 4 IBATs and 4 DBATs, and the current state (in HID0) is that
 * they are enabled, then use more BATs.
 */

void
ppcmmu_bat_config(void)
{
	if (HID0_GET_HBEN(hid0)) {
		maxibats = MAX_IBATS;
		maxdbats = MAX_DBATS;
	} else {
		maxibats = 4;
		maxdbats = 4;
	}
	maxbats = maxibats + maxdbats;
	ibat_table.bt_size = maxibats;
	dbat_table.bt_size = maxdbats;
}

/*
 * Return the value of BAT Upper for the specified IBAT number.
 * Returns 0 for invalid BAT number.
 */

uint_t
ppcmmu_get_ibat_u(uint_t ibat)
{
	switch (ibat) {
	case 0:
		return (mfspr(IBAT0U));
	case 1:
		return (mfspr(IBAT1U));
	case 2:
		return (mfspr(IBAT2U));
	case 3:
		return (mfspr(IBAT3U));
	case 4:
		return (mfspr(IBAT4U));
	case 5:
		return (mfspr(IBAT5U));
	case 6:
		return (mfspr(IBAT6U));
	case 7:
		return (mfspr(IBAT7U));
	}
	return (0);
}

/*
 * Return the value of BAT Lower for the specified IBAT number.
 * Returns 0 for invalid BAT number.
 */

uint_t
ppcmmu_get_ibat_l(uint_t ibat)
{
	switch (ibat) {
	case 0:
		return (mfspr(IBAT0L));
	case 1:
		return (mfspr(IBAT1L));
	case 2:
		return (mfspr(IBAT2L));
	case 3:
		return (mfspr(IBAT3L));
	case 4:
		return (mfspr(IBAT4L));
	case 5:
		return (mfspr(IBAT5L));
	case 6:
		return (mfspr(IBAT6L));
	case 7:
		return (mfspr(IBAT7L));
	}
	return (0);
}

/*
 * Return the value of BAT Upper for the specified DBAT number.
 * Returns 0 for invalid BAT number.
 */

uint_t
ppcmmu_get_dbat_u(uint_t dbat)
{
	switch (dbat) {
	case 0:
		return (mfspr(DBAT0U));
	case 1:
		return (mfspr(DBAT1U));
	case 2:
		return (mfspr(DBAT2U));
	case 3:
		return (mfspr(DBAT3U));
	case 4:
		return (mfspr(DBAT4U));
	case 5:
		return (mfspr(DBAT5U));
	case 6:
		return (mfspr(DBAT6U));
	case 7:
		return (mfspr(DBAT7U));
	}
	return (0);
}

/*
 * Return the value of BAT Lower for the specified DBAT number.
 * Returns 0 for invalid BAT number.
 */

uint_t
ppcmmu_get_dbat_l(uint_t dbat)
{
	switch (dbat) {
	case 0:
		return (mfspr(DBAT0L));
	case 1:
		return (mfspr(DBAT1L));
	case 2:
		return (mfspr(DBAT2L));
	case 3:
		return (mfspr(DBAT3L));
	case 4:
		return (mfspr(DBAT4L));
	case 5:
		return (mfspr(DBAT5L));
	case 6:
		return (mfspr(DBAT6L));
	case 7:
		return (mfspr(DBAT7L));
	}
	return (0);
}

/*
 * Given a pair of BAT SPR numbers (upper and lower for the same BAT register),
 * invalidate the BAT register.
 *
 * At minimum, set Vs and Vp to 0.
 * But, we may as well set the whole upper BAT to 0.
 */

static __inline__ void
ppcmmu_invalidate_bat(const uint_t spr_batu, const uint_t spr_batl)
{
	// Set Upper BAT register to all-zero bits
	mtspr(spr_batu, 0);
	// We don't need to modify the Lower BAT register
}

/*
 * Unmap the IBAT mapping for the specified IBAT number.
 */

void
ppcmmu_unmap_ibat(uint_t ibat)
{
	switch (ibat) {
	case 0:
		ppcmmu_invalidate_bat(IBAT0U, IBAT0L);
		return;
	case 1:
		ppcmmu_invalidate_bat(IBAT1U, IBAT1L);
		return;
	case 2:
		ppcmmu_invalidate_bat(IBAT2U, IBAT2L);
		return;
	case 3:
		ppcmmu_invalidate_bat(IBAT3U, IBAT3L);
		return;
	case 4:
		ppcmmu_invalidate_bat(IBAT4U, IBAT4L);
		return;
	case 5:
		ppcmmu_invalidate_bat(IBAT5U, IBAT5L);
		return;
	case 6:
		ppcmmu_invalidate_bat(IBAT6U, IBAT6L);
		return;
	case 7:
		ppcmmu_invalidate_bat(IBAT7U, IBAT7L);
		return;
	default:
		panic("Invalid IBAT register number, %d.\n", ibat);
	}
}

/*
 * Unmap the DBAT mapping for the specified DBAT number.
 */

void
ppcmmu_unmap_dbat(uint_t dbat)
{
	switch (dbat) {
	case 0:
		ppcmmu_invalidate_bat(DBAT0U, DBAT0L);
		return;
	case 1:
		ppcmmu_invalidate_bat(DBAT1U, DBAT1L);
		return;
	case 2:
		ppcmmu_invalidate_bat(DBAT2U, DBAT2L);
		return;
	case 3:
		ppcmmu_invalidate_bat(DBAT3U, DBAT3L);
		return;
	case 4:
		ppcmmu_invalidate_bat(DBAT4U, DBAT4L);
		return;
	case 5:
		ppcmmu_invalidate_bat(DBAT5U, DBAT5L);
		return;
	case 6:
		ppcmmu_invalidate_bat(DBAT6U, DBAT6L);
		return;
	case 7:
		ppcmmu_invalidate_bat(DBAT7U, DBAT7L);
		return;
	default:
		panic("Invalid DBAT register number, %d.\n", dbat);
	}
}

void
ppcmmu_bat_read(bat_table_t *bat_table, uint_t i)
{
	batinfo_t *bat;
	uint_t batl, batu;

	bat = bat_table->bt_array + i;
	ASSERT(bat_table->bt_type == IBAT_T || bat_table->bt_type == DBAT_T);
	ASSERT(i < bat_table->bt_size);
	if (bat_table->bt_type == IBAT_T)
		batu = ppcmmu_get_ibat_u(i);
	else
		batu = ppcmmu_get_dbat_u(i);
	if ((batu & (BATU_VS_FLD_MASK | BATU_VP_FLD_MASK)) == 0) {
		bat->bat_valid = 0;
		return;
	}
	if (bat_table->bt_type == IBAT_T)
		batl = ppcmmu_get_ibat_l(i);
	else
		batl = ppcmmu_get_dbat_l(i);
	bat->bat_valid = 1;
	// XXX Don't need separate type for each entry, anymore.
	bat->bat_type = bat_table->bt_type;
	bat->bat_size = (size_t)(BATU_GET_BL(batu) + 1) << 17;
	bat->bat_paddr = (caddr_t)(batl & BATL_BRPN_FLD_MASK);
	bat->bat_vaddr = (caddr_t)(batu & BATU_BEPI_FLD_MASK);
	bat->bat_vprot = ppcmmu_ptov_prot(BATL_GET_PP(batl));
#if defined(HAT_DEBUG)
	DPRINTF("%s%u={%x, %x}\n",
		bat_table->bt_desc, i, batu, batl);
	DPRINTF("     =");
	print_batu(batu);
	print_nl();
	DPRINTF("      ");
	print_batl(batl);
	print_nl();
#endif
}

/*
 * Read BAT information into bats[] array.
 * Be sure to read only the BATs that are actually available for use.
 */

void
ppcmmu_bat_read_all(void)
{
	uint_t i;

	/* Read all available IBATs */
	for (i = 0; i < maxibats; ++i)
		ppcmmu_bat_read(&ibat_table, i);

	/* Read all available DBATs */
	for (i = 0; i < maxdbats; ++i)
		ppcmmu_bat_read(&dbat_table, i);
}

/*
 * Search the specified software BAT info table for a valid mapping
 * for the given virtual address.
 */
batinfo_t *
ppcmmu_bat_table_find_bat(bat_table_t *bat_table, caddr_t vaddr)
{
	batinfo_t *bat;
	uint_t btsz;
	uint_t i;

	bat = bat_table->bt_array;
	btsz = bat_table->bt_size;
	for (i = 0; i < btsz; ++bat, ++i) {
		if (bat->bat_valid == 0)
			continue;
		if ((vaddr >= (caddr_t)bat->bat_vaddr) &&
			(vaddr < (bat->bat_vaddr + bat->bat_size))) {
			return (bat);
		}
	}

	return (NULL); /* No BAT mapping found */
}

/*
 * Search Instruction BAT mappings for a valid mapping
 * for the given virtual address.
 */
batinfo_t *
ppcmmu_find_ibat(caddr_t vaddr)
{
	return (ppcmmu_bat_table_find_bat(&ibat_table, vaddr));
}

/*
 * Search Data BAT mappings for a valid mapping
 * for the given virtual address.
 */
batinfo_t *
ppcmmu_find_dbat(caddr_t vaddr)
{
	return (ppcmmu_bat_table_find_bat(&dbat_table, vaddr));
}

/*
 * Search both Instruction and Data BAT mappings for a valid mapping
 * for the given virtual address.
 */
batinfo_t *
ppcmmu_find_bat(caddr_t vaddr)
{
	batinfo_t *bat;

	bat = ppcmmu_bat_table_find_bat(&ibat_table, vaddr);
	if (bat != NULL)
		return (bat);
	return (ppcmmu_bat_table_find_bat(&dbat_table, vaddr));
}

/*
 * Does one va range intersect another?
 *
 * Two va ranges intersect unless they are completely disjoint.
 * Beware of "fencepost" problems.  va ranges can kiss without
 * intersecting.  E.g. 10 bytes starting at 0 (0 .. 9) kisses
 * but does not intersect 10 bytes starting at 10.
 */

static __inline__ uint_t
range_intersect(caddr_t va1, size_t len1, caddr_t va2, size_t len2)
{
	if (va1 == va2)
		return (1);
	if (va1 > va2)
		return (va1 < va2 + len2);
	/* va2 > va1 */
	return (va2 < va1 + len1);
}

/*
 * Given a bat_table and va-range,
 * unload all mappings that intersect that range.
 */

void
ppcmmu_bat_table_unload_range(bat_table_t *bat_table, caddr_t vaddr, size_t len)
{
	batinfo_t *bat;
	uint_t btsz;
	uint_t i;

	bat = bat_table->bt_array;
	btsz = bat_table->bt_size;
	for (i = 0; i < btsz; ++bat, ++i) {
		if (!bat->bat_valid)
			continue;
		if (range_intersect(vaddr, len, bat->bat_vaddr, bat->bat_size)) {
			if (bat_table->bt_type == IBAT_T)
				ppcmmu_unmap_ibat(i);
			else
				ppcmmu_unmap_dbat(i);
			bat->bat_valid = 0;
		}
	}
}

void
ppcmmu_ibat_unload_range(caddr_t vaddr, size_t len)
{
	ppcmmu_bat_table_unload_range(&ibat_table, vaddr, len);
}

void
ppcmmu_dbat_unload_range(caddr_t vaddr, size_t len)
{
	ppcmmu_bat_table_unload_range(&dbat_table, vaddr, len);
}

void
ppcmmu_bat_unload_range(caddr_t vaddr, size_t len)
{
	ppcmmu_bat_table_unload_range(&ibat_table, vaddr, len);
	ppcmmu_bat_table_unload_range(&dbat_table, vaddr, len);
}


/*
 * End machine specific interface routines.
 *
 * The remainder of the routines are private to this module and are
 * used by the routines above to implement a service to the outside
 * caller.
 *
 * Start private routines.
 */

/*
 * Unload the PTE.  If required, sync the referenced & modified bits.
 */
static void
ppcmmu_pteunload(ptegp_t *ptegp, pte_t *pte, page_t *pp, hme_t *hme, int flags)
{
	hat_t *hat;
	int mask;
	int lkcnt_shift;
	int count;

	ASSERT(ptep_get_v(pte) != 0);
	ASSERT((pp == NULL) || ppcmmu_mlist_held(pp));
	ASSERT(hme->hme_next != hme);
	ASSERT(hme->hme_valid);
	ASSERT(MUTEX_HELD(&hats[hme->hme_hat].hat_mutex));
	ASSERT(MUTEX_HELD(&ptegp->ptegp_mutex));

	/*
	 * Sync ref and mod bits back to the page and invalidate PTE.
	 */
	ppcmmu_ptesync(ptegp, pte, hme, PPCMMU_INVSYNC | flags);

	ASSERT(!ptep_isvalid(pte));

	/*
	 * Remove the PTE from the list of mappings for the page.
	 */
	if (pp != NULL) {
		ASSERT(hme->hme_next != hme);
		hat = &hats[hme->hme_hat];
		atomic_add_32(&hat->hat_rss, -1);
		hme_sub(hme, pp);
	}
	hme->hme_valid = 0;
	hme->hme_nosync = 0;
	hme->hme_impl &= ~HME_BUSY;
	/*
	 * Update ptegp structure.
	 */
	mask = ((pte >= mptes) ? XXX_MAGIC(0x100) : 1);
	mask <<= ((uintptr_t)pte & PTEOFFSET) >> PTESHIFT;
	ptegp->ptegp_validmap &= ~mask; /* Update validmap bitmap */
	/* Update lock count bit map and delete any hash entry for this pte */
	if (pte >= mptes)
		lkcnt_shift = LKCNT_UPPER_SHIFT;
	else
		lkcnt_shift = LKCNT_LOWER_SHIFT;
	lkcnt_shift += (((uintptr_t)pte & PTEOFFSET) >> PTESHIFT) * LKCNT_NBITS;
	count = (ptegp->ptegp_lockmap >> lkcnt_shift) & LKCNT_MASK;
	/*
	 * XXX PPC: Is it valid to ASSERT that the mapping has been unlocked
	 * (i.e lock count is zero) before unloading it?
	 *
	 * ASSERT(count == 0);
	 */
	if (count == LKCNT_HASHED)
		delete_ptelock(pte); /* Free the hash table entry */
	ptegp->ptegp_lockmap &= ~(LKCNT_MASK << lkcnt_shift);
}

/*
 * Sync the referenced and modified bits of the page_t with the PTE.
 * Clears the ref/change bits in the PTE.
 * Also, synchronizes the Cacheable bit in the PTE
 * with the noncacheable bit in the page_t.
 *
 * Any change to the PTE requires the TLBs to be
 * flushed, so subsequent accesses or modifies will
 * really cause the memory image of the PTE to be
 * modified.
 *
 * NOTE: Assumes that there is no activity
 * on the page while munging the TLB entry, too (ugh).
 */
static void
ppcmmu_ptesync(ptegp_t *ptegp, pte_t *pte, hme_t *hme, int flags)
{
	hat_t *hat;
	as_t *as;
	page_t *pp;
	caddr_t vaddr;
	uint_t rm;
	kmutex_t *spl;

	hat = &hats[hme->hme_hat];
	as = hat->hat_as;
	pp = hme->hme_page;
	ASSERT(MUTEX_HELD(&hat->hat_mutex));
	ASSERT(&ptegp->ptegp_mutex);

	vaddr = pte_to_vaddr(pte);

	if (pp != NULL) {
		if (flags & (PPCMMU_RMSYNC | PPCMMU_RMSTAT)) {
			if (flags & PPCMMU_INVSYNC)
				rm = mmu_delete_pte(pte, vaddr);
			else if (flags & PPCMMU_RMSYNC)
				rm = mmu_pte_reset_rmbits(pte, vaddr);
			else
				// XXX Define ptep_get_rm(), pte1_get_rm()
				rm = (ptep_get_r(pte) << 1) | ptep_get_c(pte);

			if (rm) {
				if ((flags & PPCMMU_RMSYNC) && hat->hat_rmstat)
					hat_setstat(as, vaddr, PAGESIZE, rm);
				if (!hme->hme_nosync) {
					spl = SPL_HASH(pp);
					mutex_enter(spl);
					if (rm & XXX_MAGIC(0x2))
						PP_SETREF(pp);
					if (rm & XXX_MAGIC(0x1))
						PP_SETMOD(pp);
					mutex_exit(spl);
				}
			}
		} else if (flags & PPCMMU_INVSYNC) {
			(void) mmu_delete_pte(pte, vaddr);
		}
	} else if (flags & PPCMMU_INVSYNC) {
		(void) mmu_delete_pte(pte, vaddr);
	}
}

/*
 * Locking primitives accessed by HATLOCK macros
 */

/* ARGSUSED */
static kmutex_t *
ppcmmu_page_enter(page_t *pp)
{
	kmutex_t *mtx;

	mtx = SPL_HASH(pp);
	mutex_enter(mtx);
	return (mtx);
}

/* ARGSUSED */
static void
ppcmmu_page_exit(kmutex_t *mtx)
{
	mutex_exit(mtx);
}

static kmutex_t *
ppcmmu_mlist_mtx(page_t *pp)
{
	kmutex_t *mtx;

	mtx = MML_HASH(pp);
	return (mtx);
}

static kmutex_t *
ppcmmu_mlist_enter(page_t *pp)
{
	kmutex_t *mtx;

	mtx = MML_HASH(pp);
	mutex_enter(mtx);
	return (mtx);
}

static void
ppcmmu_mlist_exit(kmutex_t *mtx)
{
	mutex_exit(mtx);
}

static int
ppcmmu_mlist_held(page_t *pp)
{
	kmutex_t *mtx;

	mtx = MML_HASH(pp);
	return (MUTEX_HELD(mtx));
}

/*
 * Local functions for the HAT.
 */

/*
 * pte_to_ptegp(pte_t *)
 *	Given the address of a PTE in the ptes[] array return the address of
 *	the ptegp structure corresponding to this PTE.
 */
ptegp_t *
pte_to_ptegp(pte_t *p)
{
	uint_t i;

	ASSERT(p >= ptes && p < eptes);
	i = ((uint_t)p >> 6) & nptegmask; /* PTEG number for this pte */
	if (i >= nptegp)
		return (&ptegps[(~i & nptegmask)]);
	else
		return (&ptegps[i]);
}

/*
 * hme_to_ptegp(hme_t *p)
 *	Given the address of hme return the address of the ptegp structure
 *	corresponding to this hme.
 */
ptegp_t *
hme_to_ptegp(hme_t *p)
{
	uint_t i;

	i = (p - hments) >> XXX_MAGIC(3); /* PTEG number for this hme */
	if (i >= nptegp)
		return (&ptegps[(~i & nptegmask)]);
	else
		return (&ptegps[i]);
}

/*
 * Search the PTEG for a valid mapping given the vsid, api and H bit.
 * Returns NULL if no PTE in the group matches.
 */
static pte_t *
find_pte(pte_t *pteg, uint_t vsid, uint_t api, uint_t h)
{
	uint_t pte_w0;
	int i;

	pte_w0 = PTE0_NEW(PTE_VALID, vsid, h, api);

	for (i = NPTEPERPTEG; i; --i) {
		if (pte_w0 == ((uint_t *)pteg)[PTEWORD0])
			return (pteg);
		++pteg;
	}

	return (0);
}

/*
 * steal_pte(hat, pteg, ptegp)
 *	hat   - hat structure for the new mapping.
 *	pteg  - address of PTEG. (i.e PTE0).
 *	ptegp - pointer to ptegp structure.
 *
 * hat_mutex and ptegp_mutex are already held at the entry to this routine.
 *
 * Steal a PTE slot from the PTEG. Returns NULL if it cannot steal a slot
 * from the group, otherwise it returns pointer to the freed PTE slot.
 *
 * Algorithm:
 *	Loop (for NPTEPERPTEG times):
 *		If the PTE is not locked and hme is not busy (or not stolen
 *		in the previous scan) and can get mlist lock for the page
 *		then steal this slot, unload the mapping and return the
 *		pointer to this slot.
 *		Else continue the loop.
 *	Return NULL.
 *
 * Note: Locks should be acquired in the order:
 *		mlist_lock
 *		hat_mutex
 *		ptegp_mutex
 */
static pte_t *
steal_pte(hat_t *hat, pte_t *pteg, ptegp_t *ptegp)
{
	kmutex_t *pml;
	hme_t *hme;
	page_t *pp;
	uint_t lockmap;
	uint_t mask;
	int i;
	pte_t *p;
	int try_again = 0;
	uint_t xrpn1, xrpn2;

	ASSERT(MUTEX_HELD(&hat->hat_mutex));
	ASSERT(MUTEX_HELD(&ptegp->ptegp_mutex));
	ASSERT(((uintptr_t)pteg & PTEOFFSET) == 0);

Loop:
	lockmap = ptegp->ptegp_lockmap;
	mask = ((pteg >= mptes) ?
		(LKCNT_MASK << LKCNT_UPPER_SHIFT) : LKCNT_MASK);
	p = pteg;
	for (i = NPTEPERPTEG; i; ++p, --i, mask <<= LKCNT_SHIFT) {
		if (lockmap & mask)
			continue;		/* PTE is locked */
		hme = pte_to_hme(p);
		if (hme->hme_impl & HME_BUSY)
			continue;		/* hme is busy */
		/*
		 * Try to avoid picking up the same hme which was stolen
		 * last time we searched in this group.  If we don't find
		 * another one to steal then we come back to this.
		 */
		if (hme->hme_impl & HME_STOLEN) {
			hme->hme_impl &= ~HME_STOLEN;
			try_again = 1;
			continue;
		}
		xrpn1 = (p->ptew[PTEWORD1]) & PTE1_PFN_FLD_MASK;
		pp = ptep_to_pp(p);
		ASSERT(hme->hme_page == pp);
		if (pp != NULL && ppcmmu_mlist_held(pp))
			continue;
		hme->hme_impl |= HME_BUSY;

		if (pp != NULL || (hat != &hats[hme->hme_hat])) {
			hat_t *other_hat;

			/* Acquire the necessary locks in the right order */
			mutex_exit(&ptegp->ptegp_mutex);
			mutex_exit(&hat->hat_mutex);
			if (pp) {
				pml = ppcmmu_mlist_enter(pp);
				/*
				 * Check if the hme has changed while we were
				 * getting the mlist lock.
				 * (lock paranoia!?)
				 */
				xrpn2 = (p->ptew[PTEWORD1]) & PTE1_PFN_FLD_MASK;
				if (hme->hme_valid && xrpn2 != xrpn1) {
					ppcmmu_mlist_exit(pml);
					mutex_enter(&hat->hat_mutex);
					mutex_enter(&ptegp->ptegp_mutex);
					continue; /* Continue the search */
				}
			}
			if (hme->hme_valid) {
				other_hat = &hats[hme->hme_hat];
				mutex_enter(&other_hat->hat_mutex);
				mutex_enter(&ptegp->ptegp_mutex);
				/* Check if hme is still valid */
				if (hme->hme_valid) {
					ppcmmu_pteunload(ptegp, p, pp, hme,
						hme->hme_nosync ?
						PPCMMU_NOSYNC : PPCMMU_RMSYNC);
				}
				/* Get back the right hat mutex */
				if (hat != other_hat) {
					mutex_exit(&other_hat->hat_mutex);
					if (!mutex_tryenter(&hat->hat_mutex)) {
					    mutex_exit(&ptegp->ptegp_mutex);
					    mutex_enter(&hat->hat_mutex);
					    mutex_enter(&ptegp->ptegp_mutex);
					}
				}
			} else {
				mutex_enter(&hat->hat_mutex);
				mutex_enter(&ptegp->ptegp_mutex);
			}
			if (pp)
				ppcmmu_mlist_exit(pml);
			/*
			 * If someone grabbed our stolen PTE
			 * while the ptegp mutex is freed and
			 * reacquired then continue the search.
			 * (lock paranoia!?)
			 */
			if (hme->hme_valid)
				continue;
		} else {
			ppcmmu_pteunload(ptegp, p, pp, hme,
			    hme->hme_nosync ?
			    PPCMMU_NOSYNC : PPCMMU_RMSYNC);
		}
		hme->hme_impl = HME_STOLEN; /* Mark the hme as STOLEN */
		return (p);
	}

	if (try_again) {
		try_again = 0;
		goto Loop;
	}

	return (NULL);
}

/*
 * Find a free PTE slot in the PTEG.
 * If we find a free slot in the group, we use it;
 * otherwise we look for any stale mapping in the group before we say NO.
 */

static pte_t *
find_free_pte(hat_t *hat, pte_t *pteg, ptegp_t *ptegp, page_t *pp)
{
	kmutex_t *pml;
	uint_t validmap;
	uint_t mask;
	uint_t i;
	uint_t repeat = 1; /* repeat count */
	uint_t mask_all;
	pte_t *pte;
	uint_t flags;

	ASSERT(&ptegp->ptegp_mutex);
	ASSERT(((uintptr_t)pteg & PTEOFFSET) == 0);
	ASSERT(MUTEX_HELD(&hat->hat_mutex));
	ASSERT((pp == NULL) || ppcmmu_mlist_held(pp));

	if (pp)
		pml = ppcmmu_mlist_mtx(pp);

again:
	if (pteg >= mptes) {
		mask = XXX_MAGIC(0x100);
		mask_all = XXX_MAGIC(0xff00);
	} else {
		mask = XXX_MAGIC(0x1);
		mask_all = XXX_MAGIC(0xff);
	}
	pte = pteg;
	validmap = ptegp->ptegp_validmap;
	if ((validmap & mask_all) != mask_all) {
		/*
		 * We have at least one free slot.
		 */
		for (i = NPTEPERPTEG; i; ++pte, --i, mask <<= 1) {
			if ((validmap & mask) == 0)
				return (pte);
		}
	}

	/*
	 * Look for any stale mappings in the group.
	 * If we find one then unload it (to sync rm bits) and try again.
	 */
	for (i = NPTEPERPTEG; i != 0; ++pte, --i) {
		hme_t *hme;
		uint_t vsid;
		page_t *opp;
		hat_t *ohat;

		vsid = ptep_get_vsid(pte);
		if (IS_VSID_RANGE_INUSE(vsid >> VSIDRANGE_SHIFT))
			continue;	/* VSID is in use */

		/* Stale mapping.  Unload it to sync rm bits. */
		hme = pte_to_hme(pte);
		opp = ptep_to_pp(pte);
		ASSERT(opp == hme->hme_page);
		ohat = &hats[hme->hme_hat];

		/*
		 * Obtain all the locks in the correct order.
		 */
		mutex_exit(&ptegp->ptegp_mutex);
		mutex_exit(&hat->hat_mutex); /* Release our hat mutex */
		if (opp && (opp != pp)) {
			/*
			 * The pp for the stale mapping is different from
			 * our pp for which we are trying to make a mapping.
			 * We need to give up mlist lock on our pp to avoid
			 * possible deadlock, in case we block here. XXX
			 */
			ppcmmu_mlist_exit(pml);
			pml = ppcmmu_mlist_enter(opp);
		}
		mutex_enter(&ohat->hat_mutex);
		mutex_enter(&ptegp->ptegp_mutex);
		/*
		 * While we were getting the locks, PTE could
		 * have been unloaded/reloaded, so check it again
		 * before trying to unload it.
		 */
		if (hme->hme_valid && (vsid == ptep_get_vsid(pte))) {
			flags = hme->hme_nosync ? PPCMMU_NOSYNC : PPCMMU_RMSYNC;
			ppcmmu_pteunload(ptegp, pte, opp, hme, flags);
		}
		/*
		 * Release locks for the old (stale) hat and
		 * reacquire the locks for our hat.
		 */
		mutex_exit(&ptegp->ptegp_mutex);
		mutex_exit(&ohat->hat_mutex);
		if (opp && (opp != pp)) {
			ppcmmu_mlist_exit(pml);
			pml = ppcmmu_mlist_enter(pp);
		}
		mutex_enter(&hat->hat_mutex);
		mutex_enter(&ptegp->ptegp_mutex);
		goto again; /* try now */
	}
	if (repeat--)
		goto again;
	return (NULL);
}

/*
 * Given the address of a PTE in the page table,
 * compute the virtual address of the page for this mapping.
 */
static caddr_t
pte_to_vaddr(pte_t *pte)
{
	uintptr_t api, vsid, vaddr, h1;
	uint_t h;

	api = ptep_get_api(pte);
	vsid = ptep_get_vsid(pte);
	h = ptep_get_h(pte);
	h1 = ((uintptr_t)pte & XXX_MAGIC(0xFFC0)) >> 6;
	if (h != H_PRIMARY)
		h1 ^= hash_pteg_mask;
	vaddr = ((h1 ^ (vsid & XXX_MAGIC(0x3FF))) << MMU_PAGESHIFT) |
		(api << XXX_MAGIC(22)) | (vsid << XXX_MAGIC(28));
	return ((caddr_t)vaddr);
}

/*
 * Unlock the PTE: Decrement the lock count in the lockmap and/or
 * the hash table entry for this PTE.
 */
static void
pteunlock(ptegp_t *ptegp, pte_t *pte)
{
	int lkcnt_shift;
	int count;

	if (pte >= mptes)
		lkcnt_shift = LKCNT_UPPER_SHIFT;
	else
		lkcnt_shift = LKCNT_LOWER_SHIFT;
	lkcnt_shift += (((uintptr_t)pte & PTEOFFSET) >> PTESHIFT) * LKCNT_NBITS;
	count = (ptegp->ptegp_lockmap >> lkcnt_shift) & LKCNT_MASK;
	if (count == LKCNT_HASHED) {
		count = change_ptelock(pte, -1);
		if (count == 0) {
			/* Free the hash table entry */
			delete_ptelock(pte);
			ptegp->ptegp_lockmap &= ~(LKCNT_MASK << lkcnt_shift);
		}
	} else {
		/*
		 * XXX PPC: Currently we don't enable this ASSERT because
		 * this function gets called from the console initialization
		 * stuff from startup() before hat_kern_setup() is done.
		 * Jordan, what are the console requirements here?
		 *
		 * ASSERT(count != 0);
		 */
		ptegp->ptegp_lockmap &= ~(LKCNT_MASK << lkcnt_shift);
		if (--count > 0)
			ptegp->ptegp_lockmap |= count << lkcnt_shift;
	}
}

/*
 * Lock the PTE: Increment the lock count in the lockmap
 * and/or update the hash table entry.
 */
void
ptelock(ptegp_t *ptegp, pte_t *pte)
{
	int lkcnt_shift;
	int count;

	if (pte >= mptes)
		lkcnt_shift = LKCNT_UPPER_SHIFT;
	else
		lkcnt_shift = LKCNT_LOWER_SHIFT;
	lkcnt_shift += (((uintptr_t)pte & PTEOFFSET) >> PTESHIFT) * LKCNT_NBITS;
	count = (ptegp->ptegp_lockmap >> lkcnt_shift) & LKCNT_MASK;
	if (count == LKCNT_HASHED)
		(void) change_ptelock(pte, +1);
	else {
		count += 1;
		ptegp->ptegp_lockmap &= ~(LKCNT_MASK << lkcnt_shift);
		ptegp->ptegp_lockmap |= count << lkcnt_shift;
		if (count == LKCNT_HASHED)
			(void) create_ptelock(pte, count);
	}
}

/*
 * ptelock_hash_ops(pte, count, flags)
 * where
 *	pte:	Pointer to the hw PTE.
 *	count:	New lockcnt value for this PTE.
 *	flags:	HASH_CREATE	Allocate an entry for this PTE.
 *		HASH_CHANGE	Change lockcnt value (+count) for this PTE.
 *		HASH_DELETE	Delete the entry for this PTE.
 */
static int
ptelock_hash_ops(pte_t *pte, int count, int flags)
{
	int hashval, i;
	uint_t target = 0;

	/* Compute the hash value for this PTE */
	i = hashval = ((uint_t)pte >> PTESHIFT) & ptelock_hash_mask;

	if ((flags & HASH_CREATE) == 0)
		target = (uint_t)pte;

	lock_set(&ptelock_hash_lock);
	/*
	 * Start the search forward beginning with the hashed entry.
	 */
	while (i < ptelock_hashtab_size) {
		if (ptelock_hashtab[i].tag == target)
			goto found;
		++i;
	}
	/* Failed.  Try the search backward */
	i = hashval - 1;
	while (i >= 0) {
		if (ptelock_hashtab[i].tag == target)
			goto found;
		--i;
	}
	lock_clear(&ptelock_hash_lock);

	if (flags & HASH_CREATE) {
		/* No free entry found in the hash table - panic!! */
		panic("Out of hash entries in ptelock_hashtab[]...\n");
	} else {
		/* Could not find the entry for this PTE */
		panic("No hash entry found for the pte %p\n", pte);
	}
found:
	switch (flags) {
	case HASH_DELETE:
		/* Free the hash entry */
		ptelock_hashtab[i].tag = 0;
		ptelock_hashtab[i].lockcnt = 0;
		break;
	case HASH_CHANGE:
		/* Change the lockcnt */
		ptelock_hashtab[i].lockcnt += count;
		break;
	case HASH_CREATE:
		/* Initialize the entry */
		ptelock_hashtab[i].tag = (uint_t)pte;
		ptelock_hashtab[i].lockcnt = count;
		VMHATSTAT_INCR(vh_hash_ptelock);
		break;
	}
	lock_clear(&ptelock_hash_lock);
	return (ptelock_hashtab[i].lockcnt);
}


/*
 * Unload stale mappings from in a PTEG pair.
 *
 * Algorithm:
 *	Get PTEGP mutex lock.
 *	Scan the PTEs and for each stale mapping,
 *	  get mlist lock (if it has page structure),
 *	  and unload the mapping.
 *	Release mlist lock, if held.
 *	Release PTEGP mutex.
 */

static void
ppcmmu_vsid_gc_pteg(pte_t *pte)
{
	ptegp_t *ptegp;
	hme_t *hme;
	page_t *pp;
	int i;
	hat_t *hat;

	ptegp = pte_to_ptegp(pte);
	mutex_enter(&ptegp->ptegp_mutex);
	for (i = 0; i < NPTEPERPTEG; ++i, ++pte) {
		kmutex_t *pml;
		uint_t vsid;

		if (!ptep_isvalid(pte))
			continue;
		vsid = ptep_get_vsid(pte);
		if (IS_VSID_RANGE_INUSE(vsid >> VSIDRANGE_SHIFT))
			continue;
		/* Stale mapping.  Unload it. */
		hme = pte_to_hme(pte);
		hat = &hats[hme->hme_hat];
		pp = ptep_to_pp(pte);
		ASSERT(pp == hme->hme_page);
		/*
		 * Acquire the locks in the correct order.
		 */
		mutex_exit(&ptegp->ptegp_mutex);
		if (pp)
			pml = ppcmmu_mlist_enter(pp);
		mutex_enter(&hat->hat_mutex);
		mutex_enter(&ptegp->ptegp_mutex);
		/*
		 * Make sure the PTE hasn't been modified
		 * while we were getting the locks.
		 */
		if (hme->hme_valid &&
		    (vsid == ptep_get_vsid(pte))) {
			ppcmmu_pteunload(ptegp, pte, pp, hme,
			    hme->hme_nosync ?
			    PPCMMU_NOSYNC : PPCMMU_RMSYNC);
		}
		mutex_exit(&hat->hat_mutex);
		if (pp)
			ppcmmu_mlist_exit(pml);
	}
	mutex_exit(&ptegp->ptegp_mutex);
}

/*
 * Kernel thread to unload stale mappings from the page table
 * and recreate the VSID free list.
 *
 * Algorithm:
 *	Get ppcmmu_res_lock mutex.
 *	Loop for ever:
 *		Loop for each PTEGpair:
 *			Get PTEGP mutex lock.
 *			Scan the PTEs and for each stale mapping,
 *			  get mlist lock (if it has page structure),
 *			  and unload the mapping.
 *			Release mlist lock, if held.
 *			Release PTEGP mutex.
 *		Reconstruct free VSID list from the vsid_bitmap array.
 *		cv_wait(&ppcmmu_cv, &ppcmmu_res_lock).
 *
 */

void
ppcmmu_vsid_gc(void)
{
	pte_t *pte;

	mutex_enter(&ppcmmu_res_lock);

	for (;;) {
		/* Sweep through all PTEG pairs */
		for (pte = ptes; pte < eptes; pte += NPTEPERPTEG)
			ppcmmu_vsid_gc_pteg(pte);

		/* Reconstruct the free VSID sets */
		build_vsid_freelist();

		/*
		 * Wakeup anyone waiting for a free VSID.
		 */
		if (ppcmmu_vsid_wanted) {
			ppcmmu_vsid_wanted = 0;
			cv_broadcast(&ppcmmu_vsid_cv);
		}
		/* Wait until the wakeup call from ppcmmu_alloc() */
		cv_wait(&ppcmmu_cv, &ppcmmu_res_lock);
	}
}

/*
 * Rebuild the free vsid-range-set list from the vsid_bitmap.
 * If memory allocation fails in allocating vsid_alloc set structures then
 * it terminates building the rest of the list.  It is assumed that this
 * routine is woken up only when the vsid_range free list is empty.
 */
static void
build_vsid_freelist(void)
{
	uint_t cword;
	uint_t *bp;
	int n;
	int bitsleft;
	int free_ranges = 0;
	uint_t next_free_vsid;
	int nzeros;
	struct vsid_alloc_set *head = NULL;
	struct vsid_alloc_set *p;

	n = ppcmmu_max_vsidranges / NBINT;
	bp = vsid_bitmap + (n - 1); /* start from the last word in the bitmap */

	next_free_vsid = ppcmmu_max_vsidranges;
	for (cword = *bp; n; cword = *--bp, --n) {
		if (cword == 0) {
			free_ranges += NBINT;
			continue;
		}

		nzeros = cntlzw(cword); /* number of leading zeros */
		free_ranges += nzeros;
		bitsleft = NBINT;
		do {
			/*
			 * Allocate a set and attach it at the head of the list
			 */
			if (free_ranges) {
				p = kmem_zalloc(sizeof (struct vsid_alloc_set),
					KM_NOSLEEP);
				if (!p) {
					cmn_err(CE_WARN,
						"No memory for vsid_alloc_set");
					break;
				}
				p->vs_nvsid_ranges = free_ranges;
				next_free_vsid -= free_ranges;
				p->vs_vsid_range_id = next_free_vsid;
				free_ranges = 0;
				if (head)
					p->vs_next = head;
				else
					p->vs_next = NULL;
				head = p;
			}
			--next_free_vsid;
			bitsleft -= (nzeros + 1);
			cword <<= nzeros + 1;
			nzeros = cntlzw(cword); /* number of leading zeros */
			if (nzeros > bitsleft) {
				free_ranges += bitsleft;
				break;
			}
			free_ranges += nzeros;
		} while (bitsleft > 0);
	}

	/*
	 * If the loop terminates normally then 'free_ranges' would be 0 because
	 * the vsid_range_id '0' (used by kernel) is always set (i.e in use).
	 */
	ASSERT((p == NULL) || (free_ranges == NULL));

	ASSERT(vsid_alloc_head == NULL); /* old list should be empty */

	/* setup the head pointer to the newly built list */
	vsid_alloc_head = head;
#if defined(HAT_DEBUG)
	/*
	 * Match the vsid_bitmap with the vsid_freelist generated.
	 */
	for (p = vsid_alloc_head; p; p = p->vs_next) {
		int j, cnt;

		j = p->vs_vsid_range_id;
		for (cnt = p->vs_nvsid_ranges; cnt; --cnt, ++j) {
			if (IS_VSID_RANGE_INUSE(j)) {
				prom_printf(
					"*** ERROR FOR VSID RANGE %x ***\n",
					j);
				debug_enter(NULL);
				panic("build_vsid_freelist: "
					"INTERNAL ERROR!!\n");
			}
		}
	}
#endif /* defined(HAT_DEBUG) */
}

/*
 * Flush the cache for the specified page in the specified context.
 *
 * Flushing the cache for a page requires the context (at least the
 * corresponding segment register should be for this context) to be the
 * active context.  This generally happens thru /proc where a debugger
 * has changed the contents of the page in the program's address space
 * but the active context is of the debugger's.  Generally only the
 * debuggers write to the proc's address space so the solution implemented
 * here is the simplest but not efficient.
 */
static void
ppcmmu_cache_flushpage(hat_t *hat, caddr_t vaddr)
{
	uint_t old_sr;

	/*
	 * If this is a kernel page or we are in the right context,
	 * then simply do the flush.
	 */
	if ((vaddr >= (caddr_t)KERNELBASE) ||
		(((mfsr(0) & SR_VSID) >> XXX_MAGIC(4)) == hat->hat_vsidr)) {
		mmu_cache_flushpage(vaddr);
		return;
	}

	/*
	 * Flushing for a different context.
	 */
	/* save the old segment register value */
	old_sr = mfsrin(vaddr);
	kpreempt_disable();
	/* load the segment register for the other context */
	mtsrin_sync(VSID(hat->hat_vsidr, vaddr) | SR_KU, vaddr);
	/* do the flush */
	mmu_cache_flushpage(vaddr);
	/* restore the segment register */
	mtsrin_sync(old_sr, vaddr);
	kpreempt_enable();
}

static void
ppcmmu_page_clrwrt(page_t *pp)
{
	// XXX See usr/src/uts/i86pc/vm/hat_i86.c
	// XXX function hati_page_clrwrt()
	panic("ppcmmu_page_clrwrt() not implemented.\n");
}

/*
 * This function is currently not supported on this platform.
 * For what it is supposed to do, see hat.c and hat_srmmu.c
 */
/*ARGSUSED*/
faultcode_t
hat_softlock(xhat_t *xhat, caddr_t vaddr, size_t *lenp, page_t **ppp, uint_t flags)
{
	return (FC_NOSUPPORT);
}

#if defined(XXX_OBSOLETE_SOLARIS)

/*
 * This function is currently not supported on this platform.
 * For what it is supposed to do, see hat.c and hat_srmmu.c
 */
/*ARGSUSED*/
faultcode_t
hat_pageflip(hat_t *hat, caddr_t addr_to, caddr_t kaddr, size_t *lenp,
		page_t **pp_to, page_t **pp_from)
{
	return (FC_NOSUPPORT);
}

#endif /* defined(XXX_OBSOLETE_SOLARIS) */


/*
 * return supported features
 */
/*ARGSUSED1*/
int
hat_supported(enum hat_features feature, void *arg)
{
	switch (feature) {
	case    HAT_SHARED_PT:
		return (0);	/* Does not support ISM */
	default:
		return (0);
	}
}

/*
 * Called when a thread is exiting and has been switched to the kernel AS
 */
void
hat_thread_exit(kthread_t *thd)
{
	ASSERT(thd->t_procp->p_as == &kas);
	hat_switch(thd->t_procp->p_as->a_hat);
}

int
hat_stats_enable(xhat_t *xhat)
{
	hat_t *hat = (hat_t *)xhat;

	mutex_enter(&hat->hat_mutex);
	++(hat->hat_rmstat);
	mutex_exit(&hat->hat_mutex);
	return (1);
}

void
hat_stats_disable(xhat_t *xhat)
{
	hat_t *hat = (hat_t *)xhat;

	mutex_enter(&hat->hat_mutex);
	ASSERT(hat->hat_rmstat != 0);
	--(hat->hat_rmstat);
	mutex_exit(&hat->hat_mutex);
}

/*
 * Kernel Physical Mapping (kpm) facility
 *
 * Most of the routines needed to support segkpm are almost no-ops on the
 * PPC platform.  We map in the entire segment when it is created and leave
 * it mapped in, so there is no additional work required to set up and tear
 * down individual mappings.  All of these routines were created to support
 * SPARC platforms that have to avoid aliasing in their virtually indexed
 * caches.
 *
 * Most of the routines have sanity checks in them (e.g. verifying that the
 * passed-in page is locked).  We don't actually care about most of these
 * checks on PPC, but we leave them in place to identify problems in the
 * upper levels.
 */

/*
 * Map in a locked page and return the vaddr.
 */
/*ARGSUSED*/
caddr_t
hat_kpm_mapin(page_t *pp, struct kpme *kpme)
{
	caddr_t		vaddr;

#if defined(HAT_DEBUG)
	if (kpm_enable == 0) {
		cmn_err(CE_WARN, "hat_kpm_mapin: kpm_enable not set\n");
		return ((caddr_t)NULL);
	}

	if (pp == NULL || PAGE_LOCKED(pp) == 0) {
		cmn_err(CE_WARN, "hat_kpm_mapin: pp zero or not locked\n");
		return ((caddr_t)NULL);
	}
#endif /* defined(HAT_DEBUG) */

	vaddr = hat_kpm_page2va(pp, 1);

	return (vaddr);
}

/*
 * Mapout a locked page.
 */
/*ARGSUSED*/
void
hat_kpm_mapout(page_t *pp, struct kpme *kpme, caddr_t vaddr)
{
#if defined(HAT_DEBUG)
	if (kpm_enable == 0) {
		cmn_err(CE_WARN, "hat_kpm_mapout: kpm_enable not set\n");
		return;
	}

	if (IS_KPM_ADDR(vaddr) == 0) {
		cmn_err(CE_WARN, "hat_kpm_mapout: no kpm address\n");
		return;
	}

	if (pp == NULL || PAGE_LOCKED(pp) == 0) {
		cmn_err(CE_WARN, "hat_kpm_mapout: page zero or not locked\n");
		return;
	}
#endif /* defined(HAT_DEBUG) */
}

/*
 * Return the kpm virtual address for a specific pfn
 */
caddr_t
hat_kpm_pfn2va(pfn_t pfn)
{
	uintptr_t vaddr;

	ASSERT(kpm_enable);

	vaddr = (uintptr_t)kpm_vbase + mmu_ptob(pfn);

	return ((caddr_t)vaddr);
}

/*
 * Return the kpm virtual address for the page at pp.
 */
/*ARGSUSED*/
caddr_t
hat_kpm_page2va(page_t *pp, int checkswap)
{
	return (hat_kpm_pfn2va(pp->p_pagenum));
}

/*
 * Return the page frame number for the kpm virtual address vaddr.
 */
pfn_t
hat_kpm_va2pfn(caddr_t vaddr)
{
	pfn_t		pfn;

	ASSERT(IS_KPM_ADDR(vaddr));

	pfn = (pfn_t)btop(vaddr - kpm_vbase);

	return (pfn);
}


/*
 * Return the page for the kpm virtual address vaddr.
 */
page_t *
hat_kpm_vaddr2page(caddr_t vaddr)
{
	pfn_t		pfn;

	ASSERT(IS_KPM_ADDR(vaddr));

	pfn = hat_kpm_va2pfn(vaddr);

	return (page_numtopp_nolock(pfn));
}

/*
 * hat_kpm_fault is called from segkpm_fault
 * when we take a page fault on a KPM page.
 * This should never happen on PPC.
 */
int
hat_kpm_fault(xhat_t *xhat, caddr_t vaddr)
{
	hat_t *hat = (hat_t *)xhat;

	panic("pagefault in seg_kpm.  hat: 0x%p  vaddr: 0x%p", hat, vaddr);
	// XXX return (0);
}

/*ARGSUSED*/
void
hat_kpm_mseghash_clear(int nentries)
{
}

/*ARGSUSED*/
void
hat_kpm_mseghash_update(pgcnt_t inx, struct memseg *msp)
{
}

uint_t
hat_boot_probe(uintptr_t *va, size_t *len, pfn_t *pfn, uint_t *prot)
{
	batinfo_t *bat;
	uintptr_t probe_va;

	probe_va = *va;
	*len = 0;
	*pfn = PFN_INVALID;
	*prot = 0;
	bat = ppcmmu_find_bat((caddr_t)probe_va);
	if (bat == NULL)
		return (0);
	*va = (uintptr_t)bat->bat_vaddr;
	*len = bat->bat_size;
	*pfn = (uintptr_t)bat->bat_paddr >> MMU_PAGESHIFT;
	*prot = bat->bat_vprot;
	return (1);
}

#if defined(HAT_DEBUG)

static uint_t line_dirty = 0;

static void
print_delay(void)
{
	extern hrtime_t get_time_base(void);
	uint_t i, j;

	for (j = 10; j != 0; --j) {
		for (i = 100000; i != 0; --i)
			get_time_base();
		// DPRINTF("%c", 0);
	}
}

static void
print_nl(void)
{
	uint_t i;

	print_delay();
	DPRINTF("\n");
	print_delay();
	line_dirty = 0;
}

static void
fresh_line(void)
{
	if (line_dirty)
		print_nl();
}

/*
 * Decode number as a small number with an SI suffix.
 */

static char *
sisfx32(char *res, uint_t n)
{
	static char sfx[] = { 0, 'K', 'M', 'G', 'T', 'P', 'E' };
	uint_t mag;

	mag = 0;
	while (n >= 1024 && (n & 1023) == 0) {
		++mag;
		n /= 1024;
	}
	if (mag > 0)
		sprintf(res, "%u%c", n, sfx[mag]);
	else
		sprintf(res, "%u", n);
	return (res);
}

static void
decode_bat_bl(char *buf, uint_t bl)
{
	size_t bat_size;

	// XXX Validate BAT BL
	// XXX Must be in range for CPU type and HIDx settings
	// XXX Must be of the form (2**n)-1
	// XXX Must be consistent with BEPI
	bat_size = (size_t)(bl + 1) << 17;
	sprintf(buf, "%x=%lx=", bl, bat_size);
	while (*buf) ++buf;
	sisfx32(buf, bat_size);
}

static void
decode_wimg(char *buf, uint_t wimg)
{
	uint_t i;
	uint_t msk;

	sprintf(buf, "%x=", wimg);
	while (*buf) ++buf;
	if (wimg > 0xf)
		*buf++ = '*';	// Overflow
	i = 0;
	msk = 1 << 3;
	while (msk != 0) {
		if ((wimg & msk) != 0)
			buf[i] = "wimg"[i];
		else
			buf[i] = '-';
		++i;
		msk >>= 1;
	}
	buf[i] = '\0';
}

/*
 * Decode page protection bits.
 * pp bits in BAT registers and in PTEs
 *
 */
static void
decode_pp(char *buf, uint_t prot)
{
	char *prot_table[4] = { "xx", "ro", "rw", "ro" };

	sprintf(buf, "%x=%s", prot, prot_table[prot]);
}

/*
 * Decode address.
 * Always show %p representation.
 * Also show representation as a small number with SI suffix,
 * if the address is easily represented that way.
 * That is, if the address can accurately be represented
 * as a small magnitude ( <= 9999 ) followed by an SI suffix.
 * The SI suffix really represents the nearby power of 2
 * computer engineering system, in which 1K is 1024,
 * and higher orders of magnitude {M, G, T, P, E} are
 * multiples of 1024, not 1000.
 *
 */

static void
decode_addr(char *buf, uint_t addr)
{
	uint_t range;

	sprintf(buf, "%p", (caddr_t)addr);
	if (addr >= 1024) {
		range = addr;
		while (range >= 1024 && (range & 1023) == 0)
			range /= 1024;
		if (range <= 9999) {
			while (*buf) ++buf;
			*buf++ = '=';
			sisfx32(buf, addr);
		}
	}
}

static void
print_bat_bl(uint_t n)
{
	char buf[64];

	decode_bat_bl(buf, n);
	DPRINTF("%s", buf);
}

static void
print_wimg(uint_t n)
{
	char buf[64];

	decode_wimg(buf, n);
	DPRINTF("%s", buf);
}

static void
print_pp(uint_t n)
{
	char buf[64];

	decode_pp(buf, n);
	DPRINTF("%s", buf);
}

static void
print_addr(uint_t n)
{
	char buf[64];

	decode_addr(buf, n);
	DPRINTF("%s", buf);
}

/*
 * Definitions of structure of various PPC registers
 */

#define	FT_BOOL 1
#define	FT_UINT 2
#define	FT_ADDR 3
#define	FT_DCOD 4
#define	FT_RESV 5

#define	FN_RESV ((char *)1)
#define	FN_END NULL

struct fld {
	char *fld_name;
	ushort_t fld_type;
	ushort_t fld_off;
	ushort_t fld_len;
	void (*fld_decode)(uint_t);
};

struct fld msr_fields[] = {
	{ FN_RESV,	FT_RESV,  0, 6, NULL },
	{ "VEC",	FT_BOOL,  6, 1, NULL },
	{ FN_RESV,	FT_RESV,  7, 6, NULL },
	{ "POW",	FT_BOOL, 13, 1, NULL },
	{ FN_RESV,	FT_RESV, 14, 1, NULL },
	{ "ILE",	FT_BOOL, 15, 1, NULL },
	{ "EE",		FT_BOOL, 16, 1, NULL },
	{ "PR",		FT_BOOL, 17, 1, NULL },
	{ "FP",		FT_BOOL, 18, 1, NULL },
	{ "ME",		FT_BOOL, 19, 1, NULL },
	{ "FE0",	FT_BOOL, 20, 1, NULL },
	{ "SE",		FT_BOOL, 21, 1, NULL },
	{ "BE",		FT_BOOL, 22, 1, NULL },
	{ "FE1",	FT_BOOL, 23, 1, NULL },
	{ FN_RESV,	FT_RESV, 24, 1, NULL },
	{ "IP",		FT_BOOL, 25, 1, NULL },
	{ "IR",		FT_BOOL, 26, 1, NULL },
	{ "DR",		FT_BOOL, 27, 1, NULL },
	{ FN_RESV,	FT_RESV, 28, 1, NULL },
	{ "PMM",	FT_BOOL, 29, 1, NULL },
	{ "RI",		FT_BOOL, 30, 1, NULL },
	{ "LE",		FT_BOOL, 31, 1, NULL },
	{ FN_END, 0, 0, 0, NULL }
};

struct fld batu_fields[] = {
	{ "BEPI",	FT_ADDR,  0, 15, NULL },
	{ FN_RESV,	FT_RESV, 15,  4, NULL },
	{ "BL",		FT_DCOD, 19, 11, print_bat_bl },
	{ "VS",		FT_BOOL, 30,  1, NULL },
	{ "VP",		FT_BOOL, 31,  1, NULL },
	{ FN_END, 0, 0, 0, NULL }
};

struct fld batl_fields[] = {
	{ "BRPN",	FT_ADDR,  0, 15, NULL },
	{ FN_RESV,	FT_RESV, 15, 10, NULL },
	{ "WIMG",	FT_DCOD, 25,  4, print_wimg },
	{ FN_RESV,	FT_RESV, 29,  1, NULL },
	{ "PP",		FT_DCOD, 30,  2, print_pp },
	{ FN_END, 0, 0, 0, NULL }
};

struct fld sdr1_fields[] = {
	{ "HTABORG",	FT_ADDR,  0, 16, NULL },
	{ FN_RESV,	FT_RESV, 16,  7, NULL },
	{ "HTABMASK",	FT_UINT, 23,  9, NULL },
	{ FN_END, 0, 0, 0, NULL }
};

struct fld segr_fields[] = {
	{ "T",		FT_BOOL,  0,  1, NULL },
	{ "Ks",		FT_BOOL,  1,  1, NULL },
	{ "Kp",		FT_BOOL,  2,  1, NULL },
	{ "N",		FT_BOOL,  3,  1, NULL },
	{ FN_RESV,	FT_RESV,  4,  4, NULL },
	{ "VSID",	FT_UINT,  8, 24, NULL },
	{ FN_END, 0, 0, 0, NULL }
};

struct fld pteu_fields[] = {
	{ "V",		FT_BOOL,  0,  1, NULL },
	{ "VSID",	FT_UINT,  1, 24, NULL },
	{ "H",		FT_BOOL, 25,  1, NULL },
	{ "API",	FT_UINT, 26,  6, NULL },
	{ FN_END, 0, 0, 0, NULL }
};

struct fld ptel_fields[] = {
	{ "RPN",	FT_ADDR,  0, 20, NULL },
	{ FN_RESV,	FT_RESV, 20,  3, NULL },
	{ "R",		FT_BOOL, 23,  1, NULL },
	{ "C",		FT_BOOL, 24,  1, NULL },
	{ "WIMG",	FT_DCOD, 25,  4, print_wimg },
	{ FN_RESV,	FT_RESV, 29,  1, NULL },
	{ "PP",		FT_DCOD, 30,  2, print_pp },
	{ FN_END, 0, 0, 0, NULL }
};

static uint_t
extract32(uint_t word, uint_t off, uint_t len)
{
	uint_t shft = 32 - off - len;
	uint_t mask = (1 << len) - 1;
	return ((word >> shft) & mask);
}

static uint_t
fldmask32(uint_t word, uint_t off, uint_t len)
{
	uint_t shft = 32 - off - len;
	uint_t mask = ((1 << len) - 1) << shft;
	return (word & mask);
}

static void
print_struct(struct fld *fields, uint_t sval, uint_t nz)
{
	struct fld *f;
	uint_t val;
	uint_t typ;
	uint_t off;
	uint_t len;
	uint_t pfldnr;

	for (f = fields; f->fld_name != FN_END; ++f) {
		typ = f->fld_type;
		off = f->fld_off;
		len = f->fld_len;
		if (typ == FT_RESV) {
			val = extract32(sval, off, len);
			if (val != 0) {
				DPRINTF("Error: reserved field.\n");
				// XXX show off, len, val
			}
		}
	}

	DPRINTF("{");
	pfldnr = 0;
	for (f = fields; f->fld_name != FN_END; ++f) {
		typ = f->fld_type;
		off = f->fld_off;
		len = f->fld_len;
		switch (typ) {
		case FT_RESV:
			val = extract32(sval, off, len);
			if (val != 0) {
				if (pfldnr > 0)
					DPRINTF(",");
				if (len == 1)
					DPRINTF("RESV[%u]", off);
				else
					DPRINTF("RESV[%u-%u]", off, off + len - 1);
				DPRINTF("=%u", val);
				++pfldnr;
			}
			break;
		case FT_BOOL:
			val = extract32(sval, off, len);
			if (val != 0 || nz == 0) {
				if (pfldnr > 0)
					DPRINTF(",");
				DPRINTF("%s=%u", f->fld_name, val);
				++pfldnr;
			}
			break;
		case FT_UINT:
			val = extract32(sval, off, len);
			if (pfldnr > 0)
				DPRINTF(",");
			DPRINTF("%s=%x", f->fld_name, val);
			++pfldnr;
			break;
		case FT_ADDR:
			val = fldmask32(sval, off, len);
			if (pfldnr > 0)
				DPRINTF(",");
			DPRINTF("%s=", f->fld_name);
			print_addr(val);
			++pfldnr;
			break;
		case FT_DCOD:
			val = extract32(sval, off, len);
			if (pfldnr > 0)
				DPRINTF(",");
			DPRINTF("%s=", f->fld_name);
			(*f->fld_decode)(val);
			++pfldnr;
			break;
		default:
			if (pfldnr > 0)
				DPRINTF(",");
			DPRINTF("%s=?", f->fld_name);
			++pfldnr;
		}
	}
	DPRINTF("}");
	line_dirty = 1;
}

static void
print_msr(uint_t n)
{
	print_struct(&msr_fields[0], n, 0);
}

static void
print_msr_nz(uint_t n)
{
	print_struct(&msr_fields[0], n, 1);
}

static void
print_batu(uint_t n)
{
	print_struct(&batu_fields[0], n, 0);
	print_delay();
}

static void
print_batl(uint_t n)
{
	print_struct(&batl_fields[0], n, 0);
	print_delay();
}

static void
print_sdr1(uint_t n)
{
	print_struct(&sdr1_fields[0], n, 0);
}

static void
print_sr(uint_t n)
{
	print_struct(&segr_fields[0], n, 0);
}

/*
 * print_pteu() and print_ptel() are not static,
 * because we want trap() to be able to print decoded PTEs.
 *
 * XXX We may want to rethink what is static and what is global.
 * XXX Also, some debugging functions may be "promoted" to be
 * XXX present even when DEBUG is not defined.
 *
 */

void
print_pteu(uint_t n)
{
	print_struct(&pteu_fields[0], n, 0);
}

void
print_ptel(uint_t n)
{
	print_struct(&ptel_fields[0], n, 0);
}

/*
 * Dump the information on the mappings in the address space.
 * Return the number of mappings found.
 */
static uint_t
print_hat_mappings(hat_t *hat)
{
	ptegp_t *ptegp;
	pte_t *pte;
	hme_t *hme;
	page_t *pp;
	int i;
	uint_t range;
	uint_t mappings;
	uint_t v;

	mappings = 0;
	v = hat->hat_vsidr;

	kpreempt_disable();
	DPRINTF("Mappings in hat %p, address space %p:\n",
		hat, hat->hat_as);
	pte = ptes;
	while (pte < eptes) {
		ptegp = pte_to_ptegp(pte);
		for (i = 0; i < NPTEPERPTEG; ++i, ++pte) {
			if (!ptep_isvalid(pte))
				continue;
			range = ptep_get_vsid(pte) >> VSIDRANGE_SHIFT;
			if (range != v)
				continue;
			hme = pte_to_hme(pte);
			pp = ptep_to_pp(pte);
			if (pp != hme->hme_page) {
				DPRINTF("*** ERROR: pp != hme_page\n");
				DPRINTF("    pp=%p, hme_page=%p\n",
					pp, hme->hme_page);
			}
			DPRINTF("\taddr %x pte %x (%x %x) hme %x pp %x\n",
				pte_to_vaddr(pte), pte,
				pte->ptew[PTEWORD0], pte->ptew[PTEWORD1],
				hme, pp);
			++mappings;
		}
	}
	DPRINTF("Total of %d mappings found.\n", mappings);
	kpreempt_enable();
	return (mappings);
}

static void
print_pteg(pte_t *gp)
{
	pte_t *pte;
	uint_t i;
	uint_t col;

	col = 0;
	for (pte = gp, i = 0; i < NPTEPERPTEG; ++pte, ++i) {
		if (ptep_isvalid(pte)) {
			if (col == 0) {
				col = 1;
				DPRINTF("0x%x:", pte);
			}
			DPRINTF(" %c 0x%x 0x%x",
				(ptep_get_h(pte) == 0) ? 'P' : 'S',
				pte->ptew[PTEWORD0], pte->ptew[PTEWORD1]);
		}
	}
	/*
	 * If we printed at least one PTE, advance to fresh line
	 */
	if (col > 0)
		DPRINTF("\n");
}

static void
print_pagetable(pte_t *ptp, uint_t num_ptegs)
{
	uint_t i;

	for (i = 0; i < num_ptegs; ptp += NPTEPERPTEG, ++i)
		print_pteg(ptp);
}

static uint_t
check_hat(hat_t *hat)
{
	if (hat == NULL) {
		DPRINTF("*** NULL hat\n");
		return (0);
	}
	if (hat->hat_classtag != CLASS_HAT) {
		DPRINTF(
			"Hat %u@0x%p has wrong class tag.\n"
			"  Expected %x, got %x\n",
			hat->hat_seq, hat,
			CLASS_HAT, hat->hat_classtag);
		return (0);
	}
	return (1);
}

static uint_t
check_as_lock(hat_t *hat, uint_t op, uint_t flags)
{
	if (IS_KERNEL_HAT(hat))
		return (1);
	/*
	 * XXX Investigate and explain HAT_UNLOAD_OTHER
	 */
	if (op == HAT_OP_UNLOAD && (flags & HAT_UNLOAD_OTHER))
		return (1);
	return (AS_LOCK_HELD(hat->hat_as, &hat->hat_as->a_lock));
}

static uint_t
check_va_range(hat_t *hat, caddr_t vaddr, size_t len)
{
	caddr_t end_vaddr;
	uintptr_t va = (uintptr_t)vaddr;
	uint_t status = 1;

	if (!is_page_aligned(va)) {
		DPRINTF("*** vaddr %p is not page aligned.\n", vaddr);
		status = 0;
	}
	if (!is_page_aligned(len)) {
		DPRINTF("*** length %x is not page aligned.\n", len);
		status = 0;
	}
	if (len == 0) {
		DPRINTF("*** zero length.\n");
		status = 0;
	}
	end_vaddr = vaddr + len - 1;
	if (end_vaddr < vaddr) {
		DPRINTF("*** VA range [%p..%p] wraps.\n", vaddr, end_vaddr);
		status = 0;
	}
	if (vaddr < (caddr_t)KERNELBASE && end_vaddr >= (caddr_t)KERNELBASE) {
		DPRINTF("*** VA range [%p..%p] spans user/kernel boundary\n",
			vaddr, end_vaddr);
		status = 0;
	}
	if (IS_KERNEL_HAT(hat)) {
		if (!(vaddr >= (caddr_t)KERNELBASE)) {
			DPRINTF("*** kernel address %p not in kva range\n",
				vaddr);
			status = 0;
		}
	} else {
		uint_t in_range;

		in_range = 1;
		if (vaddr >= (caddr_t)USERLIMIT) {
			DPRINTF("*** User address %p >= USERLIMIT\n", vaddr);
			in_range = 0;
		} else if (end_vaddr >= (caddr_t)USERLIMIT) {
			DPRINTF("*** End of VA range [%p,%p] >= USERLIMIT\n",
				vaddr, end_vaddr);
			in_range = 0;
		}
		if (!in_range) {
			DPRINTF("    USERLIMIT = %p", USERLIMIT);
			if (ISP2(LINT_VARIABLE(USERLIMIT)))
				DPRINTF(" (2**%u)", LOG2P2(USERLIMIT));
			DPRINTF(".\n");
			DPRINTF("    A user VA range, [start,end], must be"
				"bound by: 0 <= start <= end < USERLIMIT\n");
			status = 0;
		}
	}
	return (status);
}

// XXX This table is wrong.  Fix it.
// XXX Explain why the new logic is correct.

static uint_t map_valid[] = {
		/* val  mem  pp    consist	*/
		/* ---  ---  ----  -------	*/
	1,	/* 000  no   NULL  no		*/
	0,	/* 001  no   NULL  yes		*/
	0,	/* 010  no   page  no		*/
	0,	/* 011  no   page  yes		*/
	0,	/* 100  yes  NULL  no		*/
	0,	/* 101  yes  NULL  yes		*/
	0,	/* 110  yes  page  no		*/
	0	/* 111  yes  page  yes		*/
};

static uint_t
check_load_consist(page_t *pp, pfn_t pfn, uint_t flags)
{
	uint_t mem;
	uint_t consist;
	uint_t rc;

	mem = pf_is_memory(pfn);
	consist = ((flags & HAT_LOAD_NOCONSIST) == 0);
	rc = map_valid[(mem ? 4 : 0) | ((pp != NULL) ? 2 : 0) | consist];
	if (rc)
		return (rc);
	DPRINTF("Inconsistent combination of page_t and flags\n"
		"  pp=%p, pfn=0x%x, flags=%x,"
		"  memory=%x, consistent=%x\n",
		pp, pfn, flags, mem, consist);
	return (0);
}

#endif /* defined(HAT_DEBUG) */
