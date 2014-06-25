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
 * Copyright 2006 Cyril Plisko.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef _VM_VM_DEP_H
#define	_VM_VM_DEP_H

#pragma ident	"%Z%%M%	%I%	%E% SMI"

/*
 * UNIX machine dependent virtual memory support
 * Based heavily on x86 counterpart
 * XXX PPC This file needs massive work !
 */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * WARNING: vm_dep.h is included by files in common.  As such, macros
 * dependent upon PPC-specific and CHRP-specific declarations cannot
 * be used in this file.
 */

#define	GETTICK()	get_time_base()

/* memranges in descending order */
extern pfn_t		*memranges;

#define	MEMRANGEHI(mtype)						\
	((mtype > 0) ? memranges[mtype - 1] - 1: physmax)
#define	MEMRANGELO(mtype)	(memranges[mtype])

/*
 * combined memory ranges from mnode and memranges[] to manage single
 * mnode/mtype dimension in the page lists.
 */
typedef struct {
	pfn_t	mnr_pfnlo;
	pfn_t	mnr_pfnhi;
	int	mnr_mnode;
	int	mnr_memrange;		/* index into memranges[] */
	/* maintain page list stats */
	pgcnt_t	mnr_mt_pgmax;		/* mnode/mtype max page cnt */
	pgcnt_t	mnr_mt_clpgcnt;		/* cache list cnt */
	pgcnt_t	mnr_mt_flpgcnt;		/* free list cnt - small pages */
	pgcnt_t	mnr_mt_lgpgcnt;		/* free list cnt - large pages */
#ifdef DEBUG
	struct mnr_mts {		/* mnode/mtype szc stats */
		pgcnt_t	mnr_mts_pgcnt;
		int	mnr_mts_colors;
		pgcnt_t *mnr_mtsc_pgcnt;
	} 	*mnr_mts;
#endif
} mnoderange_t;

#ifdef DEBUG
#define	PLCNT_SZ(ctrs_sz) {						\
	int	szc, colors;						\
	ctrs_sz += mnoderangecnt * sizeof (struct mnr_mts) *		\
	    mmu_page_sizes;						\
	for (szc = 0; szc < mmu_page_sizes; szc++) {			\
		colors = page_get_pagecolors(szc);			\
		ctrs_sz += mnoderangecnt * sizeof (pgcnt_t) * colors;	\
	}								\
}

#define	PLCNT_INIT(addr) {						\
	int	mt, szc, colors;					\
	for (mt = 0; mt < mnoderangecnt; mt++) {			\
		mnoderanges[mt].mnr_mts = (struct mnr_mts *)addr;	\
		addr += (sizeof (struct mnr_mts) * mmu_page_sizes);	\
		for (szc = 0; szc < mmu_page_sizes; szc++) {		\
			colors = page_get_pagecolors(szc);		\
			mnoderanges[mt].mnr_mts[szc].mnr_mts_colors =	\
			    colors;					\
			mnoderanges[mt].mnr_mts[szc].mnr_mtsc_pgcnt =	\
			    (pgcnt_t *)addr;				\
			addr += (sizeof (pgcnt_t) * colors);		\
		}							\
	}								\
}
#define	PLCNT_DO(pp, mtype, szc, cnt, flags) {				\
	int	bin = PP_2_BIN(pp);					\
	if (flags & PG_LIST_ISINIT)					\
		mnoderanges[mtype].mnr_mt_pgmax += cnt;			\
	ASSERT((flags & PG_LIST_ISCAGE) == 0);				\
	if (flags & PG_CACHE_LIST)					\
		atomic_add_long(&mnoderanges[mtype].			\
		    mnr_mt_clpgcnt, cnt);				\
	else if (szc)							\
		atomic_add_long(&mnoderanges[mtype].			\
		    mnr_mt_lgpgcnt, cnt);				\
	else								\
		atomic_add_long(&mnoderanges[mtype].			\
		    mnr_mt_flpgcnt, cnt);				\
	atomic_add_long(&mnoderanges[mtype].mnr_mts[szc].		\
	    mnr_mts_pgcnt, cnt);					\
	atomic_add_long(&mnoderanges[mtype].mnr_mts[szc].		\
	    mnr_mtsc_pgcnt[bin], cnt);					\
}
#else
#define	PLCNT_SZ(ctrs_sz)
#define	PLCNT_INIT(base)
#define	PLCNT_DO(pp, mtype, szc, cnt, flags) {				\
	if (flags & PG_LIST_ISINIT)					\
		mnoderanges[mtype].mnr_mt_pgmax += cnt;			\
	if (flags & PG_CACHE_LIST)					\
		atomic_add_long(&mnoderanges[mtype].			\
		    mnr_mt_clpgcnt, cnt);				\
	else if (szc)							\
		atomic_add_long(&mnoderanges[mtype].			\
		    mnr_mt_lgpgcnt, cnt);				\
	else								\
		atomic_add_long(&mnoderanges[mtype].			\
		    mnr_mt_flpgcnt, cnt);				\
}
#endif

/*
 * macros to update page list max counts.  no-op on x86.
 * XXX what about PPC ???
 */
#define	PLCNT_MAX_INCR(pp, mnode, mtype, szc)
#define	PLCNT_MAX_DECR(pp, mnode, mtype, szc)

extern mnoderange_t	*mnoderanges;
extern int		mnoderangecnt;
extern int		mtype4g;

/*
 * Per page size free lists. Allocated dynamically.
 * dimensions [mtype][mmu_page_sizes][colors]
 *
 * mtype specifies a physical memory range with a unique mnode.
 */

extern page_t ****page_freelists;

#define	PAGE_FREELISTS(mnode, szc, color, mtype)		\
	(*(page_freelists[mtype][szc] + (color)))

/*
 * For now there is only a single size cache list. Allocated dynamically.
 * dimensions [mtype][colors]
 *
 * mtype specifies a physical memory range with a unique mnode.
 */
extern page_t ***page_cachelists;

#define	PAGE_CACHELISTS(mnode, color, mtype) 		\
	(*(page_cachelists[mtype] + (color)))

/*
 * There are mutexes for both the page freelist
 * and the page cachelist.  We want enough locks to make contention
 * reasonable, but not too many -- otherwise page_freelist_lock() gets
 * so expensive that it becomes the bottleneck!
 */

#define	NPC_MUTEX	16

kmutex_t	*fpc_mutex[NPC_MUTEX];
kmutex_t	*cpc_mutex[NPC_MUTEX];

extern page_t *page_get_mnode_freelist(int, uint_t, int, uchar_t, uint_t);
extern page_t *page_get_mnode_cachelist(uint_t, uint_t, int, int);

/* Find the bin for the given page if it was of size szc */
#define	PP_2_BIN_SZC(pp, szc)						\
	(((pp->p_pagenum) & page_colors_mask) >>			\
	(hw_page_array[szc].hp_shift - hw_page_array[0].hp_shift))

#define	PP_2_BIN(pp)		(PP_2_BIN_SZC(pp, pp->p_szc))

#define	PP_2_MEM_NODE(pp)	(PFN_2_MEM_NODE(pp->p_pagenum))
#define	PP_2_MTYPE(pp)		(pfn_2_mtype(pp->p_pagenum))
#define	PP_2_SZC(pp)		(pp->p_szc)

#define	SZCPAGES(szc)		(1 << PAGE_BSZS_SHIFT(szc))
#define	PFN_BASE(pfnum, szc)	(pfnum & ~(SZCPAGES(szc) - 1))

extern struct cpu	cpus[];
#define	CPU0		cpus

/*
 * macros to loop through the mtype range (page_get_mnode_{free,cache,any}list,
 * and page_get_contig_pages)
 *
 * MTYPE_START sets the initial mtype. -1 if the mtype range specified does
 * not contain mnode.
 *
 * MTYPE_NEXT sets the next mtype. -1 if there are no more valid
 * mtype in the range.
 */

#define	MTYPE_START(mnode, mtype, flags)				\
	(mtype = mtype_func(mnode, mtype, flags))

#define	MTYPE_NEXT(mnode, mtype, flags) {				\
	if (flags & PGI_MT_RANGE) {					\
		mtype = mtype_func(mnode, mtype, flags | PGI_MT_NEXT);	\
	} else {							\
		mtype = -1;						\
	}								\
}

/* mtype init for page_get_replacement_page */

#define	MTYPE_PGR_INIT(mtype, flags, pp, mnode, pgcnt) {		\
	mtype = mnoderangecnt - 1;					\
	flags |= PGI_MT_RANGE0;						\
}

#define	MNODE_PGCNT(mnode)		mnode_pgcnt(mnode)

#define	MNODETYPE_2_PFN(mnode, mtype, pfnlo, pfnhi)			\
	ASSERT(mnoderanges[mtype].mnr_mnode == mnode);			\
	pfnlo = mnoderanges[mtype].mnr_pfnlo;				\
	pfnhi = mnoderanges[mtype].mnr_pfnhi;

#define	PC_BIN_MUTEX(mnode, bin, flags) ((flags & PG_FREE_LIST) ?	\
	&fpc_mutex[(bin) & (NPC_MUTEX - 1)][mnode] :			\
	&cpc_mutex[(bin) & (NPC_MUTEX - 1)][mnode])

#define	FPC_MUTEX(mnode, i)	(&fpc_mutex[i][mnode])
#define	CPC_MUTEX(mnode, i)	(&cpc_mutex[i][mnode])

/* Return the leader for this mapping size */
#define	PP_GROUPLEADER(pp, szc) \
	(&(pp)[-(int)((pp)->p_pagenum & (SZCPAGES(szc)-1))])

/* Return the root page for this page based on p_szc */
#define	PP_PAGEROOT(pp) ((pp)->p_szc == 0 ? (pp) : \
	PP_GROUPLEADER((pp), (pp)->p_szc))

/*
 * The counter base must be per page_counter element to prevent
 * races when re-indexing, and the base page size element should
 * be aligned on a boundary of the given region size.
 *
 * We also round up the number of pages spanned by the counters
 * for a given region to PC_BASE_ALIGN in certain situations to simplify
 * the coding for some non-performance critical routines.
 */

#define	PC_BASE_ALIGN		((pfn_t)1 << PAGE_BSZS_SHIFT(MMU_PAGE_SIZES-1))
#define	PC_BASE_ALIGN_MASK	(PC_BASE_ALIGN - 1)

/*
 * cpu/mmu-dependent vm variables
 */
extern uint_t mmu_page_sizes;
extern uint_t mmu_exported_page_sizes;

/*
 * for hw_page_map_t, sized to hold the ratio of large page to base
 * pagesize (1024 max)
 */
typedef short	hpmctr_t;

/*
 * get the setsize of the current CPU
 */
extern uint_t l2cache_sets;
extern uint_t l2cache_linesz;
extern uint_t l2cache_assoc;
extern uint_t l2cache_size;

#define	L2CACHE_ALIGN		l2cache_linesz
#define	L2CACHE_ALIGN_MAX	64
#define	CPUSETSIZE()		\
	(l2cache_assoc ? (l2cache_size / l2cache_assoc) : MMU_PAGESIZE)

#define	LEVEL_SHIFT(szc) 12	/* FIX ME - x86-specific */
/*
 * Return the log2(pagesize(szc) / MMU_PAGESIZE) --- or the shift count
 * for the number of base pages in this pagesize
 */
#define	PAGE_BSZS_SHIFT(szc) (LEVEL_SHIFT(szc) - MMU_PAGESHIFT)

/*
 * Internal PG_ flags.
 */
#define	PGI_RELOCONLY	0x010000	/* opposite of PG_NORELOC */
#define	PGI_NOCAGE	0x020000	/* cage is disabled */
#define	PGI_PGCPHIPRI	0x040000	/* page_get_contig_page pri alloc */
#define	PGI_PGCPSZC0	0x080000	/* relocate base pagesize page */

/*
 * PGI range flags - should not overlap PGI flags
 */
#define	PGI_MT_RANGE0	0x1000000	/* mtype range to 0 */
#define	PGI_MT_RANGE4G	0x2000000	/* mtype range to 4g */
#define	PGI_MT_NEXT	0x4000000	/* get next mtype */
#define	PGI_MT_RANGE	(PGI_MT_RANGE0 | PGI_MT_RANGE4G)

/*
 * hash as and addr to get a bin.
 */

#define	AS_2_BIN(as, seg, vp, addr, bin)				\
	bin = ((((uintptr_t)(addr) >> PAGESHIFT) + ((uintptr_t)(as) >> 4)) \
	    & page_colors_mask)

/*
 * cpu private vm data - accessed thru CPU->cpu_vm_data
 *	vc_pnum_memseg: tracks last memseg visited in page_numtopp_nolock()
 *	vc_pnext_memseg: tracks last memseg visited in page_nextn()
 *	vc_kmptr: orignal unaligned kmem pointer for this vm_cpu_data_t
 *	vc_kmsize: orignal kmem size for this vm_cpu_data_t
 */

typedef struct {
	struct memseg	*vc_pnum_memseg;
	struct memseg	*vc_pnext_memseg;
	void		*vc_kmptr;
	size_t		vc_kmsize;
} vm_cpu_data_t;

/* allocation size to ensure vm_cpu_data_t resides in its own cache line */
#define	VM_CPU_DATA_PADSIZE						\
	(P2ROUNDUP(sizeof (vm_cpu_data_t), L2CACHE_ALIGN_MAX))

/* for boot cpu before kmem is initialized */
extern char	vm_cpu_data0[];

/*
 * When a bin is empty, and we can't satisfy a color request correctly,
 * we scan.  If we assume that the programs have reasonable spatial
 * behavior, then it will not be a good idea to use the adjacent color.
 * Using the adjacent color would result in virtually adjacent addresses
 * mapping into the same spot in the cache.  So, if we stumble across
 * an empty bin, skip a bunch before looking.  After the first skip,
 * then just look one bin at a time so we don't miss our cache on
 * every look. Be sure to check every bin.  Page_create() will panic
 * if we miss a page.
 *
 * This also explains the `<=' in the for loops in both page_get_freelist()
 * and page_get_cachelist().  Since we checked the target bin, skipped
 * a bunch, then continued one a time, we wind up checking the target bin
 * twice to make sure we get all of them bins.
 */
#define	BIN_STEP	19

#ifdef VM_STATS
struct vmm_vmstats_str {
	ulong_t pgf_alloc[MMU_PAGE_SIZES];	/* page_get_freelist */
	ulong_t pgf_allocok[MMU_PAGE_SIZES];
	ulong_t pgf_allocokrem[MMU_PAGE_SIZES];
	ulong_t pgf_allocfailed[MMU_PAGE_SIZES];
	ulong_t	pgf_allocdeferred;
	ulong_t	pgf_allocretry[MMU_PAGE_SIZES];
	ulong_t pgc_alloc;			/* page_get_cachelist */
	ulong_t pgc_allocok;
	ulong_t pgc_allocokrem;
	ulong_t pgc_allocokdeferred;
	ulong_t pgc_allocfailed;
	ulong_t	pgcp_alloc[MMU_PAGE_SIZES];	/* page_get_contig_pages */
	ulong_t	pgcp_allocfailed[MMU_PAGE_SIZES];
	ulong_t	pgcp_allocempty[MMU_PAGE_SIZES];
	ulong_t	pgcp_allocok[MMU_PAGE_SIZES];
	ulong_t	ptcp[MMU_PAGE_SIZES];		/* page_trylock_contig_pages */
	ulong_t	ptcpfreethresh[MMU_PAGE_SIZES];
	ulong_t	ptcpfailexcl[MMU_PAGE_SIZES];
	ulong_t	ptcpfailszc[MMU_PAGE_SIZES];
	ulong_t	ptcpfailcage[MMU_PAGE_SIZES];
	ulong_t	ptcpok[MMU_PAGE_SIZES];
	ulong_t	pgmf_alloc[MMU_PAGE_SIZES];	/* page_get_mnode_freelist */
	ulong_t	pgmf_allocfailed[MMU_PAGE_SIZES];
	ulong_t	pgmf_allocempty[MMU_PAGE_SIZES];
	ulong_t	pgmf_allocok[MMU_PAGE_SIZES];
	ulong_t	pgmc_alloc;			/* page_get_mnode_cachelist */
	ulong_t	pgmc_allocfailed;
	ulong_t	pgmc_allocempty;
	ulong_t	pgmc_allocok;
	ulong_t	pladd_free[MMU_PAGE_SIZES];	/* page_list_add/sub */
	ulong_t	plsub_free[MMU_PAGE_SIZES];
	ulong_t	pladd_cache;
	ulong_t	plsub_cache;
	ulong_t	plsubpages_szcbig;
	ulong_t	plsubpages_szc0;
	ulong_t	pff_req[MMU_PAGE_SIZES];	/* page_freelist_fill */
	ulong_t	pff_demote[MMU_PAGE_SIZES];
	ulong_t	pff_coalok[MMU_PAGE_SIZES];
	ulong_t	ppr_reloc[MMU_PAGE_SIZES];	/* page_relocate */
	ulong_t ppr_relocnoroot[MMU_PAGE_SIZES];
	ulong_t ppr_reloc_replnoroot[MMU_PAGE_SIZES];
	ulong_t ppr_relocnolock[MMU_PAGE_SIZES];
	ulong_t ppr_relocnomem[MMU_PAGE_SIZES];
	ulong_t ppr_relocok[MMU_PAGE_SIZES];
	ulong_t page_ctrs_coalesce;	/* page coalesce counter */
	ulong_t page_ctrs_cands_skip;	/* candidates useful */
	ulong_t page_ctrs_changed;	/* ctrs changed after locking */
	ulong_t page_ctrs_failed;	/* page_freelist_coalesce failed */
	ulong_t page_ctrs_coalesce_all;	/* page coalesce all counter */
	ulong_t page_ctrs_cands_skip_all; /* candidates useful for all func */
	ulong_t	restrict4gcnt;
};
extern struct vmm_vmstats_str vmm_vmstats;
#endif	/* VM_STATS */

#ifdef __cplusplus
}
#endif

#endif /* _VM_VM_DEP_H */
