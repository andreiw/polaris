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
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"@(#)mp_startup.c	1.108	06/02/22 SMI"

#include <sys/types.h>
#include <sys/thread.h>
#include <sys/cpuvar.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/disp.h>
#include <sys/mmu.h>
#include <sys/class.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/systm.h>
#include <sys/var.h>
#include <sys/vtrace.h>
#include <vm/hat.h>
#include <sys/mmu.h>
#include <vm/as.h>
#include <vm/seg_kmem.h>
#include <sys/kmem.h>
#include <sys/stack.h>
#include <sys/machsystm.h>
#include <sys/traptrace.h>
#include <sys/clock.h>
#include <sys/cpc_impl.h>
#include <sys/chip.h>
#include <sys/dtrace.h>
#include <sys/archsystm.h>
#include <sys/reboot.h>
#include <sys/kdi.h>
#include <sys/memnode.h>

struct cpu	cpus[1];			/* CPU data */
struct cpu	*cpu[NCPU] = {&cpus[0]};	/* pointers to all CPUs */
cpu_core_t	cpu_core[NCPU];			/* cpu_core structures */

/*
 * Useful for disabling MP bring-up for an MP capable kernel
 * (a kernel that was built with MP defined)
 */
int use_mp = 1;

int mp_cpus = 0x1;	/* to be set by platform specific module	*/

/*
 * This variable is used by the hat layer to decide whether or not
 * critical sections are needed to prevent race conditions.  For sun4m,
 * this variable is set once enough MP initialization has been done in
 * order to allow cross calls.
 */
int flushes_require_xcalls = 0;
ulong_t	cpu_ready_set = 1;

void cpuid_pass3(cpu_t *cpu) {}	// XXX Unimplemented
void cpuid_pass4(cpu_t *cpu) {}	// XXX Unimplemented

/*
 * Init CPU info - get CPU type info for processor_info system call.
 */
// XXX On i86pc init_cpu_info() is defined in mp_startup.c
// XXX but on PowerPC it was defined in mlsetup, for some reason.
// XXX Question: where was it defined in Solaris 2.6 PPC?

#if 0
void
init_cpu_info(struct cpu *cp)
{
}
#endif

/*
 * Configure syscall support on this CPU.
 */
/*ARGSUSED*/
static void
init_cpu_syscall(struct cpu *cp)
{
	unimplemented("init_cpu_syscall");
}

/*
 * Multiprocessor initialization.
 *
 * Allocate and initialize the cpu structure, TRAPTRACE buffer, and the
 * startup and idle threads for the specified CPU.
 */
static void
mp_startup_init(int cpun)
{
	struct cpu *cp;
	struct tss *ntss;
	kthread_id_t tp;
	caddr_t	sp;
	int size;
	proc_t *procp;
	extern void idle();

	struct cpu_tables *tablesp;
#if defined(XXX_Unimplemented)

#ifdef TRAPTRACE
	trap_trace_ctl_t *ttc = &trap_trace_ctl[cpun];
#endif

	ASSERT(cpun < NCPU && cpu[cpun] == NULL);

	if ((cp = kmem_zalloc(sizeof (*cp), KM_NOSLEEP)) == NULL) {
		panic("mp_startup_init: cpu%d: "
		    "no memory for cpu structure", cpun);
		/*NOTREACHED*/
	}
	procp = curthread->t_procp;

	mutex_enter(&cpu_lock);
	/*
	 * Initialize the dispatcher first.
	 */
	disp_cpu_init(cp);
	mutex_exit(&cpu_lock);

	cpu_vm_data_init(cp);

	/*
	 * Allocate and initialize the startup thread for this CPU.
	 * Interrupt and process switch stacks get allocated later
	 * when the CPU starts running.
	 */
	tp = thread_create(NULL, 0, NULL, NULL, 0, procp,
	    TS_STOPPED, maxclsyspri);

	/*
	 * Set state to TS_ONPROC since this thread will start running
	 * as soon as the CPU comes online.
	 *
	 * All the other fields of the thread structure are setup by
	 * thread_create().
	 */
	THREAD_ONPROC(tp, cp);
	tp->t_preempt = 1;
	tp->t_bound_cpu = cp;
	tp->t_affinitycnt = 1;
	tp->t_cpu = cp;
	tp->t_disp_queue = cp->cpu_disp;

	/*
	 * Setup thread to start in mp_startup.
	 */
	sp = tp->t_stk;
	tp->t_pc = (uintptr_t)mp_startup;
	tp->t_sp = (uintptr_t)(sp - MINFRAME);

	cp->cpu_id = cpun;
	cp->cpu_self = cp;
	cp->cpu_mask = 1 << cpun;
	cp->cpu_thread = tp;
	cp->cpu_lwp = NULL;
	cp->cpu_dispthread = tp;
	cp->cpu_dispatch_pri = DISP_PRIO(tp);

	/*
	 * cpu_base_spl must be set explicitly here to prevent any blocking
	 * operations in mp_startup from causing the spl of the cpu to drop
	 * to 0 (allowing device interrupts before we're ready) in resume().
	 * cpu_base_spl MUST remain at LOCK_LEVEL until the cpu is CPU_READY.
	 * As an extra bit of security on DEBUG kernels, this is enforced with
	 * an assertion in mp_startup() -- before cpu_base_spl is set to its
	 * proper value.
	 */
	cp->cpu_base_spl = ipltospl(LOCK_LEVEL);

	/*
	 * Now, initialize per-CPU idle thread for this CPU.
	 */
	tp = thread_create(NULL, PAGESIZE, idle, NULL, 0, procp, TS_ONPROC, -1);

	cp->cpu_idle_thread = tp;

	tp->t_preempt = 1;
	tp->t_bound_cpu = cp;
	tp->t_affinitycnt = 1;
	tp->t_cpu = cp;
	tp->t_disp_queue = cp->cpu_disp;

	/*
	 * Bootstrap the CPU for CMT aware scheduling
	 * The rest of the initialization will happen from
	 * mp_startup()
	 */
	chip_bootstrap_cpu(cp);

	/*
	 * Perform CPC intialization on the new CPU.
	 */
	kcpc_hw_init(cp);

	/*
	 * Allocate virtual addresses for cpu_caddr1 and cpu_caddr2
	 * for each CPU.
	 */

	setup_vaddr_for_ppcopy(cp);

	/*
	 * Allocate space for page directory, stack, tss, gdt and idt.
	 * This assumes that kmem_alloc will return memory which is aligned
	 * to the next higher power of 2 or a page(if size > MAXABIG)
	 * If this assumption goes wrong at any time due to change in
	 * kmem alloc, things may not work as the page directory has to be
	 * page aligned
	 */
	if ((tablesp = kmem_zalloc(sizeof (*tablesp), KM_NOSLEEP)) == NULL)
		panic("mp_startup_init: cpu%d cannot allocate tables", cpun);

	if ((uintptr_t)tablesp & ~MMU_STD_PAGEMASK) {
		kmem_free(tablesp, sizeof (struct cpu_tables));
		size = sizeof (struct cpu_tables) + MMU_STD_PAGESIZE;
		tablesp = kmem_zalloc(size, KM_NOSLEEP);
		tablesp = (struct cpu_tables *)
		    (((uintptr_t)tablesp + MMU_STD_PAGESIZE) &
		    MMU_STD_PAGEMASK);
	}

	ntss = cp->cpu_tss = &tablesp->ct_tss;
	cp->cpu_gdt = tablesp->ct_gdt;
	bcopy(CPU->cpu_gdt, cp->cpu_gdt, NGDT * (sizeof (user_desc_t)));

#if defined(__amd64)

	/*
	 * #DF (double fault).
	 */
	ntss->tss_ist1 =
	    (uint64_t)&tablesp->ct_stack[sizeof (tablesp->ct_stack)];

#elif defined(__i386)

	ntss->tss_esp0 = ntss->tss_esp1 = ntss->tss_esp2 = ntss->tss_esp =
	    (uint32_t)&tablesp->ct_stack[sizeof (tablesp->ct_stack)];

	ntss->tss_ss0 = ntss->tss_ss1 = ntss->tss_ss2 = ntss->tss_ss = KDS_SEL;

	ntss->tss_eip = (uint32_t)mp_startup;

	ntss->tss_cs = KCS_SEL;
	ntss->tss_fs = KFS_SEL;
	ntss->tss_gs = KGS_SEL;

	/*
	 * setup kernel %gs.
	 */
	set_usegd(&cp->cpu_gdt[GDT_GS], cp, sizeof (struct cpu) -1, SDT_MEMRWA,
	    SEL_KPL, 0, 1);

#endif	/* __i386 */

	/*
	 * Set I/O bit map offset equal to size of TSS segment limit
	 * for no I/O permission map. This will cause all user I/O
	 * instructions to generate #gp fault.
	 */
	ntss->tss_bitmapbase = sizeof (*ntss);

	/*
	 * setup kernel tss.
	 */
	set_syssegd((system_desc_t *)&cp->cpu_gdt[GDT_KTSS], cp->cpu_tss,
	    sizeof (*cp->cpu_tss) -1, SDT_SYSTSS, SEL_KPL);

	/*
	 * If we have more than one node, each cpu gets a copy of IDT
	 * local to its node. If this is a Pentium box, we use cpu 0's
	 * IDT. cpu 0's IDT has been made read-only to workaround the
	 * cmpxchgl register bug
	 */
	cp->cpu_idt = CPU->cpu_idt;
	if (system_hardware.hd_nodes && x86_type != X86_TYPE_P5) {
		cp->cpu_idt = kmem_alloc(sizeof (idt0), KM_SLEEP);
		bcopy(idt0, cp->cpu_idt, sizeof (idt0));
	}

	/*
	 * Get interrupt priority data from cpu 0
	 */
	cp->cpu_pri_data = CPU->cpu_pri_data;

	hat_cpu_online(cp);

	/* Should remove all entries for the current process/thread here */

#ifdef TRAPTRACE
	/*
	 * If this is a TRAPTRACE kernel, allocate TRAPTRACE buffers for this
	 * CPU.
	 */
	ttc->ttc_first = (uintptr_t)kmem_zalloc(trap_trace_bufsize, KM_SLEEP);
	ttc->ttc_next = ttc->ttc_first;
	ttc->ttc_limit = ttc->ttc_first + trap_trace_bufsize;
#endif

	/*
	 * Record that we have another CPU.
	 */
	mutex_enter(&cpu_lock);
	/*
	 * Initialize the interrupt threads for this CPU
	 */
	cpu_intr_alloc(cp, NINTR_THREADS);
	/*
	 * Add CPU to list of available CPUs.  It'll be on the active list
	 * after mp_startup().
	 */
	cpu_add_unit(cp);
	mutex_exit(&cpu_lock);
#endif /* defined(XXX_Unimplemented) */
}

static ushort_t *mp_map_warm_reset_vector();
static void mp_unmap_warm_reset_vector(ushort_t *warm_reset_vector);

/*ARGSUSED*/
void
start_other_cpus(int cprboot)
{
	unsigned who;
	int cpuid = 0;
	int delays = 0;
	int started_cpu;
	ushort_t *warm_reset_vector = NULL;
	extern int procset;

	/*
	 * Initialize our own cpu_info.
	 */
	init_cpu_info(CPU);

	/*
	 * Initialize our syscall handlers
	 */
	init_cpu_syscall(CPU);

	/*
	 * if only 1 cpu or not using MP, skip the rest of this
	 */
	if (!(mp_cpus & ~(1 << cpuid)) || use_mp == 0) {
		if (use_mp == 0)
			cmn_err(CE_CONT, "?***** Not in MP mode\n");
		goto done;
	}

	/*
	 * perform such initialization as is needed
	 * to be able to take CPUs on- and off-line.
	 */
	cpu_pause_init();

	// xc_init();		/* initialize processor crosscalls */


	flushes_require_xcalls = 1;

	affinity_set(CPU_CURRENT);

	for (who = 0; who < NCPU; who++) {
		if (who == cpuid)
			continue;

		if ((mp_cpus & (1 << who)) == 0)
			continue;

		mp_startup_init(who);
		started_cpu = 1;
		/* (*cpu_startf)(who, rm_platter_pa); */

		while ((procset & (1 << who)) == 0) {

			delay(1);
			if (++delays > (20 * hz)) {

				cmn_err(CE_WARN,
				    "cpu%d failed to start", who);

				mutex_enter(&cpu_lock);
				cpu[who]->cpu_flags = 0;
				cpu_vm_data_destroy(cpu[who]);
				cpu_del_unit(who);
				mutex_exit(&cpu_lock);

				started_cpu = 0;
				break;
			}
		}
		if (!started_cpu)
			continue;
		// if (tsc_gethrtime_enable)
		//	tsc_sync_master(who);


		if (dtrace_cpu_init != NULL) {
			/*
			 * DTrace CPU initialization expects cpu_lock
			 * to be held.
			 */
			mutex_enter(&cpu_lock);
			(*dtrace_cpu_init)(who);
			mutex_exit(&cpu_lock);
		}
	}

	affinity_clear();

	for (who = 0; who < NCPU; who++) {
		if (who == cpuid)
			continue;

		if (!(procset & (1 << who)))
			continue;

		while (!(cpu_ready_set & (1 << who)))
			delay(1);
	}

done:
	unimplemented("start_other_cpus::done");
}

/*
 * Dummy functions - no i86pc platforms support dynamic cpu allocation.
 */
/*ARGSUSED*/
int
mp_cpu_configure(int cpuid)
{
	return (ENOTSUP);		/* not supported */
}

/*ARGSUSED*/
int
mp_cpu_unconfigure(int cpuid)
{
	return (ENOTSUP);		/* not supported */
}

/*
 * Startup function for 'other' CPUs (besides boot cpu).
 * Called from real_mode_start (after *ap_mlsetup).
 *
 * WARNING: until CPU_READY is set, mp_startup and routines called by
 * mp_startup should not call routines (e.g. kmem_free) that could call
 * hat_unload which requires CPU_READY to be set.
 */
void
mp_startup(void)
{
	struct cpu *cp = CPU;
	extern int procset;

	/*
	 * Initialize this CPU's syscall handlers
	 */
	init_cpu_syscall(cp);

	/*
	 * Enable interrupts with spl set to LOCK_LEVEL. LOCK_LEVEL is the
	 * highest level at which a routine is permitted to block on
	 * an adaptive mutex (allows for cpu poke interrupt in case
	 * the cpu is blocked on a mutex and halts). Setting LOCK_LEVEL blocks
	 * device interrupts that may end up in the hat layer issuing cross
	 * calls before CPU_READY is set.
	 */
	(void) splx(ipltospl(LOCK_LEVEL));

	cpuid_pass2(cp);
	cpuid_pass3(cp);
	(void) cpuid_pass4(cp);

	init_cpu_info(cp);

	mutex_enter(&cpu_lock);
	procset |= 1 << cp->cpu_id;
	mutex_exit(&cpu_lock);

	mutex_enter(&cpu_lock);
	/*
	 * It's unfortunate that chip_cpu_init() has to be called here.
	 * It really belongs in cpu_add_unit(), but unfortunately it is
	 * dependent on the cpuid probing, which must be done in the
	 * context of the current CPU. Care must be taken on x86 to ensure
	 * that mp_startup can safely block even though chip_cpu_init() and
	 * cpu_add_active() have not yet been called.
	 */
	chip_cpu_init(cp);
	chip_cpu_startup(cp);

	cp->cpu_flags |= CPU_RUNNING | CPU_READY | CPU_ENABLE | CPU_EXISTS;
	cpu_add_active(cp);
	mutex_exit(&cpu_lock);

	// add_cpunode2devtree(cp->cpu_id, cp->cpu_m.mcpu_cpi);

	/* The base spl should still be at LOCK LEVEL here */
	ASSERT(cp->cpu_base_spl == ipltospl(LOCK_LEVEL));
	set_base_spl();		/* Restore the spl to its proper value */

	(void) spl0();				/* enable interrupts */

	/*
	 * Set up the CPU module for this CPU.  This can't be done before
	 * this CPU is made CPU_READY, because we may (in heterogeneous systems)
	 * need to go load another CPU module.  The act of attempting to load
	 * a module may trigger a cross-call, which will ASSERT unless this
	 * cpu is CPU_READY.
	 */
	// cmi_init();

	if (boothowto & RB_DEBUG)
		kdi_dvec_cpu_init(cp);

	/*
	 * Setting the bit in cpu_ready_set must be the last operation in
	 * processor initialization; the boot CPU will continue to boot once
	 * it sees this bit set for all active CPUs.
	 */
	CPUSET_ATOMIC_ADD(cpu_ready_set, cp->cpu_id);

	/*
	 * Because mp_startup() gets fired off after init() starts, we
	 * can't use the '?' trick to do 'boot -v' printing - so we
	 * always direct the 'cpu .. online' messages to the log.
	 */
	cmn_err(CE_CONT, "!cpu%d initialization complete - online\n",
	    cp->cpu_id);

	/*
	 * Now we are done with the startup thread, so free it up.
	 */
	thread_exit();
	panic("mp_startup: cannot return");
	/*NOTREACHED*/
}


/*
 * Start CPU on user request.
 */
/* ARGSUSED */
int
mp_cpu_start(struct cpu *cp)
{
	ASSERT(MUTEX_HELD(&cpu_lock));
	return (0);
}

/*
 * Stop CPU on user request.
 */
/* ARGSUSED */
int
mp_cpu_stop(struct cpu *cp)
{
	extern int cbe_psm_timer_mode;
	ASSERT(MUTEX_HELD(&cpu_lock));
	return (0);
}

/*
 * Power on CPU.
 */
/* ARGSUSED */
int
mp_cpu_poweron(struct cpu *cp)
{
	ASSERT(MUTEX_HELD(&cpu_lock));
	return (ENOTSUP);		/* not supported */
}

/*
 * Power off CPU.
 */
/* ARGSUSED */
int
mp_cpu_poweroff(struct cpu *cp)
{
	ASSERT(MUTEX_HELD(&cpu_lock));
	return (ENOTSUP);		/* not supported */
}


/*
 * Take the specified CPU out of participation in interrupts.
 */
int
cpu_disable_intr(struct cpu *cp)
{
	// if (psm_disable_intr(cp->cpu_id) != DDI_SUCCESS)
	//	return (EBUSY);

	cp->cpu_flags &= ~CPU_ENABLE;
	return (0);
}

/*
 * Allow the specified CPU to participate in interrupts.
 */
void
cpu_enable_intr(struct cpu *cp)
{
	ASSERT(MUTEX_HELD(&cpu_lock));
	cp->cpu_flags |= CPU_ENABLE;
	// psm_enable_intr(cp->cpu_id);
}

static ushort_t *
mp_map_warm_reset_vector()
{
	unimplemented("mp_map_warm_reset_vector");
	return (NULL);
}

static void
mp_unmap_warm_reset_vector(ushort_t *warm_reset_vector)
{
	// psm_unmap_phys((caddr_t)warm_reset_vector, sizeof (ushort_t *));
}

void
mp_cpu_faulted_enter(struct cpu *cp)
{
	// cmi_faulted_enter(cp);
}

void
mp_cpu_faulted_exit(struct cpu *cp)
{
	// cmi_faulted_exit(cp);
}
