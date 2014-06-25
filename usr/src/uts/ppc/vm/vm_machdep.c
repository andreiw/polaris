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

#pragma ident	"%Z%%M%	%I%	%E% SMI"

/*
 * UNIX machine dependent virtual memory support.
 */

#include <vm/lint.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/cpuvar.h>
#include <sys/disp.h>
#include <sys/vm.h>
#include <sys/mman.h>
#include <sys/vnode.h>
#include <sys/cred.h>

#include <vm/hat.h>
#include <vm/as.h>
#include <vm/seg.h>
#include <vm/seg_vn.h>
#include <vm/page.h>
#include <vm/mach_page.h>
#include <vm/seg_kmem.h>
#include <vm/seg_kpm.h>

#include <sys/mmu.h>
#include <sys/pte.h>
#include <sys/cpu.h>
#include <sys/vm_machparam.h>
#include <sys/memlist.h>
#include <sys/obpdefs.h>
#include <sys/bootconf.h>

#include <vm/hat_ppcmmu.h>
#include <vm/vm_dep.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/elf_ppc.h>
#include <sys/stack.h>

#define	ELEMENTS(array) ((sizeof (array)) / (sizeof (*(array))))

uint_t vac_colors = 0;
uint_t mmu_page_sizes;

/* How many page sizes the users can see */
uint_t mmu_exported_page_sizes;

size_t auto_lpg_va_default = MMU_PAGESIZE; /* used by zmap() */

/*
 * Return the optimum page size for a given mapping
 */
/*ARGSUSED*/
size_t
map_pgsz(int maptype, struct proc *p, caddr_t addr, size_t len, int *remap)
{
	if (remap)
		*remap = 0;

	switch (maptype) {
	case MAPPGSZ_STK:
	case MAPPGSZ_HEAP:
	case MAPPGSZ_VA:
	case MAPPGSZ_ISM:
		return (MMU_PAGESIZE);
	default:
		/*
		 * Not sure how much we should tolerate
		 * unknown values for maptype.
		 * Maybe returning 0 is good enough.
		 * But for now, let's be strict and see what happens.
		 */
		panic("Unknown maptype, %u\n", maptype);
	}
	return (0);
}

/*
 * This can be patched via /etc/system to allow large pages
 * to be used for mapping application and libraries text segments.
 */
int	use_text_largepages = 0;

/*
 * Return a bit vector of large page size codes that
 * can be used to map [addr, addr + len) region.
 */

/*ARGSUSED*/
uint_t
map_execseg_pgszcvec(int text, caddr_t addr, size_t len)
{
	/*
	 * PowerPC just doesn't have much in the way of support
	 * for multiple page sizes.  4K is it, except for BAT mappings.
	 */
	return (0);
}

/*
 * Handle a pagefault.
 */
faultcode_t
pagefault(
	caddr_t addr,
	enum fault_type type,
	enum seg_rw rw,
	int iskernel)
{
	struct as *as;
	struct proc *p;
	faultcode_t res;
	caddr_t base;
	size_t len;
	int err;

	if (iskernel) {
		as = &kas;
	} else {
		p = curproc;
		as = p->p_as;
	}

	/*
	 * Dispatch pagefault.
	 */
	res = as_fault(as->a_hat, as, addr, 1, type, rw);

	/*
	 * If this isn't a potential unmapped hole in the user's
	 * UNIX data or stack segments, just return status info.
	 */
	if (!(res == FC_NOMAP && iskernel == 0))
		return (res);

	/*
	 * Check to see if we happened to faulted on a currently unmapped
	 * part of the UNIX data or stack segments.  If so, create a zfod
	 * mapping there and then try calling the fault routine again.
	 */
	base = p->p_brkbase;
	len = p->p_brksize;

	if (addr < base || addr >= base + len) {		/* data seg? */
		base = (caddr_t)((caddr_t)USRSTACK - p->p_stksize);
		len = p->p_stksize;
		if (addr < base || addr >= (caddr_t)USRSTACK) {	/* stack seg? */
			/* not in either UNIX data or stack segments */
			return (FC_NOMAP);
		}
	}

	/* the rest of this function implements a 3.X 4.X 5.X compatibility */
	/* This code is probably not needed anymore */

	/* expand the gap to the page boundaries on each side */
	len = (((uint_t)base + len + PAGEOFFSET) & PAGEMASK) -
	    ((uint_t)base & PAGEMASK);
	base = (caddr_t)((uint_t)base & PAGEMASK);

	as_rangelock(as);
	if (as_gap(as, MMU_PAGESIZE, &base, &len, AH_CONTAIN, addr) != 0) {
		/*
		 * Since we already got an FC_NOMAP return code from
		 * as_fault, there must be a hole at `addr'.  Therefore,
		 * as_gap should never fail here.
		 */
		panic("pagefault as_gap");
	}

	err = as_map(as, base, len, segvn_create, zfod_argsp);
	as_rangeunlock(as);
	if (err)
		return (FC_MAKE_ERR(err));

	return (as_fault(as->a_hat, as, addr, 1, F_INVAL, rw));
}

/*ARGSUSED*/
void
map_addr(caddr_t *addrp, size_t len, offset_t off, int vacalign, uint_t flags)
{
	struct proc *p = curproc;
	caddr_t userlimit = (flags & _MAP_LOW32) ?
	    (caddr_t)_userlimit32 : p->p_as->a_userlimit;

	map_addr_proc(addrp, len, off, vacalign, userlimit, curproc, flags);
}

/*ARGSUSED*/
int
map_addr_vacalign_check(caddr_t addr, u_offset_t off)
{
	return (0);
}

/*
 * map_addr_proc() is the routine called when the system is to
 * chose an address for the user.  We will pick an address range which is
 * just below the user text or below the current stack limit.
 *
 * As per PowerPC ABI the regions (0x10000 to USRTEXT) and (user_stack_limit
 * to program_break_address) can be used for dynamic segments.  Using the
 * address space just below the USRTEXT (or whatever the text origin of the
 * process) is an optimization to have the relative addresses within a 32M
 * range (limited by the relative address branching on PPC).  So, the
 * allocation is done in the following regions in that order:
 *
 *	Region 1: 0x10000 to USERTEXT
 *		Allocation is from high end to low end.
 *	Region 2: program_break_address to user_stack_limit
 *		Allocation is from high end to low end.
 *
 * addrp is a value/result parameter.
 *	On input it is a hint from the user to be used in a completely
 *	machine dependent fashion.  We decide to completely ignore this hint.
 *
 *	On output it is NULL if no address can be found in the current
 *	processes address space or else an address that is currently
 *	not mapped for len bytes with a page of red zone on either side.
 *	If align is true, then the selected address will obey the alignment
 *	constraints of a vac machine based on the given off value.
 */
/*ARGSUSED*/
void
map_addr_proc(
	caddr_t *addrp,
	size_t len,
	offset_t off,
	int vacalign,
	caddr_t userlimit,
	struct proc *p,
	uint_t flags)
{
	struct as *as = p->p_as;
	caddr_t addr;
	caddr_t base;
	size_t slen;
	size_t align_amount;
	rctl_qty_t lim;

	len = (len + PAGEOFFSET) & PAGEMASK;

	/*
	 * Redzone for each side of the request. This is done to leave
	 * one page unmapped between segments. This is not required, but
	 * it's useful for the user because if their program strays across
	 * a segment boundary, it will catch a fault immediately making
	 * debugging a little easier.
	 */
	len += 2 * PAGESIZE;

	/*
	 * Use ELF_PPC_MAXPGSZ (64k) alignment for better ld.so.1 behavior.
	 *
	 * (The PPC ABI specifies 64k for p_align field of program header
	 * for shared objects.  We do the alignement here so that the
	 * run-time linker does not have to do additional system
	 * calls (i.e. mmap/munmap) to enforce alignment on the objects.)
	 */
	align_amount = ELF_PPC_MAXPGSZ;
	len += align_amount;

	/*
	 * Look for a large enough hole starting below the start of user
	 * text origin.  After finding it, use the upper part.  Addition of
	 * PAGESIZE is for the redzone as described above.
	 */
	base = (caddr_t)0x10000;
	if (p->p_brkbase > base) {
		slen = (uintptr_t)p->p_brkbase - (uintptr_t)base;
		if (as_gap(as, len, &base, &slen, AH_LO, (caddr_t)NULL) == 0) {
			caddr_t as_addr;

			addr = base + slen - len  + PAGESIZE;
			as_addr = addr;

			/*
			 * Round address DOWN to the alignment amount,
			 * add the offset, and if this address is less
			 * than the original address, add alignment amount.
			 */
			addr = (caddr_t)((uintptr_t)addr & ~(align_amount - 1));
			addr += off & (align_amount - 1);
			if (addr < as_addr)
				addr += align_amount;
			*addrp = addr;
			return;
		}
	}

	base = p->p_brkbase;
	lim = rctl_enforced_value(rctlproc_legacy[RLIMIT_STACK], p->p_rctls, p);
	slen = p->p_usrstack - base - (((size_t)lim + PAGEOFFSET) & PAGEMASK);

	/*
	 * Look for a large enough hole starting below the stack limit.
	 * After finding it, use the upper part.  Addition of PAGESIZE is
	 * for the redzone as described above.
	 */
	if (as_gap(as, len, &base, &slen, AH_HI, NULL) == 0) {
		caddr_t as_addr;

		addr = base + slen - len  + PAGESIZE;
		as_addr = addr;

		/*
		 * Round address DOWN to the alignment amount,
		 * add the offset, and if this address is less
		 * than the original address, add alignment amount.
		 */
		addr = (caddr_t)((uintptr_t)addr & ~(align_amount - 1));
		addr += (uintptr_t)(off & (align_amount - 1));
		if (addr < as_addr)
			addr += align_amount;
		ASSERT(addr <= (as_addr + align_amount));
		ASSERT(((uintptr_t)addr & (align_amount - 1)) ==
		    ((uintptr_t)(off & (align_amount - 1))));
		*addrp = addr;
	} else {
		*addrp = NULL;	/* no more virtual space */
	}
}

/*
 * Determine whether [base, base+len] contains a mappable range of
 * addresses at least minlen long.  base and len are adjusted if
 * required to provide a mappable range.
 */
/* ARGSUSED3 */
int
valid_va_range(caddr_t *basep, size_t *lenp, size_t minlen, int dir)
{
	uintptr_t hi, lo;

	lo = (uintptr_t)*basep;
	hi = lo + *lenp;

	if (hi - lo < minlen)
		return (0);

	/*
	 * If hi rolled over the top, try cutting back.
	 */
	if (hi < lo) {
		uintptr_t avail = 0 - lo;

		if (avail < minlen)
			return (0);
		*lenp = avail;
	}
	return (1);
}

/*
 * Determine whether [addr, addr+len] are valid user addresses.
 */
int
valid_usr_range(
	caddr_t addr,
	size_t len,
	uint_t prot,
	struct as *as,
	caddr_t userlimit)
{
	caddr_t eaddr = addr + len;

	if (eaddr <= addr || addr >= userlimit || eaddr > userlimit)
		return (RANGE_BADADDR);
	return (RANGE_OKAY);
}

/*
 * Return 1 if the page frame is onboard memory, else 0.
 */
int
pf_is_memory(pfn_t pf)
{
	return (address_in_memlist(phys_install, mmu_ptob((uint64_t)pf), 1));
}

/*
 * initialized by page_coloring_init().
 */
uint_t	page_colors;
uint_t	page_colors_mask;
uint_t	page_coloring_shift;
int	cpu_page_colors;
static uint_t	l2_colors;

/*
 * Page freelists and cachelists are dynamically allocated once mnoderangecnt
 * and page_colors are calculated from the l2 cache n-way set size.  Within a
 * mnode range, the page freelist and cachelist are hashed into bins based on
 * color. This makes it easier to search for a page within a specific memory
 * range.
 */
#define	PAGE_COLORS_MIN	16

page_t ****page_freelists;
page_t ***page_cachelists;

/*
 * As the PC architecture evolved memory up was clumped into several
 * ranges for various historical I/O devices to do DMA.
 * < 16Meg - ISA bus
 * < 2Gig - ???
 * < 4Gig - PCI bus or drivers that don't understand PAE mode
 */
static pfn_t arch_memranges[] = {
    0x100000,	/* pfn range for 4G and above */
    0x80000,	/* pfn range for 2G-4G */
    0x01000,	/* pfn range for 16M-2G */
    0x00000,	/* pfn range for 0-16M */
};

/*
 * These are changed during startup if the machine has limited memory.
 */
pfn_t *memranges = &arch_memranges[0];
int nranges = ELEMENTS(arch_memranges);

/*
 * Used by page layer to know about page sizes
 */
hw_pagesize_t hw_page_array[65];

/*
 * This can be patched via /etc/system to allow old non-PAE aware device
 * drivers to use kmem_alloc'd memory on 32 bit systems with > 4Gig RAM.
 */

int restricted_kmemalloc = 0;

/*
 * For now, we just don't bother with page coloring on PowerPC.
 */

void
page_coloring_init()
{
}

/*ARGSUSED*/
int
bp_color(struct buf *bp)
{
	return (0);
}

/*
 * Copy the data from the physical page represented by "frompp" to
 * that represented by "topp".  ppcopy uses CPU->cpu_caddr1 and
 * CPU->cpu_caddr2.  It assumes that no one uses either map at interrupt
 * level and no one sleeps with an active mapping there.
 *
 * Note that the ref/mod bits in the page_t's are not affected by
 * this operation, hence it is up to the caller to update them appropriately.
 */
void
ppcopy(page_t *frompp, page_t *topp)
{
	caddr_t		pp_addr1;
	caddr_t		pp_addr2;
	void		*pte1;
	void		*pte2;
	kmutex_t	*ppaddr_mutex;

	ASSERT_STACK_ALIGNED();
	ASSERT(PAGE_LOCKED(frompp));
	ASSERT(PAGE_LOCKED(topp));

	if (kpm_enable) {
		pp_addr1 = hat_kpm_page2va(frompp, 0);
		pp_addr2 = hat_kpm_page2va(topp, 0);
		kpreempt_disable();
	} else {
		/*
		 * disable pre-emption so that CPU can't change
		 */
		kpreempt_disable();

		pp_addr1 = CPU->cpu_caddr1;
		pp_addr2 = CPU->cpu_caddr2;
		pte1 = (void *)CPU->cpu_caddr1pte;
		pte2 = (void *)CPU->cpu_caddr2pte;

		ppaddr_mutex = &CPU->cpu_ppaddr_mutex;
		mutex_enter(ppaddr_mutex);

		hat_mempte_remap(page_pptonum(frompp), pp_addr1, pte1,
		    PROT_READ | HAT_STORECACHING_OK, HAT_LOAD_NOCONSIST);
		hat_mempte_remap(page_pptonum(topp), pp_addr2, pte2,
		    PROT_READ | PROT_WRITE | HAT_STORECACHING_OK,
		    HAT_LOAD_NOCONSIST);
	}

	hwblkpagecopy(pp_addr1, pp_addr2);

	if (!kpm_enable)
		mutex_exit(ppaddr_mutex);
	kpreempt_enable();
}

/*
 * Zero the physical page from off to off + len given by `pp'
 * without changing the reference and modified bits of page.
 *
 * We use this using CPU private page address #2, see ppcopy() for more info.
 * pagezero() must not be called at interrupt level.
 */
void
pagezero(page_t *pp, uint_t off, uint_t len)
{
	caddr_t		pp_addr2;
	void		*pte2;
	kmutex_t	*ppaddr_mutex;

	ASSERT_STACK_ALIGNED();
	ASSERT(len <= MMU_PAGESIZE);
	ASSERT(off <= MMU_PAGESIZE);
	ASSERT(off + len <= MMU_PAGESIZE);
	ASSERT(PAGE_LOCKED(pp));

	if (kpm_enable) {
		pp_addr2 = hat_kpm_page2va(pp, 0);
		kpreempt_disable();
	} else {
		kpreempt_disable();

		pp_addr2 = CPU->cpu_caddr2;
		pte2 = (void *)CPU->cpu_caddr2pte;

		ppaddr_mutex = &CPU->cpu_ppaddr_mutex;
		mutex_enter(ppaddr_mutex);

		hat_mempte_remap(page_pptonum(pp), pp_addr2, pte2,
		    PROT_READ | PROT_WRITE | HAT_STORECACHING_OK,
		    HAT_LOAD_NOCONSIST);
	}

	hwblkclr(pp_addr2 + off, len);

	if (!kpm_enable)
		mutex_exit(ppaddr_mutex);
	kpreempt_enable();
}

/*
 * Platform-dependent page scrub call.
 */
void
pagescrub(page_t *pp, uint_t off, uint_t len)
{
	/*
	 * For now, we rely on the fact that pagezero() will
	 * always clear UEs.
	 */
	pagezero(pp, off, len);
}
