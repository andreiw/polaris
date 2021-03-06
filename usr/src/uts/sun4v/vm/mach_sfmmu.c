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
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"@(#)mach_sfmmu.c	1.6	05/06/08 SMI"

#include <sys/types.h>
#include <vm/hat.h>
#include <vm/hat_sfmmu.h>
#include <vm/page.h>
#include <sys/pte.h>
#include <sys/systm.h>
#include <sys/mman.h>
#include <sys/sysmacros.h>
#include <sys/machparam.h>
#include <sys/vtrace.h>
#include <sys/kmem.h>
#include <sys/mmu.h>
#include <sys/cmn_err.h>
#include <sys/cpu.h>
#include <sys/cpuvar.h>
#include <sys/debug.h>
#include <sys/lgrp.h>
#include <sys/archsystm.h>
#include <sys/machsystm.h>
#include <sys/vmsystm.h>
#include <sys/bitmap.h>
#include <vm/rm.h>
#include <sys/t_lock.h>
#include <sys/vm_machparam.h>
#include <sys/promif.h>
#include <sys/prom_isa.h>
#include <sys/prom_plat.h>
#include <sys/prom_debug.h>
#include <sys/privregs.h>
#include <sys/bootconf.h>
#include <sys/memlist.h>
#include <sys/memlist_plat.h>
#include <sys/cpu_module.h>
#include <sys/reboot.h>
#include <sys/kdi.h>
#include <sys/hypervisor_api.h>

/*
 * External routines and data structures
 */
extern void	sfmmu_cache_flushcolor(int, pfn_t);

/*
 * Static routines
 */
static void	sfmmu_set_tlb(void);

/*
 * Global Data:
 */
caddr_t	textva, datava;
tte_t	ktext_tte, kdata_tte;		/* ttes for kernel text and data */

int	enable_bigktsb = 1;

tte_t bigktsb_ttes[MAX_BIGKTSB_TTES];
int bigktsb_nttes = 0;


/*
 * Controls the logic which enables the use of the
 * QUAD_LDD_PHYS ASI for TSB accesses.
 */
int	ktsb_phys = 1;

#ifdef SET_MMU_STATS
struct mmu_stat	mmu_stat_area[NCPU];
#endif /* SET_MMU_STATS */

#ifdef DEBUG
/*
 * The following two variables control if the hypervisor/hardware will
 * be used to do the TSB table walk for kernel and user contexts.
 */
int hv_use_0_tsb = 1;
int hv_use_non0_tsb = 1;
#endif /* DEBUG */

static void
sfmmu_set_fault_status_area(void)
{
	caddr_t mmfsa_va;
	extern	caddr_t mmu_fault_status_area;

	mmfsa_va =
	    mmu_fault_status_area + (MMFSA_SIZE  * getprocessorid());
	set_mmfsa_scratchpad(mmfsa_va);
	prom_set_mmfsa_traptable(&trap_table, va_to_pa(mmfsa_va));
}

void
sfmmu_set_tsbs()
{
	uint64_t rv;
	struct hv_tsb_block *hvbp = &ksfmmup->sfmmu_hvblock;

#ifdef DEBUG
	if (hv_use_0_tsb == 0)
		return;
#endif /* DEBUG */

	rv = hv_set_ctx0(hvbp->hv_tsb_info_cnt,
	    hvbp->hv_tsb_info_pa);
	if (rv != H_EOK)
		prom_printf("cpu%d: hv_set_ctx0() returned %lx\n",
		    getprocessorid(), rv);

#ifdef SET_MMU_STATS
	ASSERT(getprocessorid() < NCPU);
	rv = hv_mmu_set_stat_area(va_to_pa(&mmu_stat_area[getprocessorid()]),
	    sizeof (mmu_stat_area[0]));
	if (rv != H_EOK)
		prom_printf("cpu%d: hv_mmu_set_stat_area() returned %lx\n",
		    getprocessorid(), rv);
#endif /* SET_MMU_STATS */
}

/*
 * This routine remaps the kernel using large ttes
 * All entries except locked ones will be removed from the tlb.
 * It assumes that both the text and data segments reside in a separate
 * 4mb virtual and physical contigous memory chunk.  This routine
 * is only executed by the first cpu.  The remaining cpus execute
 * sfmmu_mp_startup() instead.
 * XXX It assumes that the start of the text segment is KERNELBASE.  It should
 * actually be based on start.
 */
void
sfmmu_remap_kernel(void)
{
	pfn_t	pfn;
	uint_t	attr;
	int	flags;

	extern char end[];
	extern struct as kas;

	textva = (caddr_t)(KERNELBASE & MMU_PAGEMASK4M);
	pfn = va_to_pfn(textva);
	if (pfn == PFN_INVALID)
		prom_panic("can't find kernel text pfn");
	pfn &= TTE_PFNMASK(TTE4M);

	attr = PROC_TEXT | HAT_NOSYNC;
	flags = HAT_LOAD_LOCK | SFMMU_NO_TSBLOAD;
	sfmmu_memtte(&ktext_tte, pfn, attr, TTE4M);
	/*
	 * We set the lock bit in the tte to lock the translation in
	 * the tlb.
	 */
	TTE_SET_LOCKED(&ktext_tte);
	sfmmu_tteload(kas.a_hat, &ktext_tte, textva, NULL, flags);

	datava = (caddr_t)((uintptr_t)end & MMU_PAGEMASK4M);
	pfn = va_to_pfn(datava);
	if (pfn == PFN_INVALID)
		prom_panic("can't find kernel data pfn");
	pfn &= TTE_PFNMASK(TTE4M);

	attr = PROC_DATA | HAT_NOSYNC;
	sfmmu_memtte(&kdata_tte, pfn, attr, TTE4M);
	/*
	 * We set the lock bit in the tte to lock the translation in
	 * the tlb.  We also set the mod bit to avoid taking dirty bit
	 * traps on kernel data.
	 */
	TTE_SET_LOCKED(&kdata_tte);
	TTE_SET_LOFLAGS(&kdata_tte, 0, TTE_HWWR_INT);
	sfmmu_tteload(kas.a_hat, &kdata_tte, datava,
	    (struct page *)NULL, flags);

	/*
	 * create bigktsb ttes if necessary.
	 */
	if (enable_bigktsb) {
		int i = 0;
		caddr_t va = ktsb_base;
		size_t tsbsz = ktsb_sz;
		tte_t tte;

		ASSERT(va >= datava + MMU_PAGESIZE4M);
		ASSERT(tsbsz >= MMU_PAGESIZE4M);
		ASSERT(IS_P2ALIGNED(tsbsz, tsbsz));
		ASSERT(IS_P2ALIGNED(va, tsbsz));
		attr = PROC_DATA | HAT_NOSYNC;
		while (tsbsz != 0) {
			ASSERT(i < MAX_BIGKTSB_TTES);
			pfn = va_to_pfn(va);
			ASSERT(pfn != PFN_INVALID);
			ASSERT((pfn & ~TTE_PFNMASK(TTE4M)) == 0);
			sfmmu_memtte(&tte, pfn, attr, TTE4M);
			ASSERT(TTE_IS_MOD(&tte));
			/*
			 * No need to lock if we use physical addresses.
			 * Since we invalidate the kernel TSB using virtual
			 * addresses, it's an optimization to load them now
			 * so that we won't have to load them later.
			 */
			if (!ktsb_phys) {
				TTE_SET_LOCKED(&tte);
			}
			sfmmu_tteload(kas.a_hat, &tte, va, NULL, flags);
			bigktsb_ttes[i] = tte;
			va += MMU_PAGESIZE4M;
			tsbsz -= MMU_PAGESIZE4M;
			i++;
		}
		bigktsb_nttes = i;
	}

	sfmmu_set_tlb();
}

/*
 * Setup the kernel's locked tte's
 */
void
sfmmu_set_tlb(void)
{
	(void) hv_mmu_map_perm_addr(textva, KCONTEXT, *(uint64_t *)&ktext_tte,
	    MAP_ITLB | MAP_DTLB);
	(void) hv_mmu_map_perm_addr(datava, KCONTEXT, *(uint64_t *)&kdata_tte,
	    MAP_DTLB);

	if (!ktsb_phys && enable_bigktsb) {
		int i;
		caddr_t va = ktsb_base;
		uint64_t tte;

		ASSERT(bigktsb_nttes <= MAX_BIGKTSB_TTES);
		for (i = 0; i < bigktsb_nttes; i++) {
			tte = *(uint64_t *)&bigktsb_ttes[i];
			(void) hv_mmu_map_perm_addr(va, KCONTEXT, tte,
			    MAP_DTLB);
			va += MMU_PAGESIZE4M;
		}
	}
}

/*
 * This routine is executed by all other cpus except the first one
 * at initialization time.  It is responsible for taking over the
 * mmu from the prom.  We follow these steps.
 * Lock the kernel's ttes in the TLB
 * Initialize the tsb hardware registers
 * Take over the trap table
 * Flush the prom's locked entries from the TLB
 */
void
sfmmu_mp_startup(void)
{
	sfmmu_set_tlb();
	setwstate(WSTATE_KERN);
	/*
	 * sfmmu_set_fault_status_area() takes over trap_table
	 */
	sfmmu_set_fault_status_area();
	sfmmu_set_tsbs();
	install_va_to_tte();
}

void
kdi_tlb_page_lock(caddr_t va, int do_dtlb)
{
	tte_t tte;
	pfn_t pfn = va_to_pfn(va);
	uint64_t ret;

	sfmmu_memtte(&tte, pfn, (PROC_TEXT | HAT_NOSYNC), TTE8K);
	ret = hv_mmu_map_perm_addr(va, KCONTEXT, *(uint64_t *)&tte,
	    MAP_ITLB | (do_dtlb ? MAP_DTLB : 0));

	if (ret != H_EOK) {
		cmn_err(CE_PANIC, "cpu%d: cannot set permanent mapping for "
		    "va=0x%p, hv error code 0x%lux",
		    getprocessorid(), (void *)va, ret);
	}
}

void
kdi_tlb_page_unlock(caddr_t va, int do_dtlb)
{
	(void) hv_mmu_unmap_perm_addr(va, KCONTEXT,
	    MAP_ITLB | (do_dtlb ? MAP_DTLB : 0));
}

/*
 * Clear machine specific TSB information for a user process
 */
void
sfmmu_clear_utsbinfo()
{
	(void) hv_set_ctxnon0(0, NULL);
}

/*
 * Set machine specific TSB information
 */
void
sfmmu_setup_tsbinfo(sfmmu_t *sfmmup)
{
	struct tsb_info *tsbinfop;
	hv_tsb_info_t *tdp;

	tsbinfop = sfmmup->sfmmu_tsb;
	if (tsbinfop == NULL) {
		sfmmup->sfmmu_hvblock.hv_tsb_info_pa = (uint64_t)-1;
		sfmmup->sfmmu_hvblock.hv_tsb_info_cnt = 0;
		return;
	}
	tdp = &sfmmup->sfmmu_hvblock.hv_tsb_info[0];
	sfmmup->sfmmu_hvblock.hv_tsb_info_pa = va_to_pa(tdp);
	sfmmup->sfmmu_hvblock.hv_tsb_info_cnt = 1;
	tdp->hvtsb_idxpgsz = TTE8K;
	tdp->hvtsb_assoc = 1;
	tdp->hvtsb_ntte = TSB_ENTRIES(tsbinfop->tsb_szc);
	tdp->hvtsb_ctx_index = 0;
	tdp->hvtsb_pgszs = tsbinfop->tsb_ttesz_mask;
	tdp->hvtsb_rsvd = 0;
	tdp->hvtsb_pa = tsbinfop->tsb_pa;
	if ((tsbinfop = tsbinfop->tsb_next) == NULL)
		return;
	sfmmup->sfmmu_hvblock.hv_tsb_info_cnt++;
	tdp++;
	tdp->hvtsb_idxpgsz = TTE4M;
	tdp->hvtsb_assoc = 1;
	tdp->hvtsb_ntte = TSB_ENTRIES(tsbinfop->tsb_szc);
	tdp->hvtsb_ctx_index = 0;
	tdp->hvtsb_pgszs = tsbinfop->tsb_ttesz_mask;
	tdp->hvtsb_rsvd = 0;
	tdp->hvtsb_pa = tsbinfop->tsb_pa;
	/* Only allow for 2 TSBs */
	ASSERT(tsbinfop->tsb_next == NULL);
}

/*
 * Invalidate a TSB via processor specific TSB invalidation routine
 */
void
sfmmu_inv_tsb(caddr_t tsb_base, uint_t tsb_bytes)
{
	extern void cpu_inv_tsb(caddr_t, uint_t);

	cpu_inv_tsb(tsb_base, tsb_bytes);
}

/*
 * Completely flush the D-cache on all cpus.
 * Not applicable to sun4v.
 */
void
sfmmu_cache_flushall()
{
}
