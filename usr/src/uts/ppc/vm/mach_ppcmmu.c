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

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#define	MACH_PPCMMU_IMPL

#include <vm/lint.h>

#include <sys/sysmacros.h>
#include <sys/t_lock.h>
#include <sys/memlist.h>
#include <sys/cpu.h>
#include <vm/seg_kmem.h>
#include <sys/mman.h>
#include <sys/mmu.h>
#include <sys/kmem.h>
#include <sys/pte.h>
#include <sys/debug.h>
#include <sys/vm_machparam.h>
#include <sys/ppc_instr.h>
#include <sys/ppc_subr.h>
#include <sys/sr.h>
#include <vm/hat_ppcmmu.h>
#include <vm/hat.h>
#include <vm/as.h>
#include <vm/page.h>
#include <vm/seg.h>
#include <sys/types.h>
#include <sys/map.h>
#include <sys/cmn_err.h>
#include <sys/vm_machparam.h>
#include <sys/proc.h>
#include <sys/ddidmareq.h>
#include <sys/sysconfig_impl.h>
#include <sys/prom_isa.h>
#include <sys/obpdefs.h>
#include <sys/bootconf.h>
#include <sys/promif.h>
#include <sys/prom_plat.h>
#include <sys/byteorder.h>
#include <sys/systm.h>
#include <sys/bitmap.h>
#include <sys/p2sizes.h>

#define	XXX_POWERPC 1

#if defined(XXX_POWERPC)

/*
 * PAGE_TABLE is the base virtual address of "the" page table.
 */
#define	PAGE_TABLE	ADDRESS_C(0xF0000000)

int segkmem_ready = 0;

#else /* !defined(XXX_POWERPC) */

extern int segkmem_ready;

#endif /* defined(XXX_POWERPC) */

/* PROM memory translation callbacks */
#define	NUM_CALLBACKS	3

/*
 * static data
 */
static caddr_t kern_pt_virt;
static caddr_t kern_pt_phys;
static size_t  kern_pt_size;
static int ppcmmu_kernel_pt_ready = 0;

/* static table for the translations property */
struct trans {
	unsigned int virt;
	unsigned int size;
	unsigned int phys;
	unsigned int mode;
};

/*
 * External functions.
 */
extern void ppcmmu_bat_config(void);
extern void ppcmmu_bat_read_all(void);
extern void ppcmmu_bat_unload_range(caddr_t, size_t, uint_t);
extern batinfo_t *ppcmmu_find_bat(caddr_t);
extern page_t *page_numtopp_nolock(pfn_t pfnum);
page_t *page_numtopp_alloc(pfn_t pfnum);
void ppcmmu_takeover(void);

/*
 * static functions.
 */
static int lomem_check_limits(ddi_dma_lim_t *limits, uint_t lo, uint_t hi);
static caddr_t ppcdevmap(int, int, uint_t);
static int get_translations_prop(struct trans *translations, int get_trans);

/*
 * va_to_pfn_bat(caddr_t vaddr)
 *
 * Given the virtual address, see if it is covered by any BAT register.
 * Returns PFN_INVALID (-1), if there are no BAT translations.
 *
 */
pfn_t
va_to_pfn_bat(caddr_t vaddr)
{
	batinfo_t *bat;
	uintptr_t off;
	caddr_t paddr;

	bat = ppcmmu_find_bat(vaddr);

	if (bat == NULL)
		return (PFN_INVALID);

	off = vaddr - bat->bat_vaddr;
	paddr = bat->bat_paddr + off;
	return ((pfn_t)paddr >> MMU_PAGESHIFT);
}

/*
 * va_to_pfn_prom(caddr_t vaddr)
 *
 * Given the virtual address find the physical page number,
 * using PROM services.
 * returns PFN_INVALID (-1) for invalid mapping.
 *
 */
pfn_t
va_to_pfn_prom(caddr_t vaddr)
{
	pfn_t pfn;
	int rc, mode, valid;

	/*
	 * Use the PROM's MMU translate method since the PROM may
	 * fault the mapping into its page tables when it is accessed
	 * so we may not see it if we go routing directly thru the
	 * PROM's page table
	 */
	rc = prom_translate_virt(vaddr, &valid, &pfn, &mode);
	if (rc == -1 || valid != -1)
		return (PFN_INVALID);
	return (pfn >> MMU_PAGESHIFT);
}

/*
 * va_to_pfn(void *addr)
 *	Given the virtual address find the physical page number,
 *      returns PFN_INVALID (-1) for invalid mapping.
 *      If segkmem_ready != 0 then it simply calls hat_getpfnum().
 */
pfn_t
va_to_pfn(void *addr)
{
	caddr_t vaddr = (caddr_t)addr;
	pfn_t pfn;

	if (segkmem_ready != 0)
		return (hat_getpfnum(curproc->p_as->a_hat, vaddr));
	/*
	 * The hat layer is not set up yet, so check the BAT registers first
	 * and then check the PROM's page tables.
	 */
	pfn = va_to_pfn_bat(vaddr);
	if (pfn != PFN_INVALID)
		return (pfn);
	return (va_to_pfn_prom(vaddr));
}

uint64_t
va_to_pa(void *vaddr)
{
	pfn_t pfn;
	pfn_t base;	// physaddr of start of page containing vaddr
	pfn_t off;	// offset of vaddr with respect to base

	pfn = va_to_pfn(vaddr);
	if (pfn == PFN_INVALID)
		return (PFN_INVALID);
	base = pfn << MMU_PAGESHIFT;
	off = (pfn_t)vaddr & MMU_PAGEOFFSET;
	return (base | off);
}

void
hat_kern_alloc()
{
}

/*
 * Called from before startup() to configure and initialize the MMU.
 *
 * All BAT registers are read in so hat_boot_probe() can consult
 * software BAT information to see what is mapped.
 */

void
ppcmmu_init(void)
{
	ppcmmu_bat_config();
	ppcmmu_bat_read_all();
}

size_t ppc_page_table_sz;

/*
 * Align on natural boundary.
 * That is, round up to the next higher power of 2.
 */

#if defined(_LP64)
#define	align_natural_addr(n) align_natural64(n)
#define	align_natural_size(n) align_natural64(n)
#else
#define	align_natural_addr(n) align_natural32(n)
#define	align_natural_size(n) align_natural32(n)
#endif

uint32_t
align_natural32(uint32_t n)
{
	if (!n)
		return (1);
	if (ISP2(n))
		return (n);
	return (1 << (32 - cntlzw(n)));
}

uint64_t
align_natural64(uint64_t n)
{
	uint64_t msb;
	uint32_t n32;

	if (!n)
		return ((uint64_t)1);
	if (ISP2(n))
		return (n);
	if (n > UINT_MAX) {
		msb = 64;
		n32 = (uint32_t)(n >> 32);
	} else {
		msb = 32;
		n32 = (uint32_t)n;
	}
	return ((uint64_t)1 << (msb - cntlzw(n32)));
}

/*
 * Given the size of physical memory (in pages),
 * compute the recommended page table size.
 *
 * NOTE: For now, Solaris does capacity planning for things like
 * page_t's and page tables, once at boot time.  There are no
 * current plans to have Solaris/PPC for the embedded market
 * do anything fancy with dynamic reconfiguration.  So, we do
 * capacity planning just once.  However, the given size does
 * not have to be the amount of physical memory known to exist
 * at boot time.  If there is some reason to plan for more,
 * then a larger size can be planned for.  It is not a good
 * idea to try to plan for less.
 *
 * See MPCFPE32B, Rev. 3, 2005/09, page 7-41,
 * Section 7.7.1.2, Page Table Size,
 * Table 7-19.  Minimum Recommended Page Table Sizes.
 *
 * But use the table, the explanations, and the examples
 * only to get the general idea, not for the real arithmetic,
 * because there are mistakes in arithmetic in this documentation.
 *
 */

static size_t
set_page_table_size(size_t pmem_pages)
{
	size_t pt_size;

	/* Enough space for PTEs to map 2 * physical memory */
	pt_size = (size_t)align_natural_size(pmem_pages * 2 * sizeof (pte_t));
	if (pt_size < B64K)
		pt_size = B64K;
	return (pt_size);
}

#if 0

/*
 * Initialize kernel variables like nptegp, etc.,
 * based on the information passed from boot.
 */

void
ppcmmu_param_init(void)
{
	ASSERT(physmem != 0);
	kern_pt_virt = (uint_t)PAGE_TABLE;
	kern_pt_size = set_page_table_size(physmem);

	/*
	 * nptegp is used to initialize the hat layer.
	 * It is set here based on the eventual page table
	 * so that other structures can be sized appropriately.
	 */
	nptegp = kern_pt_size / sizeof (pte_group_pair_t);
}

#endif /* 0 */

void
ppcmmu_alloc_pagetable(caddr_t s_avail, caddr_t e_avail, caddr_t *pt, size_t *len)
{
	ASSERT(physinstalled != 0);
	kern_pt_size = set_page_table_size(physinstalled);
	kern_pt_virt = (caddr_t)P2ALIGN((uintptr_t)s_avail, kern_pt_size);
	if (kern_pt_virt + kern_pt_size <= e_avail) {
		*pt = kern_pt_virt;
		*len = kern_pt_size;
	} else {
		*pt = NULL;
		*len = 0;
	}

	/*
	 * nptegp is used to initialize the hat layer.
	 * It is set here so that other structures can be sized appropriately.
	 */
	nptegp = kern_pt_size / sizeof (pte_group_pair_t);
}

#define	PAGE_TABLE_MAP_MODE XXX_MAGIC(0x18)

void
ppcmmu_prom_alloc_pagetable(void)
{

	/*
	 * Allocate space for the kernel's page table.
	 *
	 * prom_allocate_phys does the work of the next three calls but
	 * it assumes default values some places where we want to
	 * use a specific value.
	 */

#if 0
	if (prom_claim_virt(kern_pt_size, (caddr_t)kern_pt_virt)
	    == (caddr_t)-1) {
		prom_printf("prom_claim_virt page table failed\n");
		prom_enter_mon();
	}

	if (prom_allocate_phys(kern_pt_size, kern_pt_size, &kern_pt_phys)
	    == -1) {
		prom_printf("prom_allocate_phys page table failed\n");
		prom_enter_mon();
	}

	if (prom_map_phys(PAGE_TABLE_MAP_MODE, kern_pt_size,
		(caddr_t)kern_pt_virt, 0, kern_pt_phys) == -1)  {
		prom_printf("prom_map_phys page table failed\n");
		prom_enter_mon();
	}

#endif /* 0 */

}

/*
 * This routine will place a mapping in the page table.
 * This is only used by the startup code to grab hold
 * of the MMU before everything is setup.
 * It may also be called as a result of a memory request
 * from the PROM.
 */
void
map_one_page(caddr_t phys, caddr_t vaddr, int mode)
{
	ulong_t hash1, h, i;
	pte_t *pte;
	pte_t new_pte;
	uint_t pte0, new_pte0;
	uint_t pte0_mask;
	uint_t vsid;
	uint_t api;
	pfn_t pfn;
	uint_t wimg;

	/*
	 * We should never be called until the kernel's page table
	 * has been allocated and associated kernel globals initialized.
	 */
	ASSERT(ppcmmu_kernel_pt_ready != 0);

	/*
	 * This routine can be called as a result of a PROM callback,
	 * which could potentially happen after we have set up segkmem.
	 */
	if (segkmem_ready) {
		hat_devload(kas.a_hat, vaddr, MMU_PAGESIZE,
		    (ulong_t)(phys) >> MMU_PAGESHIFT,
		    PROT_READ|PROT_WRITE|PROT_EXEC, HAT_LOAD_LOCK);
		return;
	}

	/*
	 * Fill in the software PTE
	 */
	vsid = VSID(KERNEL_VSID_RANGE, vaddr);
	api = VADDR_TO_API(vaddr);
	pfn = (pfn_t)phys >> MMU_PAGESHIFT;
	wimg = mode >> XXX_MAGIC(3);

	if (pfn > PTE1_RPN_VAL_MASK) {
		panic("map_one_page: pfn=%lx is too big (> %lx)",
			pfn, (pfn_t)PTE1_RPN_VAL_MASK);
	}

	// v, vsid, h, api
	new_pte.ptew[PTEWORD0] = PTE0_NEW(PTE_VALID, vsid, 0, api);
	// rpn, xpn, r, c, wimg, x, pp
	new_pte.ptew[PTEWORD1] = PTE1_NEW(pfn, 0, 0, 0, wimg, 0, MMU_STD_SRWX);

	/* Compute primary from vaddr */
	hash1 = fqva_to_hash1(vsid, vaddr);
	pte0_mask = PTE0_VSID_FLD_MASK | PTE0_H_FLD_MASK | PTE0_API_FLD_MASK;
	/* Search for an invalid page table entry, then use it */
	h = hash1;
	pte = hash_get_primary(h);
	for (i = 0; (h == hash1) || i < NPTEPERPTEG; ++pte, ++i) {
		if (i == NPTEPERPTEG) {
			/*
			 * Switch to secondary hash table
			 */
			i = 0;
			h = hash1 ^ hash_pteg_mask;
			pte = hash_get_secondary(hash1);
		}
		if (!ptep_isvalid(pte)) {
			/*
			 * Found one.  Update the PTE
			 */
			ptep_set_h(&new_pte, (h != hash1));
			mmu_update_pte(pte, &new_pte, 0, vaddr);
#ifdef PRINT_MAPS
			prom_printf("P: 0x%x V: 0x%x PTE: 0x%x: 0x%x 0x%x\n",
				phys, vaddr, pte,
				pte->ptew[PTEWORD0], pte->ptew[PTEWORD1]);
#endif /* PRINT_MAPS */
			return;
		}
		/*
		 * Make sure vaddr is not already mapped.
		 */
		pte0 = pte->ptew[PTEWORD0];
		new_pte0 = new_pte.ptew[PTEWORD0];
		if ((pte0 & pte0_mask) == (new_pte0 & pte0_mask)) {
#if defined(DEBUG)
			prom_printf("map_one_page: Duplicate PTE.\n");
			prom_printf("Not mapping virtual address %p\n",
				vaddr);
#endif /* DEBUG */
			return;
		}
	}
	panic("map_one_page: no available PTE found");
}

/*
 * unmap a page.
 * This routine should only be called as a result of PROM callback
 * or early on during startup whilst we are taking over the mmu.
 * Note that is should never be called before the kernel's page table
 * and associated kernel globals have been initialized.
 */
void
unmap_one_page(caddr_t vaddr)
{
	pte_t *pte;
	ulong_t hash1, h, i;
	uint_t pte0_mask;
	uint_t key;
	uint_t vsid;
	uint_t api;

	ASSERT(ppcmmu_kernel_pt_ready != 0);

	/*
	 * If segkmem has been initialized, then we can use
	 * the standard methods to unmap the memory.
	 */
	if (segkmem_ready) {
		hat_unlock(kas.a_hat, vaddr, MMU_PAGESIZE);
		hat_unload(kas.a_hat, vaddr, MMU_PAGESIZE, HAT_UNLOAD);
		return;
	}

	/* Compute primary from vaddr */
	vsid = VSID(KERNEL_VSID_RANGE, vaddr);
	hash1 = fqva_to_hash1(vsid, vaddr);
	h = hash1;
	pte = hash_get_primary(h);

	pte0_mask = PTE0_VSID_FLD_MASK | PTE0_H_FLD_MASK | PTE0_API_FLD_MASK;
	api = VADDR_TO_API(vaddr);
	key = PTE0_NEW(0, vsid, 0, api);
	/* Search for the matching page table entry, then invaldiate it */
	for (i = 0; (h == hash1) || i < NPTEPERPTEG; ++pte, ++i) {
		if (i == NPTEPERPTEG) {
			/* Switch to the secondary hash table */
			i = 0;
			h = hash1 ^ hash_pteg_mask;
			pte = hash_get_secondary(hash1);
			key |= PTE0_H_FLD_MASK;
		}
		if (!ptep_isvalid(pte))
			continue;
		if ((pte->ptew[PTEWORD0] & pte0_mask) == key) {
			mmu_delete_pte(pte, vaddr);
			return;
		}
	}
	cmn_err(CE_WARN, "unmap_one_page: couldn't find mapping for %p\n",
		vaddr);
}

/*
 * This routine is called from the PROM when a mapping is being
 * created.  It is the PROM's way of asking the kernel to map pages
 * in the page table.  This is really only used after we have taken
 * over memory management - if we call one of the PROM services,
 * we need to provide a way for the PROM to allocate memory if it
 * needs to.
 */
static int
map_callback(uint_t *args, int num_args, uint_t *ret)
{
	uint_t phys, virt, size, mode;

	ASSERT(num_args >= 0);

	phys = args[0] &= MMU_PAGEMASK;	/* round down phys addr */
	virt = args[1];
	size = args[2];
	mode = args[3];
	size = btopr((virt + size) - (virt & MMU_PAGEMASK));
	virt &= MMU_PAGEMASK;

	/* Create PTEs for this mapping */
	for (; size; virt += MMU_PAGESIZE, phys += MMU_PAGESIZE, --size) {
		map_one_page((caddr_t)phys, (caddr_t)virt, mode);
	}

	return (*ret = 0);
}

/*
 * PROM callback to unmap a page
 */
static int
unmap_callback(uint_t *args, int num_args, uint_t *ret)
{
	uint_t virt, size;

	ASSERT(num_args >= 0);

	virt = args[0];
	size = args[1];

	size = btopr((virt + size) - (virt & MMU_PAGEMASK));
	virt &= MMU_PAGEMASK;

	/* Remove PTEs for this mapping */
	for (; size; virt += MMU_PAGESIZE, --size) {
		unmap_one_page((caddr_t)virt);
	}

	return (*ret = 0);
}
/*
 * PROM callback
 * Given a virtual address find the physical address it maps and the mode
 */
static int
translate_callback(uint_t *args, int num_args, uint_t *ret)
{
	ulong_t hash1, h, i;
	caddr_t vaddr;
	pte_t *pte;
	uint_t pte0_mask;
	uint_t key;
	uint_t vsid;
	uint_t api;
	pfn_t pfn;
	pfn_t paddr;
	int rc;

	ASSERT(num_args >= 0);

	vaddr = (caddr_t)args[0];
	rc = -1;

	/*
	 * We probably should not be seeing any requests from the
	 * PROM for addresses mapped by BATs.  The map_callback() routine
	 * maps everything in the page table.  And, at the time we take
	 * over memory management, all mappings are placed in the
	 * page table.  Although the kernel can create mappings using
	 * BATs, the PROM should not be interested in these mappings.
	 */
	ASSERT(ppcmmu_find_bat(vaddr) == NULL);

	pte0_mask = PTE0_VSID_FLD_MASK | PTE0_H_FLD_MASK | PTE0_API_FLD_MASK;
	vsid = VSID(KERNEL_VSID_RANGE, vaddr);
	api = VADDR_TO_API(vaddr);

	/* Compute primary from vaddr */
	hash1 = fqva_to_hash1(vsid, vaddr);

	/* Search for the matching page table entry */
	h = hash1;
	pte = hash_get_primary(h);
	key = PTE0_NEW(0, vsid, 0, api);
	for (i = 0; (h == hash1) || i < NPTEPERPTEG; ++pte, ++i) {
		if (i == NPTEPERPTEG) {
			/*
			 * Switch to the secondary hash table
			 */
			i = 0;
			h = hash1 ^ hash_pteg_mask;
			pte = hash_get_secondary(hash1);
			key |= PTE0_H_FLD_MASK;
		}

		if (!ptep_isvalid(pte))
			continue;

		if ((pte->ptew[PTEWORD0] & pte0_mask) == key) {
			pfn = ptep_get_rpn(pte);
			paddr = (pfn << MMU_PAGESHIFT) | ((pfn_t)vaddr & MMU_PAGEOFFSET);
			// XXX ret[1] = (uint_t)(paddr >> 32);	/* physical address high */
			ret[1] = 0;
			ret[2] = (uint_t)paddr;	// XXX questionable fit
			/* mode */
			ret[3] = pte->ptew[PTEWORD1] &
				(PTE1_WIMG_FLD_MASK | PTE1_PP_FLD_MASK);
			rc = 0;
			break;
		}
	}
	ret[0] = rc;
	return (rc);
}

/*
 * This routine sets up callbacks for memory management from the PROM.
 */
void
setup_prom_callbacks()
{
	extern int init_callback_handler(struct callbacks *prom_callbacks);
	static struct callbacks prom_callbacks[NUM_CALLBACKS + 1] = {
		"map",		map_callback,
		"unmap",	unmap_callback,
		"translate",	translate_callback,
		NULL, NULL
	};

	init_callback_handler(prom_callbacks);
}

/*
 * Initialize all segment registers.
 * At first, all segment registers are for use by the kernel.
 * When a usrland process runs, it gets to use sr0..sr13.
 */

void
init_segregs(void)
{
	uint_t segval;
	/*		T KU KS  N  VSID */
	segval = SR_NEW(0, 1, 0, 0, VSID(KERNEL_VSID_RANGE, 0));
	mtsr(0, segval);
	mtsr(1, segval + 1);
	mtsr(2, segval + 2);
	mtsr(3, segval + 3);
	mtsr(4, segval + 4);
	mtsr(5, segval + 5);
	mtsr(6, segval + 6);
	mtsr(7, segval + 7);
	mtsr(8, segval + 8);
	mtsr(9, segval + 9);
	mtsr(10, segval + 10);
	mtsr(11, segval + 11);
	mtsr(12, segval + 12);
	mtsr(13, segval + 13);
	mtsr(14, segval + 14);
	mtsr(15, segval + 15);
	ppc_sync();
}

/*
 * Take over memory management from the PROM.
 * We accomplish this by doing the following steps:
 *	- Allocate new page table memory (already done in ppcmmu_init)
 * 	- Copy the translations into the new page table
 *	- Install PROM callbacks in case anyone calls prom_map etc
 * 	- Set appropriate global variables
 * 	- Set the SDR1 register to point to new page table
 */

void
ppcmmu_takeover()
{
	uint_t sdr1;
	uint_t htabmask;
	int num_translations;
	uint_t trans_tbl_sz;
	struct trans *tp, *trans_ent;

	/*
	 * Zero out page table
	 */
	bzero((caddr_t)kern_pt_virt, (size_t)kern_pt_size);

	/*
	 * Setup for kernel Page Table usage
	 */
	// XXX Calculate based on actual pagetable size.
	nptegp = 512;		   /* minimum PTEG pairs */

	// XXX Provide for newer fields in SDR1.
	// XXX Handle extended bits for larger page tables.
	sdr1 = (uintptr_t)kern_pt_phys + ((kern_pt_size >> XXX_MAGIC(16)) - 1);
	htabmask =  sdr1 & SDR1_HTABMASK;
	nptegp  *= htabmask + 1;
	nptegmask = (2 * nptegp) - 1;
	hash_pteg_mask = (htabmask << XXX_MAGIC(10)) | XXX_MAGIC(0x3FF);
	ptes = (pte_t *)kern_pt_virt;
	eptes = (pte_t *)((uint_t)kern_pt_virt + kern_pt_size);
	mptes = (pte_t *)((uint_t)ptes + (kern_pt_size >> 1));
	ppcmmu_kernel_pt_ready = 1;

	/*
	 * Copy current translations into new page table.
	 */

	/*
	 * Just get the current number of translations for now.
	 */
	num_translations = get_translations_prop(NULL, 0);

	/*
	 * Create a table to hold the translations property.
	 * We add an extra page to the table to deal with extra translations
	 * that will be created as a result of allocating a table to
	 * contain the translations.
	 */
	trans_tbl_sz = num_translations * sizeof (struct trans);
	trans_tbl_sz = roundup(trans_tbl_sz, MMU_PAGESIZE) + MMU_PAGESIZE;

	tp = (struct trans *)prom_alloc(0, trans_tbl_sz, MMU_PAGESIZE);
	trans_ent = tp;

	/*
	 * Now fill in our buffer
	 * with the contents of the translations property
	 */
	num_translations = get_translations_prop(tp, 1);

	ASSERT(num_translations <= (trans_tbl_sz / sizeof (struct trans)));

	while (num_translations--) {
		int size;
		uint_t vaddr, paddr, endvaddr, npages;

		/* Convert size, if necessary */
		size = htonl(trans_ent->size);

		/*
		 * Skip over BAT mappings.  There really should be none.
		 */
		if (size >= B256M) {
			++trans_ent;
			continue;
		}

		/* Create PTEs for this mapping */
		vaddr = htonl(trans_ent->virt);	/* convert virt */
		paddr = htonl(trans_ent->phys) & MMU_PAGEMASK;
		endvaddr = vaddr + size;
		vaddr &= MMU_PAGEMASK;
		npages = btopr(endvaddr - vaddr);
#ifdef PRINT_TRANS
		prom_printf("V: 0x%x P: 0x%x pages: 0x%x mode: 0x%x\n",
		    vaddr, paddr, npages, htonl(trans_ent->mode));
#endif /* PRINT_TRANS */

		for (; npages--; vaddr += MMU_PAGESIZE, paddr += MMU_PAGESIZE) {
			/*
			 * Skip over the mappings for the translations table,
			 * since we will unmap it when we are done copying
			 * the translations.
			 */
			if ((vaddr >= (uint_t)tp) &&
			    (vaddr < ((uint_t)tp + trans_tbl_sz))) {
				continue;
			}
			map_one_page((caddr_t)paddr, (caddr_t)vaddr,
			    htonl(trans_ent->mode));
		}
		++trans_ent;
	}

	/*
	 * Free the translations table.
	 */
	prom_free((caddr_t)tp, trans_tbl_sz);

	/*
	 * If we expect to have anymore calls to prom_map, etc.,
	 * we need to have the call backs setup so the PROM can
	 * tell us that a new mapping has occurred.
	 */
	setup_prom_callbacks();

	init_segregs();
	/*
	 * Set SDR1 to point to the new page table.
	 */
	mtsdr1(sdr1);
	ppc_sync();
}

/*
 * Update hat data structures (hmes, ptegp, etc.)
 * to reflect the mappings in the page table.
 * Also we need to lock all the kernel mappings in the page table.
 *
 * NOTE:
 *	Normally all the mappings in the page table should be for the virtual
 *	addresses above KERNELBASE, except the mappings created for Boot
 *	(i.e. boot-text/boot-data) and PROM mappings,
 *	which are unloaded later in the startup().
 */
void
hat_kern_setup()
{
	pte_t *pte;
	hme_t *hme;
	ptegp_t *ptegp;
	uint_t mask;
	int i;
	caddr_t vaddr;

	hat_enter(kas.a_hat);

	hme = pte_to_hme(ptes);
	pte = ptes;
	while (pte < eptes) {
		ptegp = pte_to_ptegp(pte);
		mask = ((pte >= mptes) ? 0x100 : 1);
		for (i = 0; i < NPTEPERPTEG; ++pte, ++hme, ++i) {
			if (!ptep_isvalid(pte))
				continue;
			hme->hme_valid = 1;
			ptegp->ptegp_validmap |= (mask << i);
			ptelock(ptegp, pte);
		}
	}

#if defined(XXX_OBSOLETE_SOLARIS)

	/*
	 * Update Sysmap[] PTE array
	 */
	for (vaddr = SYSBASE; vaddr < SYSLIMIT; vaddr += MMU_PAGESIZE) {
		pte = ppcmmu_ptefind(&kas.a_hat, vaddr, PTEGP_NOLOCK);
		if (pte == NULL)
			continue;
		pte_to_spte(pte, &Sysmap[mmu_btop(addr - SYSBASE)]);
	}

#endif /* defined(XXX_OBSOLETE_SOLARIS) */

	hat_exit(kas.a_hat);
}

/*
 * Unload the mappings created by Boot for virtual addresses below KERNELBASE
 * (i.e boot-text/boot-data, etc.).
 *
 * After this the boot services are no longer available.
 */
void
unload_boot()
{
	ppcmmu_bat_unload_range(0, KERNELBASE, BATS_ALL);
}

/*
 * Kernel lomem memory allocation/freeing
 */

static struct lomemlist {
	struct lomemlist	*lomem_next;	/* next in a list */
	ulong_t		lomem_paddr;	/* base kernel virtual */
	ulong_t		lomem_size;	/* size of space (bytes) */
} lomemusedlist, lomemfreelist;

static kmutex_t	lomem_lock;		/* mutex protecting lomemlist data */
static int lomemused, lomemmaxused;
static caddr_t lomem_startva;
/*LINTED static unused */
static caddr_t lomem_endva;
static ulong_t  lomem_startpa;
static kcondvar_t lomem_cv;
static ulong_t  lomem_wanted;

/*
 * Space for low memory (below 16 meg), contiguous, memory
 * for DMA use (allocated by ddi_iopb_alloc()).  This default,
 * changeable in /etc/system, allows 2 64K buffers, plus space for
 * a few more small buffers for (e.g.) SCSI command blocks.
 */
#define	LOMEM_DMA_SIZE (2 * 64 * 1024)
long lomempages = (LOMEM_DMA_SIZE / MMU_PAGESIZE) + 4;

#ifdef DEBUG
static int lomem_debug = 0;
#endif /* DEBUG */

void
lomem_init()
{
	int nfound;
	int pfn;
	struct lomemlist *dlp;
	int biggest_group = 0;

	/*
	 * Try to find lomempages pages of contiguous memory below 16meg.
	 * If we can't find lomempages, find the biggest group.
	 * With 64K alignment being needed for devices that have
	 * only 16 bit address counters (next byte fixed), make sure
	 * that the first physical page is 64K aligned; if lomempages
	 * is >= 16, we have at least one 64K segment that doesn't
	 * span the address counter's range.
	 */

again:
	if (lomempages <= 0)
		return;

	for (nfound = 0, pfn = 0; pfn < btop(B16M); ++pfn) {
		if (page_numtopp_alloc(pfn) == NULL) {
			/* Encountered an unallocated page. Back out.  */
			if (nfound > biggest_group)
				biggest_group = nfound;
			for (; nfound; --nfound) {
				/*
				 * XXX Fix Me: Cannot page_free here.
				 * Causes it to go to sleep!!
				 */
				page_free(page_numtopp_nolock(pfn - nfound), 1);
			}
			/*
			 * nfound is back to zero to continue search.
			 * Bump pfn so next pfn is on a 64Kb boundary.
			 */
			pfn |= (btop(B64K) - 1);
		} else {
			if (++nfound >= lomempages)
				break;
		}
	}

	if (nfound < lomempages) {

		/*
		 * Ran beyond 16 meg.  pfn is last in group + 1.
		 * This is *highly* unlikely, as this search happens
		 *   during startup, so there should be plenty of
		 *   pages below 16mb.
		 */

		if (nfound > biggest_group)
			biggest_group = nfound;

		cmn_err(CE_WARN, "lomem_init: obtained only %d of %d pages.\n",
				biggest_group, (int)lomempages);

		if (nfound != biggest_group) {
			/*
			 * The last group isn't the biggest.
			 * Free it and try again for biggest_group.
			 */
			for (; nfound; --nfound) {
				page_free(page_numtopp_nolock(pfn-nfound), 1);
			}
			lomempages = biggest_group;
			goto again;
		}

		--pfn;	/* Adjust to be pfn of last in group */
	}


	/* pfn is last page frame number; compute  first */
	pfn -= (nfound - 1);
	lomem_startva = ppcdevmap(pfn, nfound, PROT_READ|PROT_WRITE);
	lomem_endva = lomem_startva + ptob(lomempages);

	/* Set up first free block */
	lomemfreelist.lomem_next = dlp =
		(struct lomemlist *)kmem_alloc(sizeof (struct lomemlist), 0);
	dlp->lomem_next = NULL;
	dlp->lomem_paddr = lomem_startpa = ptob(pfn);
	dlp->lomem_size  = ptob(nfound);

	/*
	 * Downgrade the page locks to shared locks
	 * instead of exclusive locks.
	 */
	for (; nfound; --nfound) {
		page_downgrade(page_numtopp_nolock(pfn));
		++pfn;
	}

#ifdef DEBUG
	if (lomem_debug)
		printf("lomem_init: %d pages, phys=%x virt=%x\n",
		    (int)lomempages, (int)dlp->lomem_paddr,
		    (int)lomem_startva);
#endif /* DEBUG */

	mutex_init(&lomem_lock, "lomem_lock", MUTEX_DEFAULT, NULL);
	cv_init(&lomem_cv, "lomem_cv", CV_DEFAULT, NULL);
}

/*
 * Allocate contiguous, memory below 16meg.
 * Only used for ddi_iopb_alloc (and ddi_memm_alloc) - os/ddi_impl.c.
 */
caddr_t
lomem_alloc(nbytes, limits, align, cansleep)
	uint_t nbytes;
	ddi_dma_lim_t *limits;
	int align;
	int cansleep;
{
	struct lomemlist *dlp;	/* lomem list ptr scans free list */
	struct lomemlist *dlpu; /* New entry for used list if needed */
	struct lomemlist *dlpf;	/* New entry for free list if needed */
	struct lomemlist *pred;	/* Predecessor of dlp */
	struct lomemlist *bestpred = NULL;
	ulong_t left, right;
	ulong_t leftrounded, rightrounded;

	if (align > 16) {
		cmn_err(CE_WARN, "lomem_alloc: align > 16\n");
		return (NULL);
	}

	if ((dlpu = (struct lomemlist *)kmem_alloc(sizeof (struct lomemlist),
		cansleep ? 0 : KM_NOSLEEP)) == NULL)
			return (NULL);

	/* In case we need a second lomem list element ... */
	if ((dlpf = (struct lomemlist *)kmem_alloc(sizeof (struct lomemlist),
		cansleep ? 0 : KM_NOSLEEP)) == NULL) {
			kmem_free(dlpu, sizeof (struct lomemlist));
			return (NULL);
	}

	/* Force 16-byte multiples and alignment; great simplification. */
	align = 16;
	nbytes = (nbytes + XXX_MAGIC(15)) & (~XXX_MAGIC(15));

	mutex_enter(&lomem_lock);

again:
	for (pred = &lomemfreelist; (dlp = pred->lomem_next) != NULL;
	    pred = dlp) {
		/*
		 * The criteria for choosing lomem space are:
		 *   1. Leave largest possible free block after allocation.
		 *	From this follows:
		 *		a. Use space in smallest usable block.
		 *		b. Avoid fragments (i.e., take from end).
		 *	Note: This may mean that we fragment a smaller
		 *	block when we could have allocated from the end
		 *	of a larger one, but c'est la vie.
		 *
		 *   2. Prefer taking from right (high) end.  We start
		 *	with 64Kb aligned space, so prefer not to break
		 *	up the first chunk until we have to.  In any event,
		 *	reduce fragmentation by being consistent.
		 */
		if (dlp->lomem_size < nbytes ||
			(bestpred &&
			dlp->lomem_size > bestpred->lomem_next->lomem_size))
				continue;

		left = dlp->lomem_paddr;
		right = dlp->lomem_paddr + dlp->lomem_size;
#if defined(XXX_OBSOLETE_DDI)
		leftrounded = ((left + limits->dlim_adreg_max - 1) &
						~limits->dlim_adreg_max);
		rightrounded = right & ~limits->dlim_adreg_max;
#endif /* XXX_OBSOLETE_DDI */

		/*
		 * See if this block will work, either from left,
		 * from right, or after rounding up left to be on an "address
		 * increment" (dlim_adreg_max) boundary.
		 */
		if (lomem_check_limits(limits, right - nbytes, right - 1) ||
		    lomem_check_limits(limits, left, left + nbytes - 1) ||
		    (leftrounded + nbytes <= right &&
			lomem_check_limits(limits, leftrounded,
						leftrounded + nbytes - 1))) {
			bestpred = pred;
		}
	}

	if (bestpred == NULL) {
		if (cansleep) {
			if (lomem_wanted == 0 || nbytes < lomem_wanted)
				lomem_wanted = nbytes;
			cv_wait(&lomem_cv, &lomem_lock);
			goto again;
		}
		mutex_exit(&lomem_lock);
		kmem_free(dlpu, sizeof (struct lomemlist));
		kmem_free(dlpf, sizeof (struct lomemlist));
		return (NULL);
	}

	/* bestpred is predecessor of block we're going to take from */
	dlp = bestpred->lomem_next;

	if (dlp->lomem_size == nbytes) {
		/* Perfect fit.  Just use whole block. */
		ASSERT(lomem_check_limits(limits,  dlp->lomem_paddr,
				dlp->lomem_paddr + dlp->lomem_size - 1));
		bestpred->lomem_next = dlp->lomem_next;
		dlp->lomem_next = lomemusedlist.lomem_next;
		lomemusedlist.lomem_next = dlp;
	} else {
		left = dlp->lomem_paddr;
		right = dlp->lomem_paddr + dlp->lomem_size;
#if defined(XXX_OBSOLETE_DDI)
		leftrounded = ((left + limits->dlim_adreg_max - 1) &
						~limits->dlim_adreg_max);
		rightrounded = right & ~limits->dlim_adreg_max;
#endif /* XXX_OBSOLETE_DDI */

		if (lomem_check_limits(limits, right - nbytes, right - 1)) {
			/* Take from right end */
			dlpu->lomem_paddr = right - nbytes;
			dlp->lomem_size -= nbytes;
		} else if (lomem_check_limits(limits, left, left + nbytes - 1)) {
			/* Take from left end */
			dlpu->lomem_paddr = left;
			dlp->lomem_paddr += nbytes;
			dlp->lomem_size -= nbytes;
		} else if (rightrounded - nbytes >= left &&
			lomem_check_limits(limits, rightrounded - nbytes,
							rightrounded - 1)) {
			/* Take from right after rounding down */
			dlpu->lomem_paddr = rightrounded - nbytes;
			dlpf->lomem_paddr = rightrounded;
			dlpf->lomem_size  = right - rightrounded;
			dlp->lomem_size -= (nbytes + dlpf->lomem_size);
			dlpf->lomem_next = dlp->lomem_next;
			dlp->lomem_next  = dlpf;
			dlpf = NULL;	/* Don't free it */
		} else {
			ASSERT(leftrounded + nbytes <= right &&
				lomem_check_limits(limits, leftrounded,
						leftrounded + nbytes - 1));
			/* Take from left after rounding up */
			dlpu->lomem_paddr = leftrounded;
			dlpf->lomem_paddr = leftrounded + nbytes;
			dlpf->lomem_size  = right - dlpf->lomem_paddr;
			dlpf->lomem_next  = dlp->lomem_next;
			dlp->lomem_size = leftrounded - dlp->lomem_paddr;
			dlp->lomem_next  = dlpf;
			dlpf = NULL;	/* Don't free it */
		}
		dlp = dlpu;
		dlpu = NULL;	/* Don't free it */
		dlp->lomem_size = nbytes;
		dlp->lomem_next = lomemusedlist.lomem_next;
		lomemusedlist.lomem_next = dlp;
	}

	if ((lomemused += nbytes) > lomemmaxused)
		lomemmaxused = lomemused;

	mutex_exit(&lomem_lock);

	if (dlpu) kmem_free(dlpu, sizeof (struct lomemlist));
	if (dlpf) kmem_free(dlpf, sizeof (struct lomemlist));

#if defined(DEBUG)
	if (lomem_debug) {
		printf("lomem_alloc: alloc paddr 0x%x size %d\n",
		    (int)dlp->lomem_paddr, (int)dlp->lomem_size);
	}
#endif /* defined(DEBUG) */
	return (lomem_startva + (dlp->lomem_paddr - lomem_startpa));
}

static int
lomem_check_limits(ddi_dma_lim_t *limits, uint_t lo, uint_t hi)
{
#if defined(XXX_OBSOLETE_DDI)
	return (lo >= limits->dlim_addr_lo && hi <= limits->dlim_addr_hi &&
		((hi & ~(limits->dlim_adreg_max)) ==
			(lo & ~(limits->dlim_adreg_max))));
#else
	return (0);
#endif /* !defined(XXX_OBSOLETE_DDI) */
}

void
lomem_free(kaddr)
	caddr_t kaddr;
{
	struct lomemlist *dlp, *pred, *dlpf;
	ulong_t paddr;

	/* Convert kaddr from virtual to physical */
	paddr = (kaddr - lomem_startva) + lomem_startpa;

	mutex_enter(&lomem_lock);

	/* Find the allocated block in the used list */
	for (pred = &lomemusedlist; (dlp = pred->lomem_next) != NULL;
	    pred = dlp)
		if (dlp->lomem_paddr == paddr)
			break;

	if (dlp->lomem_paddr != paddr) {
		cmn_err(CE_WARN, "lomem_free: bad addr=0x%x paddr=0x%x\n",
			(int)kaddr, (int)paddr);
		return;
	}

	lomemused -= dlp->lomem_size;

	/* Remove from used list */
	pred->lomem_next = dlp->lomem_next;

	/* Insert/merge into free list */
	for (pred = &lomemfreelist; (dlpf = pred->lomem_next) != NULL;
	    pred = dlpf) {
		if (paddr <= dlpf->lomem_paddr)
			break;
	}

	/* Insert after pred; dlpf may be NULL */
	if (pred->lomem_paddr + pred->lomem_size == dlp->lomem_paddr) {
		/* Merge into pred */
		pred->lomem_size += dlp->lomem_size;
		kmem_free(dlp, sizeof (struct lomemlist));
	} else {
		/* Insert after pred */
		dlp->lomem_next = dlpf;
		pred->lomem_next = dlp;
		pred = dlp;
	}

	if (dlpf &&
		pred->lomem_paddr + pred->lomem_size == dlpf->lomem_paddr) {
		pred->lomem_next = dlpf->lomem_next;
		pred->lomem_size += dlpf->lomem_size;
		kmem_free(dlpf, sizeof (struct lomemlist));
	}

	if (pred->lomem_size >= lomem_wanted) {
		lomem_wanted = 0;
		cv_broadcast(&lomem_cv);
	}

	mutex_exit(&lomem_lock);

#if defined(DEBUG)
	if (lomem_debug) {
		printf("lomem_free: freeing addr 0x%x -> addr=0x%x, size=%d\n",
		    (int)paddr, (int)pred->lomem_paddr, (int)pred->lomem_size);
	}
#endif /* defined(DEBUG) */
}

caddr_t
ppcdevmap(pf, npf, prot)
	int pf;
	int npf;
	uint_t prot;
{
	caddr_t addr;

#if defined(XXX_OBSOLETE)
	// XXX kmxtob  is obsolete
	// XXX rmalloc is obsolete
	addr = (caddr_t)kmxtob(rmalloc(kernelmap, npf));
	segkmem_mapin(&kvseg, addr, npf * MMU_PAGESIZE, prot | HAT_NOSYNC, pf,
				HAT_LOAD_LOCK);
#endif /* XXX_OBSOLETE */
	return (addr);
}

/*
 * This routine is like page_numtopp, but accepts only free pages, which
 * it allocates (unfrees) and returns with the exclusive lock held.
 * It is used by machdep.c/dma_init() to find contiguous free pages.
 */
page_t *
page_numtopp_alloc(pfn_t pfnum)
{
	page_t *pp;

	pp = page_numtopp_nolock(pfnum);
	if (pp == NULL)
		return ((page_t *)NULL);

	if (!page_trylock(pp, SE_EXCL))
		return (NULL);

	if (!PP_ISFREE(pp)) {
		page_unlock(pp);
		return (NULL);
	}

	/* If associated with a vnode, destroy mappings */

	if (pp->p_vnode) {

		page_destroy_free(pp);

		if (!page_lock(pp, SE_EXCL, (kmutex_t *)NULL, P_NO_RECLAIM))
			return (NULL);
	}

	if (!PP_ISFREE(pp) || !page_reclaim(pp, (kmutex_t *)NULL)) {
		page_unlock(pp);
		return (NULL);
	}

	return (pp);
}

/*
 * Get the translations property - we cache the phandle for the
 * mmu node for future callers.
 * If get_prop is set, we assume translations is a valid buffer and
 * retrieve the property.  Otherwise, we just return the length.
 *
 * Note that checking (translations == NULL) is not a good check to
 * see if we have a valid buffer since this buffer may be alloc'd
 * by prom_alloc() - the PROM may not have a problem with returning
 * a buffer at addr 0x0.
 */
static int
get_translations_prop(struct trans *translations, int get_prop)
{

	int len;
	pnode_t mmunode = (pnode_t)NULL;
	ihandle_t node;

	if (mmunode == (pnode_t)NULL) {
		node = prom_mmu_ihandle();
		ASSERT(node != (ihandle_t)-1);

		mmunode = (pnode_t)prom_getphandle(node);
	}
	ASSERT(mmunode != (pnode_t)-1);

	len = prom_getproplen(mmunode, "translations");

	if (len <= 0) {
		return (-1);
	}
	/*
	 * Is the caller only interested in the length?
	 */
	if (get_prop == 0) {
		/* Each translations entry consists of 4 integers */
		return (len >> 4);
	}

	/*
	 * Fill in our buffer
	 */
	if (prom_getprop(mmunode, "translations", (caddr_t)translations) < 0) {
		return (-1);
	} else {
		/* Each translations entry consists of 4 integers */
		return (len >> 4);    /* number of translation entries */
	}
}

/*
 * Low level functions for PowerPC MMU.
 *
 * These functions assume 32-bit implementation of PowerPC.
 */


#if defined(MP)

/*
 * TLB flush.
 * On PowerPC the 'tblie' or 'tlbsync' can be executed on only one processor.
 * So, this code is under the ppcmmu_tlbflush_lock spin lock.
 *
 * Algorithm:
 *	Save interrupt status (MSR:EE)
 * 	Disable interrupts (MSR:EE = 0)
 * 	If (ncpus == 1) then
 *		tlbie
 *		sync
 *	Else
 *		Get the ppcmmu_tlbflush_lock
 *		tlbie
 *		sync
 *		Release the ppcmmu_tlbflush_lock
 *	Restore interrupt status (MSR:EE)
 */

uint_t ppcmmu_tlbflush_lock = 0;

static __inline__ void tlbflush(caddr_t addr) {
	uint_t msr_save, msr_dis;
	uint_t val;

	msr_save = mfmsr();
	msr_dis = MSR_SET_EE(msr_save, 0);
	mtmsr(msr_dis);
	if (ncpus == 1) {
		tlbie(addr);
		ppc_sync();
	} else {
		uint_t *lckp = &ppcmmu_tlbflush_lock;
		uint_t lck;

		__asm__(
		"1:	lwarx	%[lck], 0, %[lckp]\n"
		"	cmpi	%[lck], 0\n"
		"	bne-	1b\n"
		"	li	%[lck], 0xff\n"
		"	stwcx.	%[lck], 0, %[lckp]\n"
		"	bne-	1b\n"
		"	tlbie	%[addr]\n"
		"	sync\n"
		"	li	%[lck], 0\n"
		"	stw	%[lck], %[lckp]\n"
		: /* No outputs */
		: [lckp] "r" (lckp), [lck] "r" (lck), [addr] "r" (addr)
		: "cc"
		/* */);
	mtmsr(msr_save);
}

#else /* !defined(MP) */

/*
 * TLB flush
 *
 * Algorithm:
 *	Save interrupt status (MSR:EE)
 * 	Disable interrupts (MSR:EE = 0)
 *	tlbie
 *	sync
 *	Restore interrupt status (MSR:EE)
 */

static __inline__ void tlbflush(caddr_t addr) {
	uint_t msr_save, msr_dis;
	uint_t val;

	msr_save = mfmsr();
	msr_dis = MSR_SET_EE(msr_save, 0);
	mtmsr(msr_dis);
	tlbie(addr);
	ppc_sync();
	mtmsr(msr_save);
}

#endif /* defined(MP) */

/*
 * mmu_update_pte(pte_t *pte, pte_t *new_pte, int remap, caddr_t vaddr)
 *	Update the location of a PTE in hte page table
 *	with the values supplied for a new mapping.
 *	Steps:	if (remap)
 *			Invalidate the old mapping
 *			Sync
 *			TLB flush for the entry.
 *			Sync
 *			Tlbsync and sync
 *			Or in the old rm bits into the new pte.
 *		Write the new hw pte without valid bit set
 *		Sync
 *		Update the valid bit
 */

void
mmu_update_pte(pte_t *pte, pte_t *new_pte, uint_t remap, caddr_t vaddr)
{
	uint_t new0, new1, refchg_mask, refchg;

	new0 = new_pte->ptew[PTEWORD0];
	new1 = new_pte->ptew[PTEWORD1];
	refchg_mask = PTE1_R_FLD_MASK | PTE1_C_FLD_MASK;
	refchg = new1 & refchg_mask;

	ASSERT(pte_in_pagetable(pte));
	ASSERT(ptep_isvalid(new_pte));
	ASSERT(refchg == 0);
	if (remap) {
		// Clear valid bit
		pte->ptew[PTEWORD0] &= ~PTE0_V_FLD_MASK;
		// XXX Make ppc_sync() volatile
		// XXX Also, make sure other code does not move across it
		// XXX See GCC 4.10 Ref Manual, page 260
		//
		// XXX Write post-compile script to verify that
		// XXX mmu_update_pte() contains expected sequence
		// XXX with no dangerous code re-ordering.
		ppc_sync();
		tlbflush(vaddr);
		// Copy the old ref/change bits into the new PTE
		new1 |= pte->ptew[PTEWORD1] & refchg_mask;
	}
	pte->ptew[PTEWORD1] = new1;	// Write PTE word 1
	ppc_sync();
	pte->ptew[PTEWORD0] = new0;	// Write PTE word 0 (valid bit set)
}

/*
 * mmu_pte_reset_rmbits(pte_t *pte, caddr_t vaddr)
 *	Clear the R and C bits in the PTE.
 *	Steps:	Invalidate the old mapping
 *		Sync
 *		TLB flush for the entry
 *		Sync
 *		Tlbsync and sync
 *		Save the old rm bits
 *		Clear the R and C bits in the PTE.
 *		Sync
 *		Set the valid bit
 *		Return the old rm bits
 */

uint_t
mmu_pte_reset_rmbits(pte_t *pte, caddr_t vaddr)
{
	uint_t w0, w1, refchg_mask, refchg;

	w0 = pte->ptew[PTEWORD0];
	w1 = pte->ptew[PTEWORD1];
	refchg_mask = PTE1_R_FLD_MASK | PTE1_C_FLD_MASK;
	refchg = w1 & refchg_mask;

	ASSERT(ptep_isvalid(pte));

	// Clear valid bit
	pte->ptew[PTEWORD0] &= ~PTE0_V_FLD_MASK;
	// XXX Make ppc_sync() volatile
	// XXX Also, make sure other code does not move across it
	// XXX See GCC 4.10 Ref Manual, page 260
	ppc_sync();
	tlbflush(vaddr);
	// Clear ref/change bits
	w1 &= ~refchg_mask;
	pte->ptew[PTEWORD1] = w1;	// write PTE word 1
	ppc_sync();
	pte->ptew[PTEWORD0] = w0;	// write PTE word 0 (valid bit set)
	return (pte1_get_rm(refchg));
}

/*
 * mmu_update_pte_wimg(pte_t *pte, uint_t wimg, caddr_t vaddr)
 *	Update the WIMG bits in the PTE.
 *	Steps:	Invalidate the old mapping (valid bit to 0)
 *		Sync
 *		TLB flush for the entry.
 *		Sync
 *		Tlbsync and sync
 *		Set the new wimg bits in the PTE.
 *		Sync
 *		Set the valid bit.
 */

void
mmu_update_pte_wimg(pte_t *pte, uint_t wimg, caddr_t vaddr)
{
	uint_t w0, w1;

	w0 = pte->ptew[PTEWORD0];
	w1 = pte->ptew[PTEWORD1];

	ASSERT(ptep_isvalid(pte));

	// Clear valid bit
	pte->ptew[PTEWORD0] &= ~PTE0_V_FLD_MASK;
	// XXX Make ppc_sync() volatile
	// XXX Also, make sure other code does not move across it
	// XXX See GCC 4.10 Ref Manual, page 260
	ppc_sync();
	tlbflush(vaddr);
	// Set new WIMG bits in PTE
	w1 = PTE1_SET_WIMG(w1, wimg);
	pte->ptew[PTEWORD1] = w1;	// write PTE word 1
	ppc_sync();
	pte->ptew[PTEWORD0] = w0;	// write PTE word 0 (valid bit set)
}

/*
 * mmu_update_pte_prot(pte_t *pte, uint_t prot, caddr_t vaddr)
 *	Update the pp bits in pte.
 *	Steps:	Invalidate the old mapping (valid bit to 0)
 *		Sync
 *		TLB flush for the entry.
 *		Sync
 *		Tlbsync and sync
 *		Set the new pp bits in the PTE.
 *		Sync
 *		Set the valid bit.
 */

void
mmu_update_pte_prot(pte_t *pte, uint_t prot, caddr_t vaddr)
{
	uint_t w0, w1;

	w0 = pte->ptew[PTEWORD0];
	w1 = pte->ptew[PTEWORD1];

	ASSERT(ptep_isvalid(pte));

	// Clear valid bit
	pte->ptew[PTEWORD0] &= ~PTE0_V_FLD_MASK;
	// XXX Make ppc_sync() volatile
	// XXX Also, make sure other code does not move across it
	// XXX See GCC 4.10 Ref Manual, page 260
	ppc_sync();
	tlbflush(vaddr);
	// Set new pp bits in PTE
	w1 = PTE1_SET_PP(w1, prot);
	pte->ptew[PTEWORD1] = w1;	// write PTE word 1
	ppc_sync();
	pte->ptew[PTEWORD0] = w0;	// write PTE word 0 (valid bit set)
}

/*
 * mmu_delete_pte(pte_t *pte, caddr_t vaddr)
 *	Invalidate the PTE entry.
 *	Steps:	Invalidate the old mapping
 *		Sync
 *		TLB flush for the entry.
 *		Sync
 *		Tlbsync and sync
 *		Copy the old rm bits into the new pte.
 *	Return old rm bits.
 */

uint_t
mmu_delete_pte(pte_t *pte, caddr_t vaddr)
{
	uint_t w0, w1, refchg_mask, refchg;

	w0 = pte->ptew[PTEWORD0];
	w1 = pte->ptew[PTEWORD1];
	refchg_mask = PTE1_R_FLD_MASK | PTE1_C_FLD_MASK;
	refchg = w1 & refchg_mask;

	ASSERT(PTE0_GET_V(w0) != 0);

	// Clear valid bit
	pte->ptew[PTEWORD0] &= ~PTE0_V_FLD_MASK;
	// XXX Make ppc_sync() volatile
	// XXX Also, make sure other code does not move across it
	// XXX See GCC 4.10 Ref Manual, page 260
	ppc_sync();
	tlbflush(vaddr);
	return (pte1_get_rm(refchg));
}

/*
 * Reload segment registers for the specified VSID range,
 * if it is not already loaded.
 */

void
mmu_segload(uint_t range)
{
	uint_t base;
	uint_t cur_base, cur_seg0, segval;

	// XXX We can probably get away with using 14
	// XXX VSIDs per process, instead of 16.
	// XXX SR 14 and 15 are for the kernel,
	// XXX and therefore, are always the same.
	base = range * XXX_MAGIC(16);
	cur_seg0 = mfsr(0);
	cur_base = SR_GET_VSID(cur_seg0);
	if (base == cur_base)
		return;
	segval = SR_NEW(0, 0, 1, 0, base);
	mtsr(0, segval);
	mtsr(1, segval + 1);
	mtsr(2, segval + 2);
	mtsr(3, segval + 3);
	mtsr(4, segval + 4);
	mtsr(5, segval + 5);
	mtsr(6, segval + 6);
	mtsr(7, segval + 7);
	mtsr(8, segval + 8);
	mtsr(9, segval + 9);
	mtsr(10, segval + 10);
	mtsr(11, segval + 11);
	mtsr(12, segval + 12);
	mtsr(13, segval + 13);
	ppc_sync();
}

/*
 * mmu_cache_flushpage(vaddr)
 *
 * Flush the I-cache for the specified page.
 * The sequence is to flush D-cache and invalidate the I-cache.
 *	On 603 systems the sequence is (This is true for UP systems)
 *		dcbst
 *		sync
 *		icbi
 *		isync
 *	On 604 the sequence is (This is true for MP systems)
 *		dcbst
 *		sync
 *		icbi
 *		sync
 *		isync
 */

void
mmu_cache_flushpage(caddr_t flush_addr)
{
	caddr_t start_addr, end_addr, addr;
	size_t dcache_blocksize = XXX_GET_FROM_CONFIGURATION(32);

	// Accept addresses that are not on a page boundary,
	// but we flush the entire page containing the given address.
	// Truncate to start on page boundary.
	// Note: optimizer should generate rlwinm rD,rS,0,0,19
	start_addr = (caddr_t)((intptr_t)flush_addr & PAGEMASK);
	end_addr = start_addr + PAGESIZE;
	for (addr = start_addr; addr < end_addr; addr += dcache_blocksize) {
		dcbst(addr);
	}
	ppc_sync();

	for (addr = start_addr; addr < end_addr; addr += dcache_blocksize) {
		icbi(addr);
	}
	if (ncpus != 1) {
		ppc_sync();
	}
	isync();
}
