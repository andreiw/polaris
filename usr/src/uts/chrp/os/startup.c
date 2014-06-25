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
 * Portions Copyright 2006 by Sun Microsystems Inc., Cyril Plisko.
 * All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Portions Copyright 2006 Sun Microsystems Laboratories.  All rights reserved.
 * Use is subject to license terms.
 */


#pragma ident	"%Z%%M%	%I%	%E% CP"

/*
 * CHRP unix startup procedures.
 * Largely based on uts/i86pc/os/startup.c and uts/sun4/os/startup.c
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/avintr.h>

#include <sys/bootconf.h>
#include <sys/cmn_err.h>

#include <sys/thread.h>
#include <sys/machparam.h>
#include <vm/seg.h>
#include <sys/vm_machparam.h>
#include <sys/machsystm.h>
#include <sys/mman.h>		/* Import PROT_* */
#include <sys/sysmacros.h>	/* Import ISP2(), P2ROUNDUP(), et al */
#include <sys/p2sizes.h>
#include <sys/kmem.h>
#include <vm/seg_kmem.h>

#define	ROUND_UP_PAGE(x)	\
	((uintptr_t)P2ROUNDUP((uintptr_t)(x), (uintptr_t)MMU_PAGESIZE))

extern void progressbar_init(void);
extern void progressbar_start(void);

/*
 * Static Routines:
 */

static void kvm_init(void);

static void startup_init(void);
static void startup_memlist(void);
static void startup_modules(void);
static void startup_bop_gone(void);
static void startup_vm(void);
static void startup_end(void);

static void ersatz_krtld(void);

/*
 * Table of mapping information for various kernel segments.
 */

struct kvminfo {
	caddr_t	kvm_vaddr;
	size_t	kvm_size;
	uint_t	kvm_nbats;
	uint_t	kvm_vprot;
};

typedef struct kvminfo kvminfo_t;

kvminfo_t kvm_text;
kvminfo_t kvm_data;

static __inline__ uint_t
in_kernel_text(caddr_t va)
{
	caddr_t s_ktext = kvm_text.kvm_vaddr;
	caddr_t e_ktext = s_ktext + kvm_text.kvm_size;

	return (va >= s_ktext && va < e_ktext);
}

static __inline__ uint_t
in_kernel_data(caddr_t va)
{
	caddr_t s_kdata = kvm_data.kvm_vaddr;
	caddr_t e_kdata = s_kdata + kvm_data.kvm_size;

	return (va >= s_kdata && va < e_kdata);
}

/*
 * kpm mapping window
 */
caddr_t kpm_vbase;
size_t  kpm_size;
static int kpm_desired = 0;	/* Do we want to try to use segkpm? */

pfn_t physmax;
pgcnt_t physinstalled;
pgcnt_t physmem = 0;

/*
 * XXX Usually bootops is passed in from boot.
 */

struct bootops		*bootops;
/* struct bootops		**bootopsp; */
struct boot_syscalls	*sysp;		/* passed in from boot */

pgcnt_t obp_pages;	/* Memory used by PROM for its text and data */

caddr_t s_text;		/* start of kernel text segment */
caddr_t e_text;		/* end of kernel text segment */
caddr_t s_data;		/* start of kernel data segment */
caddr_t e_data;		/* end of kernel data segment */
caddr_t s_modtext;	/* start of loadable module text reserved */
caddr_t e_modtext;	/* end of loadable module text reserved */
caddr_t s_moddata;	/* start of loadable module data reserved */
caddr_t e_moddata;	/* end of loadable module data reserved */
caddr_t econtig;	/* end of first block of contiguous kernel */

static caddr_t s_avail;
static caddr_t e_avail;
static caddr_t s_gap;
static caddr_t e_gap;

/*
 * modtext_resv:
 *   Amount of memory to try to reserve for module text.
 *   Tunable.  Default is MODTEXT_RESV.
 *
 * moddata_resv:
 *   Amount of memory to try to reserve for module data.
 *   Tunable.  Default is MODDATA_RESV.
 */

static size_t modtext_resv;
static size_t moddata_resv;

struct memlist *phys_install;	/* Total installed physical memory */
struct memlist *phys_avail;	/* Total available physical memory */

/*
 * VM data structures
 */
long page_hashsz;		/* Size of page hash table (power of two) */
struct page *pp_base;		/* Base of initial system page struct array */
struct page **page_hash;	/* Page hash table */
struct seg ktextseg;		/* Segment used for kernel executable image */
struct seg kvalloc;		/* Segment used for "valloc" mapping */
struct seg kpseg;		/* Segment used for pageable kernel virt mem */
struct seg kmapseg;		/* Segment used for generic kernel mappings */
struct seg kdebugseg;		/* Segment used for the kernel debugger */

struct seg *segkmap = &kmapseg;	/* Kernel generic mapping segment */
struct seg *segkp = &kpseg;	/* Pageable kernel virtual memory segment */

struct seg *segkpm = NULL;	/* Unused on 32-bit PowerPC */

caddr_t segkp_base;		/* Base address of segkp */
pgcnt_t segkpsize = 0;

/*
 * Simple boot time debug facilities
 */
static char *prm_dbg_str[] = {
	"%s:%d: '%s' is 0x%x\n",
	"%s:%d: '%s' is 0x%llx\n"
};

static int prom_debug = 1;

#define	PRM_DEBUG(q)	if (prom_debug)		\
	prom_printf(prm_dbg_str[sizeof (q) >> 3], "startup.c", __LINE__, #q, q);
#define	PRM_POINT(q)	if (prom_debug)		\
	prom_printf("%s:%d: %s\n", "startup.c", __LINE__, q);

/* real-time-clock initialization parameters */
long gmt_lag;		/* offset in seconds of gmt to local time */
extern long process_rtc_config_file(void);

short		cputype;
uintptr_t	kernelbase;
uintptr_t	eprom_kernelbase;
size_t		segmapsize;
int		segmapfreelists;

extern uint_t l2cache_sets;
extern uint_t l2cache_linesz;
extern uint_t l2cache_assoc;
extern uint_t l2cache_size;

/*
 * List of bootstrap pages.  We mark these as allocated in startup.
 * release_bootstrap() will free them when we're completely done with
 * the bootstrap.
 */
struct system_hardware system_hardware;

/*
 * Machine-dependent startup code
 */
void
startup(void)
{
	progressbar_init();
	startup_init();
	startup_memlist();
	startup_modules();
	startup_bop_gone();
	startup_vm();
	startup_end();
	progressbar_start();
}

void
startup_init(void)
{
	extern uint_t unit_test(void);

	PRM_POINT("startup_init() starting...");

	ersatz_krtld();

	/*
	 * Complete the extraction of cpuid data
	 */

	cpuid_pass2(CPU);

	(void) check_boot_version(BOP_GETVERSION(bootops));

	/*
	 * Halt if this is an unsupported processor.
	 */

	/* Check and halt would go here. */

	unit_test();

	PRM_POINT("startup_init() done");
}

/*
 * There are some things that krtld sets up for us.
 * If they are not setup, then do something reasonable, here.
 */

void
ersatz_krtld()
{
	extern char _text;
	extern char _etext;
	extern char _data;
	extern char _edata;

	extern char __bss_start;
	extern char _end;
	extern void _start(void);

	if (s_text == NULL) {
		s_text = &_text;
		e_text = &_etext;
		s_data = &_data;
		e_data = &_edata;
	}

	PRM_DEBUG(s_text);
	PRM_DEBUG(e_text);
	PRM_DEBUG(s_data);
	PRM_DEBUG(e_data);

	ASSERT(e_text > s_text);
	ASSERT(e_data > s_data);
	ASSERT(&__bss_start >= e_data);
}

/*
 * Deduce how kernel text and data are mapped.
 *
 * What is required, what is known
 * -------------------------------
 * All we really know is that somehow the kernel has been
 * loaded and is running.  This could have been accomplished
 * by any one of several means: a development machine
 * downloading to a target, directly; or some sort of
 * boot loader; perhaps a debugger is involved.
 * There may or may not be firmware, such as OpenFirmware.
 * The boot loader may or may not try to cooperate with
 * the firmware.  Solaris may or may not try to co-exist
 * with firmware, for a short while or for a while longer,
 * or perhaps even permanently.  At this stage, we just
 * want to know how the kernel has already been mapped.
 * We assume that the loader has mapped the kernel with
 * a small number of BAT registers.  But we don't know
 * how large the mappings are or what BAT registers have
 * been used.  So, we probe to find out what maps kernel
 * text and data.
 *
 * It is not safe to assume that all BAT mappings are
 * specifically intended to map the kernel.  There may
 * be mappings established by firmware that map all
 * of physical memory with a 1:1 virtual-to-physical
 * mapping.  On machines with sufficiently small physical
 * memory, we can easily detect and ignore those mappings.
 * This is because it is a requirement (part of the ABI)
 * that the kernel be mapped somewhere in the upper 2
 * segments of virtual address space, 0xE000_0000 and
 * above.  So, the probe ignores mappings for virtual
 * address under 0xE000_0000 and ignores mappings for
 * 256Mbytes or larger, because they are probably used
 * for a 1:1 virtual-to-physical mapping.  If there
 * is a mapping at 0xE000_0000 and it is larger than
 * expected, then we will have to rethink things.
 *
 * Kernel text and data protections
 * --------------------------------
 * If processors and kernels were designed properly,
 * we would expect to see two mappings: one for kernel
 * text and one for kernel data.  Kernel text would
 * be mapped read-only, and kernel data would be mapped
 * read/write.  But, this is not the case, due to a
 * combination of hardware limitations on supported combinations of protection
 * mechanisms and privileges, and due to historical
 * expectations of Solaris and loaders and debuggers.
 * Instead, the kernel will be mapped
 * read/write for both text and data, and therefore,
 * there is no particularly strict alignment requirement
 * for the boundary between kernel text and data, and
 * the whole thing could be mapped by one big set of
 * mappings, all with the same protections.  It could
 * all be mapped by 1 BAT register.
 *
 * Kernel virtual memory layout
 * ----------------------------
 * Solaris on other processors uses a hard-code number,
 * KERNEL_TEXT.  See usr/src/uts/`uname -i`/sys/machparam.h
 * for the various definitions of KERNEL_TEXT.
 * On PowerPC, we discover the BAT mappings that cover
 * kernel text and kernel data, which would have been
 * established by the boot loader.  Check to ensure that
 * we have BAT mappings adequate for the minimum size
 * requirements.  Do some sanity tests to ensure that
 * things are consistent and adequately provisioned.
 */

/*
 * Find the first mapping that covers kernel text.
 */

uint_t
probe_ktext_start(void)
{
	uintptr_t va;
	pfn_t pfn;
	size_t len;
	uint_t prot;
	uint_t rc;
	uint_t err;
	uint_t warn;

	/*
	 * Use the virtual address of _start to find the
	 * first kernel text mapping.  It is assumed that
	 * the kernel is built so that _start is first.
	 * It must at least be contianed within the first
	 * kernel text mapping.
	 */
	va = (uintptr_t)s_text;
	prom_printf("probe_ktext_start: s_text=%p\n", va);
	rc = hat_boot_probe(&va, &len, &pfn, &prot);
	if (rc == 0) {
		prom_printf("Couldn't find kernel text boot mapping.\n");
		return (0);
	}
	err = 0;
	warn = 0;
	if ((prot & PROT_READ) == 0) {
		prom_printf("Mapping of kernel text is not readable.\n");
		err = 1;
	}
	if ((prot & PROT_USER) != 0) {
		prom_printf("Mapping of kernel text is user accessible.\n");
		/*
		 * Classify this as an error,
		 * unless this is one of those 1:1 vitual-to-physical mappings.
		 */
		if (va == 0 && len == B256M)
			warn = 1;
		else
			err = 1;
	}
	if (err != 0 || warn != 0) {
		prom_printf("Expecting prot=%x, got prot=%x\n",
			PROT_READ | PROT_WRITE, prot);
	}
	ASSERT(ISP2(len));
	kvm_text.kvm_vaddr = (caddr_t)va;
	kvm_text.kvm_size = len;
	kvm_text.kvm_nbats = 1;
	kvm_text.kvm_vprot = prot;
	return (err == 0);
}

/*
 * Find additional mappings that cover kernel text, if any.
 */

uint_t
probe_ktext_more(void)
{
	uintptr_t ktext_va;
	size_t ktext_size;
	uint_t ktext_nbats;
	uintptr_t va;
	pfn_t pfn;
	size_t len;
	uint_t prot;
	uint_t rc;

	/*
	 * See if kernel text is mapped by more than one BAT.
	 * Continue probing at the next virtual address
	 * after the first mapping of kernel text.
	 */
	ktext_va = (uintptr_t)kvm_text.kvm_vaddr;
	ktext_size = kvm_text.kvm_size;
	ktext_nbats = kvm_text.kvm_nbats;
	for (;;) {
		va = ktext_va + ktext_size;
		/*
		 * Don't ask if there might be a mapping for kernel text
		 * when this virtual address is already on record as
		 * a mapping for kernel data.
		 */
		if (va == (uintptr_t)kvm_data.kvm_vaddr)
			break;
		rc = hat_boot_probe(&va, &len, &pfn, &prot);
		if (rc == 0 || prot != kvm_text.kvm_vprot)
			break;
		ASSERT(ISP2(len));
		ktext_size += len;
		++ktext_nbats;
	}

	kvm_text.kvm_vaddr = (caddr_t)ktext_va;
	kvm_text.kvm_size = ktext_size;
	kvm_text.kvm_nbats = ktext_nbats;
	return (1);
}

/*
 * Find the first mapping that covers kernel data.
 */

uint_t
probe_kdata_start(void)
{
	uintptr_t va;
	pfn_t pfn;
	size_t len;
	uint_t prot;
	uint_t rc;
	uint_t err;
	uint_t warn;

	/* Pick the address of some kernel data to use for the probe */
	va = (uintptr_t)&physmem;
	prom_printf("probe_kdata_start: %p is in kernel data\n", va);
	rc = hat_boot_probe(&va, &len, &pfn, &prot);
	if (rc == 0) {
		prom_printf("Couldn't find kernel data boot mapping.\n");
		return (0);
	}
	err = 0;
	warn = 0;
	if ((prot & PROT_READ) == 0) {
		prom_printf("Mapping of kernel data is not readable.\n");
		err = 1;
	}
	if ((prot & PROT_WRITE) == 0) {
		prom_printf("Mapping of kernel text is not writable.\n");
		err = 1;
	}
	if ((prot & PROT_USER) != 0) {
		prom_printf("Mapping of kernel data is user accessible.\n");
		/*
		 * Classify this as an error,
		 * unless this is one of those 1:1 vitual-to-physical mappings.
		 */
		if (va == 0 && len == B256M)
			warn = 1;
		else
			err = 1;
	}
	if (err != 0 || warn != 0) {
		prom_printf("Expecting prot=%x, got prot=%x\n",
			PROT_READ | PROT_WRITE, prot);
	}
	ASSERT(ISP2(len));
	kvm_data.kvm_vaddr = (caddr_t)va;
	kvm_data.kvm_size = len;
	kvm_data.kvm_nbats = 1;
	kvm_data.kvm_vprot = prot;
	return (err == 0);
}

/*
 * Find additional mappings that cover kernel data, if any.
 */

uint_t
probe_kdata_more(void)
{
	uintptr_t kdata_va;
	size_t kdata_size;
	uint_t kdata_nbats;
	uintptr_t va;
	pfn_t pfn;
	size_t len;
	uint_t prot;
	uint_t rc;

	/*
	 * See if kernel data is mapped by more than one BAT.
	 * Continue probing at the next virtual address
	 * after the first mapping of kernel data.
	 */
	kdata_va = (uintptr_t)kvm_data.kvm_vaddr;
	kdata_size = kvm_data.kvm_size;
	kdata_nbats = kvm_data.kvm_nbats;
	for (;;) {
		va = kdata_va + kdata_size;
		/*
		 * Don't ask if there might be a mapping for kernel data
		 * when this virtual address is already on record as
		 * a mapping for kernel text.
		 */
		if (va == (uintptr_t)kvm_text.kvm_vaddr)
			break;
		rc = hat_boot_probe(&va, &len, &pfn, &prot);
		if (rc == 0 || prot != kvm_data.kvm_vprot)
			break;
		ASSERT(ISP2(len));
		kdata_size += len;
		++kdata_nbats;
	}
	kvm_data.kvm_vaddr = (caddr_t)kdata_va;
	kvm_data.kvm_size = kdata_size;
	kvm_data.kvm_nbats = kdata_nbats;
	return (1);
}

static uint_t
probe_kernel()
{
	uint_t err;
	uint_t abi_err;

	err = 0;
	err |= !probe_ktext_start();
	if (in_kernel_text(s_data)) {
		/*
		 * One big mapping covers all of kernel text
		 * and at least some kernel data.
		 */
		kvm_data.kvm_vaddr = kvm_text.kvm_vaddr;
		kvm_data.kvm_size = kvm_text.kvm_size;
		kvm_data.kvm_nbats = kvm_text.kvm_nbats;
	} else if (!in_kernel_text(e_text)) {
		err |= !probe_kdata_start();
	}
	if (err == 0) {
		if (!in_kernel_text(s_data))
			err |= !probe_ktext_more();
		if (in_kernel_text(e_data)) {
			kvm_data.kvm_size = kvm_text.kvm_size;
			kvm_data.kvm_nbats = kvm_text.kvm_nbats;
		} else {
			err |= !probe_kdata_more();
		}
	}

	/*
	 * ODW firmware maps kernel text and data only because it covers
	 * everything with a 1:1 virtual-to-physical mapping.
	 * Modify our view of how kernel text and data are mapped,
	 * so that it starts at kernel text, and the size is limited
	 * to 32 Mbytes.
	 */
	if (kvm_text.kvm_vaddr == 0 && kvm_text.kvm_size == B256M) {
		kvm_text.kvm_vaddr = s_text;
		kvm_text.kvm_size = B32M;
	}
	if (kvm_data.kvm_vaddr == 0 && kvm_data.kvm_size == B256M) {
		kvm_data.kvm_vaddr = s_text;
		kvm_data.kvm_size = B32M;
	}

	abi_err = 0;
	if (kvm_text.kvm_vaddr < (caddr_t)KERNELBASE) {
		prom_printf(
			"WARNING: kernel text is mapped below KERNELBASE\n"
			"         kernel text @ %p, KERNELBASE=%p\n",
			kvm_text.kvm_vaddr, (caddr_t)KERNELBASE);
		abi_err = 1;
	}
	if (kvm_data.kvm_vaddr < (caddr_t)KERNELBASE) {
		prom_printf(
			"WARNING: kernel data is mapped below KERNELBASE\n"
			"         kernel data @ %p, KERNELBASE=%p\n",
			kvm_data.kvm_vaddr, (caddr_t)KERNELBASE);
		abi_err = 1;
	}
	if (abi_err) {
		prom_printf(
			"The mapping of the kernel"
			" is not 32-bit PowerPC ABI-compliant.\n"
			"Any attempt to run userland processes"
			" will almost certainly fail.\n");
	}
	ASSERT(in_kernel_text(e_text));
	ASSERT(in_kernel_data(e_data));
	return (err == 0);
}

/*
 * The purpose of startup_memlist is to get the system to the
 * point where it can use kmem_alloc()'s that operate correctly
 * relying on BOP_ALLOC().  This includes allocating page_ts,
 * page hash table, vmem initialized, etc.
 *
 * Boot's versions of physinstalled and physavail are insufficient for
 * the kernel's purposes.  Specifically we don't know which pages that
 * are not in physavail can be reclaimed after boot is gone.
 *
 * This code solves the problem by dividing the address space
 * into 3 regions as it takes over the MMU from the booter.
 *
 * 1) Any (non-nucleus) pages that are mapped at addresses above KERNEL_TEXT
 * can not be used by the kernel.
 *
 * 2) Any free page that happens to be mapped below kernelbase
 * is protected until the boot loader is released, but will then be reclaimed.
 *
 * 3) Boot shouldn't use any address in the remaining area between kernelbase
 * and KERNEL_TEXT.
 *
 * In the case of multiple mappings to the same page, region 1 has precedence
 * over region 2.
 */

/*
 * Enable some debugging messages concerning memory usage.
 */

static int debugging_mem = 1;

static void
print_memlist(char *title, struct memlist *mp)
{
	prom_printf("MEMLIST: %s:\n", title);
	while (mp != NULL)  {
		prom_printf("\tAddress 0x%" PRIx64 ", size 0x%" PRIx64 "\n",
		    mp->address, mp->size);
		mp = mp->next;
	}
}

static void
print_memlist_array(char *title, u_longlong_t *list, size_t nelems)
{
	size_t i;

	prom_printf("MEMLIST: %s:\n", title);
	for (i = 0; i < nelems; i += 2) {
		prom_printf("\tAddress 0x%" PRIx64 ", size 0x%" PRIx64 "\n",
		    list[i], list[i+1]);
	}
}

static u_longlong_t *boot_physinstalled, *boot_physavail, *boot_virtavail;
static size_t boot_physinstalled_len, boot_physavail_len, boot_virtavail_len;

void *
nucleus_alloc_best(size_t *size, char *func, char *objname)
{
	caddr_t p;
	size_t rsize, sz_gap, sz_avail;

	rsize = P2ALIGN(*size, 16);
	sz_gap = e_gap - s_gap;
	sz_avail = e_avail - s_avail;
	prom_printf("nucleus_alloc_best: %s() %s:\n",
		func, objname);
	prom_printf("  remaining space = %lu+%lu, need %lu\n",
		sz_gap, sz_avail, size);
	if (sz_gap >= rsize) {
		p = s_gap;
		s_gap += rsize;
	} else if (sz_avail >= rsize) {
		p = s_avail;
		s_avail += rsize;
	} else if (sz_gap > sz_avail) {
		p = s_gap;
		s_gap = e_gap;
		rsize = sz_gap;
	} else {
		p = s_avail;
		s_avail = e_avail;
		rsize = sz_avail;
	}
	*size = rsize;
	return (p);
}

void *
nucleus_alloc(size_t size, int required, char *func, char *objname)
{
	caddr_t p;
	size_t rsize, sz_gap, sz_avail;

	rsize = P2ALIGN(size, 16);
	sz_gap = e_gap - s_gap;
	sz_avail = e_avail - s_avail;
	prom_printf("nucleus_alloc: %s() %s:\n",
		func, objname);
	prom_printf("  remaining space = %lu+%lu, need %lu\n",
		sz_gap, sz_avail, size);
	if (sz_gap >= rsize) {
		p = s_gap;
		s_gap += rsize;
	} else if (sz_avail >= rsize) {
		p = s_avail;
		s_avail += rsize;
	} else {
		prom_printf("%s/nucleus_alloc: "
			"could not allocate %ul bytes for %s\n.",
			func, size, objname);
		if (required)
			panic("Insufficient space.");
		p = NULL;
	}
	return (p);
}

void *
nucleus_zalloc(size_t size, int required, char *func, char *objname)
{
	void *p;

	p = nucleus_alloc(size, required, func, objname);
	bzero(p, size);
	return (p);
}

void
startup_memlist(void)
{
#if defined(XXX_i86pc)
	size_t memlist_sz;
	size_t memseg_sz;
	size_t pagehash_sz;
	size_t pp_sz;
	caddr_t pagecolor_mem;
	size_t pagecolor_memsz;
	caddr_t page_ctrs_mem;
	size_t page_ctrs_size;
	struct memlist *current;
	pgcnt_t orig_npages = 0;
	extern void startup_build_mem_nodes(struct memlist *);
#endif /* defined(XXX_i86pc) */

	caddr_t pt_vaddr;
	size_t pt_size;
	size_t rsize;
	uint_t memblocks;
	uint_t err;

	PRM_POINT("startup_memlist() starting...");

	/*
	 * Copy in the memlist from boot space, each list
	 * is an {addr, size} array with addr and size of 64-bit (u_longlong_t).
	 * The returned length (boot_*_len) for each_array is
	 * 2 * number of array elements.
	 */

	copy_boot_memlists(&boot_physinstalled, &boot_physinstalled_len,
	    &boot_physavail, &boot_physavail_len,
	    &boot_virtavail, &boot_virtavail_len);

	/*
	* Examine the boot loaders physical memory map to find out:
	* - total memory in system - physinstalled
	* - the max physical address - physmax
	* - the number of segments the intsalled memory comes in
	*/

	memblocks = boot_physinstalled_len >> 1; /* divided by two */
	installed_top_size_memlist_array(boot_physinstalled,
	    boot_physinstalled_len, &physmax, &physinstalled);

	PRM_DEBUG(physmax);
	PRM_DEBUG(physinstalled);
	PRM_DEBUG(memblocks);

	if (!probe_kernel())
		panic("Failed to find kernel text and data mappings.\n");

	/*
	 * Use leftover large page nucleus text/data space
	 * for loadable modules, pagetables, and HMEs.
	 * Use at most modtext_resv and moddata_resv.
	 *
	 * Trial allocation of pagetable first.
	 * If there is enough room for pagetable,
	 * (even with alignment restriction) and moddata_resv,
	 * then go ahead and allocate pagetable and moddata
	 * in kernel data.  If there is not enough room for both,
	 * then allocate pagetable using another BAT, and
	 * use the remaining kernel data for moddata.
	 */
	s_avail = (caddr_t)ROUND_UP_PAGE(e_data);
	e_avail = kvm_data.kvm_vaddr + kvm_data.kvm_size;
	s_gap = (caddr_t)P2ALIGN((uintptr_t)s_avail, 16);
	e_gap = s_gap;
	ppcmmu_alloc_pagetable(s_avail, e_avail, &pt_vaddr, &pt_size);
	if (pt_size != 0) {
		e_gap = pt_vaddr;
		s_avail = pt_vaddr + pt_size;
	}

	/*
	 * Allow modtext_resv and moddata_resv to be tuned.
	 */
	if (modtext_resv == 0)
		modtext_resv = MODTEXT_RESV;
	if (moddata_resv == 0)
		moddata_resv = MODDATA_RESV;

	/*
	 * XXX Should use vmem to divvy up remaining nucleus pages.
	 */

	/*
	 * Try to find some spare space for module text.
	 * Try to fit up to modtext_resv bytes in the available space.
	 * Try to fit in the gap before the pagetable, then
	 * try the available space above the pagetable.
	 */
	rsize = P2ALIGN(modtext_resv, 16);
	s_modtext = nucleus_alloc_best(&rsize, "startup_memlist", "modtext");
	e_modtext = s_modtext + rsize;

	/*
	 * Try to find some spare space for module data.
	 * Try to fit up to moddata_resv bytes in the available space.
	 * Try to fit in the gap before the pagetable, then
	 * try the available space above the pagetable.
	 */
	rsize = P2ALIGN(moddata_resv, 16);
	s_moddata = nucleus_alloc_best(&rsize, "startup_memlist", "moddata");
	e_moddata = s_moddata + rsize;

	econtig = s_avail;

	PRM_DEBUG(pt_vaddr);
	PRM_DEBUG(pt_size);
	PRM_DEBUG(s_modtext);
	PRM_DEBUG(e_modtext);
	PRM_DEBUG(s_moddata);
	PRM_DEBUG(e_moddata);
	PRM_DEBUG(econtig);
	PRM_DEBUG(s_avail);
	PRM_DEBUG(e_avail);

	kernelbase = (uintptr_t)s_text;
	*(uintptr_t *)&_kernelbase = kernelbase;
	*(uintptr_t *)&_userlimit = kernelbase;
	*(uintptr_t *)&_userlimit32 = _userlimit;

	kernelheap = s_avail;
	ekernelheap = e_avail;
	kernelheap_init(kernelheap, ekernelheap, kernelheap + MMU_PAGESIZE,
		kernelheap, kernelheap);
	/*
	 * Initialize the page structures from the memory lists.
	 */
	availrmem_initial = availrmem = freemem = 0;
#if 0
	PRM_POINT("Calling kphysm_init()...");
	boot_npages = kphysm_init(pp_base, memseg_base, 0, boot_npages);
	PRM_POINT("kphysm_init() done");
	PRM_DEBUG(boot_npages);
#endif

	/*
	 * Initialize kernel memory allocator.
	 */
	kmem_init();

	PRM_POINT("startup_memlist() done");
}

void
startup_modules(void)
{
	unsigned int i;
	extern void prom_setup(void);

	PRM_POINT("startup_modules() starting...");

	/*
	 * Read the GMT lag from /etc/rtc_config.
	 */
#if XXXPPC
	/*
	 * This appears to be only in x86 based systems, not sparc
	 * anyway KRTLD is needed to load this anyway and it's not up yet
	 */
	gmt_lag = process_rtc_config_file();
#endif
	/*
	 * Calculate default settings of system parameters based upon
	 * maxusers, yet allow to be overridden via the /etc/system file.
	 */
	PRM_POINT("param_calc");
	param_calc(0);

#if XXXPPC
	mod_setup();
#endif
	/*
	 * Initialize system parameters.
	 */
	PRM_POINT("param_init");
	param_init();

	/*
	 * maxmem is the amount of physical memory we're playing with.
	 */
	maxmem = physmem;

	/*
	 * Initialize the hat layer.
	 */
	PRM_POINT("hat_init");
	hat_init();

	/*
	 * Initialize segment management stuff.
	 */
	PRM_POINT("seg_init");
	seg_init();

	PRM_POINT("modload specfs");
	if (modload("fs", "specfs") == -1)
		halt("Can't load specfs");

	PRM_POINT("modload devfs");
	if (modload("fs", "devfs") == -1)
		halt("Can't load devfs");

	PRM_POINT("dispinit");
	dispinit();

	/*
	 * This is needed here to initialize hw_serial[] for cluster booting.
	 */
	if ((i = modload("misc", "sysinit")) != (unsigned int)-1)
		(void) modunload(i);
	else
		cmn_err(CE_CONT, "sysinit load failed");

	/*
	 * Create a kernel device tree.
	 * First, create rootnex,
	 * then invoke bus specific code to probe devices.
	 */
	setup_ddi();

	/*
	 * Set up the CPU module subsystem.
	 * Modifies the device tree,
	 * so it must be done after setup_ddi().
	 */
	// cmi_init();

	/*
	 * Load all platform specific modules
	 */
	// psm_modload();

	PRM_POINT("startup_modules() done");
}

void
startup_bop_gone(void)
{
	PRM_POINT("startup_bop_gone() starting...");

	/*
	 * Do final allocations of HAT data structures that need to
	 * be allocated before quiescing the boot loader.
	 */
	PRM_POINT("Calling hat_kern_alloc()...");
	hat_kern_alloc();
	PRM_POINT("hat_kern_alloc() done");

	PRM_POINT("startup_bop_gone() done");
}

void
startup_vm(void)
{
	PRM_POINT("startup_vm() starting...");

	PRM_POINT("startup_vm() done");
}

void
startup_end(void)
{
	PRM_POINT("startup_end() starting...");

	/*
	 * Perform tasks that get done after most of the VM
	 * initialization has been done but before the clock
	 * and other devices get started.
	 */
	kern_setup1();

	/*
	 * Perform CPC initialization for this CPU.
	 */
	kcpc_hw_init(CPU);

	/*
	 * Configure the system.
	 */
	PRM_POINT("Calling configure()...");
	configure();		/* set up devices */
	PRM_POINT("configure() done");

	cpu_intr_alloc(CPU, NINTR_THREADS);
	// psm_install();

	/*
	 * We're done with bootops.  We don't unmap the bootstrap yet because
	 * we're still using bootsvcs.
	 */
	PRM_POINT("zeroing out bootops");
	/* XXX - Omit for now	*bootopsp = (struct bootops *)0; */
	bootops = (struct bootops *)NULL;

	PRM_POINT("Enabling interrupts");
	// XXX - Enable Interrupts Here

	PRM_POINT("startup_end() done");
}

void
post_startup(void)
{
	PRM_POINT("post_startup() starting...");

	PRM_POINT("post_startup() done");
}

/*
 * Initialize the platform-specific parts of a page_t.
 */
void
add_physmem_cb(page_t *pp, pfn_t pnum)
{
	pp->p_pagenum = pnum;
	pp->p_mapping = NULL;
	pp->p_share = 0;
	pp->p_mlentry = 0;
}
