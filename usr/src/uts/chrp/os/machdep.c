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


/* 2.6_leveraged #pragma ident	"@(#)machdep.c	1.142	96/10/17 SMI" */

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/signal.h>
#include <sys/systm.h>
#include <sys/user.h>
#include <sys/map.h>	/* not in S10, but in 2.6 */
#include <sys/mman.h>
#include <sys/vm.h>
#include <sys/memlist.h>

#include <sys/disp.h>
#include <sys/class.h>

#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/kmem.h>

#include <sys/reboot.h>
#include <sys/uadmin.h>
#include <sys/callb.h>   /* in S10, but not 2.6 */

#include <sys/cred.h>
#include <sys/vnode.h>
#include <sys/file.h>

#include <sys/procfs.h>
#include <sys/acct.h>

#include <sys/vfs.h>
#include <sys/dnlc.h>
#include <sys/var.h>
#include <sys/cmn_err.h>
#include <sys/utsname.h>
#include <sys/debug.h>
#include <sys/kdi_impl.h>	/* in S10, but not 2.6 */

#include <sys/dumphdr.h>
#include <sys/bootconf.h>
#include <sys/varargs.h>
#include <sys/promif.h>
#include <sys/prom_isa.h>
#include <sys/prom_plat.h>
#include <sys/modctl.h>		/* for "procfs" hack */
// #include <sys/kvtopdata.h>	/* not in S10, but in 2.6 */

#include <sys/consdev.h>
#include <sys/frame.h>

/*
 * SUNDDI_IMPL
 * Not in S10, but in 2.6
 * so sunddi.h will not redefine splx() et al
 */
#define	SUNDDI_IMPL

#include <sys/sunddi.h>
#include <sys/ddidmareq.h>
#include <sys/psw.h>
#include <sys/regset.h>
#include <sys/reg.h>
// #include <sys/privregs.h>	/* in S10, but not 2.6 */
#include <sys/clock.h>
#include <sys/pte.h>		/* not in S10, but in 2.6 */
#include <sys/cpu.h>
#include <sys/stack.h>
#include <sys/trap.h>
#include <sys/pic.h>
#include <sys/mmu.h>
#include <sys/ppc_subr.h>
#include <vm/hat.h>
#include <vm/anon.h>
#include <vm/as.h>
#include <vm/page.h>
#include <vm/seg.h>
#include <vm/seg_kmem.h>
#include <vm/seg_map.h>
#include <vm/seg_vn.h>
#include <vm/seg_kp.h>
#include <vm/hat_ppcmmu.h>
#include <sys/swap.h>
#include <sys/thread.h>
#include <sys/sysconf.h>
#include <sys/vm_machparam.h>
#include <sys/archsystm.h>
#include <sys/machsystm.h>
#include <sys/machlock.h> /* in S10, but not 2.6 */
// #include <sys/x_call.h>
#include <sys/instance.h>

#include <sys/time.h>
// #include <sys/smp_impldefs.h>
#include <sys/console.h>

/* These are all in S10, not in 2.6 */

#include <sys/atomic.h>
#include <sys/panic.h>
#include <sys/cpuvar.h>
#include <sys/dtrace.h>
#include <sys/bl.h>
#include <sys/nvpair.h>
// #include <sys/x86_archext.h>
#include <sys/pool_pset.h>
#include <sys/autoconf.h>
#include <sys/kdi.h>

#ifdef	TRAPTRACE
#include <sys/traptrace.h>
#endif	/* TRAPTRACE */

#ifdef C2_AUDIT
extern void audit_enterprom(int);
extern void audit_exitprom(int);
#endif


void __NORETURN halt(char *s);

/*
 * Declare these as initialized data so we can patch them.
 */
int noprintf = 0;	/* patch to non-zero to suppress kernel printf's */
int msgbufinit = 1;	/* message buffer has been initialized, ok to printf */

int maxphys = 56 * 1024;	/* XXX See vm_subr.c - max b_count in physio */
caddr_t	p0_va;		/* Virtual address for accessing physical page 0 */
int do_forcefault = 0;  /* don't do forcefaults until after startup() */
int	pokefault = 0;

/*
 * defined here, though unused on ppc,
 * to make kstat_fr.c happy.
 */
int vac;

struct debugvec *dvec = (struct debugvec *)0;

// phys_install	- Total installed physical memory
// phys_avail	- Available (unreserved) physical memory
// virt_avail	- Available (unmapped?) virtual memory
extern struct memlist *phys_install;
extern struct memlist *phys_avail;
extern struct memlist *virt_avail;
extern struct kvtopdata kvtopdata;
extern struct bootops *bootops;		// passed in from boot

int (*getcharptr)(void);
int (*ischarptr)(void);
void (*putcharptr)(char) = prom_putchar;
// XXX int (*putsyscharptr)(char) = console_char_no_output;
int (*putsyscharptr)(char) = NULL;
// XXX void (*putstrptr)(char *, uint_t) = prom_writestr;
void (*putstrptr)(char *, uint_t) = NULL;

void stop_other_cpus();
void debug_enter(char *);

static void complete_panic(void);

kthread_id_t clock_thread;	/* clock interrupt thread pointer */
kmutex_t memseg_lock;		/* lock for searching memory segments */

int	procset = 1;

#ifdef PGINPROF
/*
 * Return the difference (in microseconds)
 * between the current time and a previous
 * time as represented by the arguments.
 */
vmtime(otime, olbolt)
	int otime, olbolt;
{
	return (((time-otime)*hz + lbolt-olbolt)*(1000000/hz));
}

#endif /* PGINPROF */

/*
 * In case console is off, panicstr contains argument
 * to last call to panic.
 */
char *volatile panicstr = NULL;
va_list  panicargs;
int default_panic_timeout;
int panic_timeout = 0;

/*
 * This is the state of the world before the file system are sync'd
 * and system state is dumped. Should be put in panic data structure.
 */
label_t	panic_regs;	/* adb looks at these */
kthread_id_t panic_thread;
kthread_id_t panic_clock_thread;
struct cpu panic_cpu;
kmutex_t paniclock;
extern void int20(void);

#if defined(XXX_OBSOLETE_SOLARIS)

void
__NORETURN
do_panic(const char *fmt, va_list adx)
{
	if (setup_panic(fmt, adx) == 0)
		complete_panic();


	if (!nopanicdebug)
		int20();

	mdboot(A_REBOOT, AD_BOOT, NULL, 0);
}

/*
 * Panic is called on unresolvable fatal errors.
 * It prints "panic: mesg", and then reboots.
 * If we are called twice, then we avoid trying to
 * sync the disks as this often leads to recursive panics.
 */

void
__NORETURN
panic(const char *fmt, ...)
{
	va_list adx;

	va_start(adx, fmt);
	do_panic(fmt, adx);
	va_end(adx);
}

int
setup_panic(char *fmt, va_list adx)
{
	extern int conslogging;
	int s;
	kthread_id_t tp;

#ifndef LOCKNEST	/* No telling what locks we hold when we call panic */
	noprintf = 1;
	s = splzs();

	/*
	 * Allow only threads on panicking cpu to blow through locks.
	 * Use counter to avoid endless looping if code should panic
	 * before panicstr is set.
	 */
	mutex_enter(&paniclock);
	if (panicstr) {
		panicstr = fmt;
		va_copy(panicargs, adx);
		(void) splx(s);
		return (0);
	}

	conslogging = 0;
	panic_cpu = *curthread->t_cpu;
	panicstr = fmt;
	va_copy(panicargs, adx);
	/*
	 * Insure that we are neither preempted nor swapped out
	 * while setting up the panic state.
	 */
	thread_lock(curthread);
	curthread->t_bound_cpu = CPU;
	curthread->t_schedflag |= TS_DONT_SWAP;
	curthread->t_preempt++;
	thread_unlock(curthread);
	panic_thread = curthread;

	/*
	 * Order is important here. We need the cross-call to flush
	 * the caches. Once the QUIESCED bit is set, then all interrupts
	 * below level 9 are taken on the panic cpu. clock_thread is the
	 * exception.
	 */
#ifdef MP
	xc_capture_cpus(CPUSET_ALL_BUT(panic_cpu.cpu_id));
	xc_release_cpus();
	for (i = 0; i < NCPU; i++) {
		if (i != panic_cpu.cpu_id &&
		    cpu[i] != NULL &&
		    (cpu[i]->cpu_flags & CPU_EXISTS)) {
			cpu[i]->cpu_flags &= ~CPU_READY;
			cpu[i]->cpu_flags |= CPU_QUIESCED;
		}
	}
#endif /* MP */
	if (clock_thread)
		clock_thread->t_bound_cpu = CPU;
#ifdef MP
	stop_other_cpus();
	xc_initted = 0;
#endif /* MP */

	/*
	 * Pass this point if the clockthread thread runnable but
	 * not ON_PROC then it will take interrupts on panic_cpu.
	 * Otherwise we create new clock thread to take interrupts
	 * on panic_cpu
	 */
	if (clock_thread &&
	    (clock_thread->t_state == TS_SLEEP || clock_thread->t_lock)) {
		if (CPU->cpu_intr_thread) {
			tp = CPU->cpu_intr_thread;
			CPU->cpu_intr_thread = tp->t_link;
			tp->t_pri = clock_thread->t_pri;
			tp->t_cpu = CPU;
			CPU->cpu_intr_stack = tp->t_stk -= SA(MINFRAME);
			panic_clock_thread = clock_thread;
			clock_thread = tp;
		} else {
			(void) splx(s);
			return (-1);
		}
	}

	/*
	 * If on interrupt stack, allocate new interrupt thread stack
	 */
	if ((CPU->cpu_on_intr)) {
		if (CPU->cpu_intr_thread) {
			tp = CPU->cpu_intr_thread;
			CPU->cpu_intr_thread = tp->t_link;
			CPU->cpu_intr_stack = tp->t_stk -= SA(MINFRAME);
			CPU->cpu_on_intr = 0;
		} else {
			(void) splx(s);
			return (-1);
		}
	}
	(void) splx(s);
	return (0);
#endif	/* LOCKNEST */
}

#endif /* defined(XXX_OBSOLETE_SOLARIS) */


static void
complete_panic(void)
{
	extern void prf(char *, va_list, vnode_t *, int);
	int s;
	static int in_sync = 0;
	static int in_dumpsys = 0;
	extern int sync_timeout;
	va_list	temp_pargs;

	s = splzs();

	noprintf = 0;   /* turn printfs on */

	if (curthread->t_cred == NULL)
		curthread->t_cred = kcred;

	printf("panic: ");
	va_copy(temp_pargs, panicargs);
	// XXX prf(panicstr, temp_pargs, (vnode_t *)NULL, PRW_CONS | PRW_BUF);
	printf("\n");

	if ((boothowto & RB_DEBUG) && (nopanicdebug == 0))
		debug_enter((char *)NULL);

	if (!in_sync) {
		in_sync = 1;
		vfs_syncall();
	} else {
		/* Assume we paniced while syncing and avoid timeout */
		sync_timeout = 0;
	}

	(void) setjmp(&panic_thread->t_pcb);	/* save stack ptr for dump */
	panic_regs = panic_thread->t_pcb;

	if (!in_dumpsys) {
		in_dumpsys = 1;
		dumpsys();
	}
	(void) splx(s);
}

/*
 * Allow interrupt threads to run only don't allow them to nest
 * save the current interrupt count
 */
void
panic_hook()
{
#if defined(XXX_OBSOLETE_SOLARIS)
	int s;

	if (CPU->cpu_id != panic_cpu.cpu_id) {
		for (;;);
	}

	if (panic_thread != curthread ||
	    CPU->cpu_on_intr > panic_cpu.cpu_on_intr)
		return;

	s = spl0();
	(void) splx(s);
#endif /* defined(XXX_OBSOLETE_SOLARIS) */
}

/*
 * XXXPPC NEEDS REVIEW.
 *
 * Machine dependent code to reboot.
 * "mdep" is interpreted as a character pointer; if non-null, it is a pointer
 * to a string to be used as the argument string when rebooting.
 */
/*ARGSUSED*/
void
__NORETURN
mdboot(int cmd, int fcn, char *mdep, boolean_t cb)
{
	int s;
	struct cpu *cpup;
	extern void reset_leaves(void);

	if (!panicstr) {
		kpreempt_disable();
		mutex_enter(&cpu_lock);
		for (s = 1; s < NCPU; s++) {
			cpup = cpu[s];
			if (!cpup || !(cpup->cpu_flags & CPU_EXISTS))
				continue;
			cpup->cpu_flags |= CPU_QUIESCED;
			if (CPU == cpup)
				continue;
			aston(cpup->cpu_dispthread);
#ifdef MP
			poke_cpu(cpup->cpu_id);
#endif
		}
		mutex_exit(&cpu_lock);
		affinity_set(0);
	}

	/*
	 * XXX - rconsvp is set to NULL to ensure that output messages
	 * are sent to the underlying "hardware" device using the
	 * monitor's printf routine since we are in the process of
	 * either rebooting or halting the machine.
	 */
	rconsvp = NULL;

	// XXX s = spl6();
	reset_leaves();		/* call all drivers reset entry point	*/

#ifdef MP
	mutex_enter(&cpu_lock);
	pause_cpus(NULL);
	mutex_exit(&cpu_lock);
#endif
	(void) spl8();
	// XXX (*psm_shutdownf)();

	if (fcn == AD_HALT || fcn == AD_POWEROFF)
		halt((char *)NULL);
	else {
		printf("rebooting...\n");
		prom_reboot("");
	}
	/*NOTREACHED*/
}

/*
 * Machine-dependent portion of dump-checking;
 * verify that a physical address is valid.
 */
int
dump_checksetbit_machdep(u_longlong_t addr)
{
	struct memlist	*pmem;

	for (pmem = phys_install; pmem; pmem = pmem->next) {
		if (pmem->address <= addr &&
		    addr < (pmem->address + pmem->size))
			return (1);
	}
	return (0);
}

/*
 * Temporary address we use to map things when dumping them.
 */
caddr_t	dump_addr;
caddr_t cur_dump_addr;

static int npgs = 0;
static int dbn = 0;

extern void ppcmmu_unload(struct as *, caddr_t, uint_t, int);

/*
 * Called from dumpsys() to ensure the kvtopdata is in the dump.
 * Also does msgbuf since it's no longer in ktextseg.
 *
 * XXX  Not entirely convinced we need to do this specially ..
 */
void
dump_kvtopdata(void)
{
	void *i, *j;
	extern pfn_t va_to_pfn(void *);

#if 0  // XXX
	i = (void *)(((uintptr_t)&kvtopdata) & MMU_PAGEMASK);
	for (j = (uintptr_t)(&kvtopdata + sizeof (kvtopdata)); i < j;
	    i += MMU_PAGESIZE) {
		dump_addpage(va_to_pfn(i));
	}
	i = (caddr_t)&msgbuf;
	j = i + sizeof (msgbuf);
	while (i < j) {
		dump_addpage(va_to_pfn(i));
		i += MMU_PAGESIZE;
	}
#endif /* 0 */
}

#ifdef MP
void
idle_other_cpus()
{
	int cpuid = CPU->cpu_id;
	cpuset_t set;

	ASSERT(cpuid <= NCPU);

	xc_capture_cpus(CPUSET_ALL_BUT(cpuid));
}

void
resume_other_cpus()
{
	int cpuid = CPU->cpu_id;

	ASSERT(cpuid <= NCPU);

	xc_release_cpus();
}

extern void	mp_halt(char *);

void
stop_other_cpus()
{
	int cpuid = CPU->cpu_id;

	ASSERT(cpuid <= NCPU);

	xc_call(NULL, NULL, NULL, X_CALL_HIPRI, CPUSET_ALL_BUT(cpuid),
	    (int (*)())mp_halt);
}

#else /* !MP */
void idle_other_cpus() {}
void resume_other_cpus() {}
void stop_other_cpus() {}
#endif /* MP */

/*
 *	Machine dependent abort sequence handling
 */
void
abort_sequence_enter(char *msg)
{
	if (abort_enable != 0)
		debug_enter(msg);
}

/*
 * Enter debugger.  Called when the user types ctrl-alt-d or whenever
 * code wants to enter the debugger and possibly resume later.
 */
void
debug_enter(msg)
	char	*msg;		/* message to print, possibly NULL */
{
	int s;

#ifdef MP
	if (panic_thread == NULL)
		idle_other_cpus();
#endif	/* MP */

	if (msg)
		prom_printf("%s\n", msg);
	// XXX s = splzs();

	if (boothowto & RB_DEBUG) {
		// XXX int20();
	}
	(void) splx(s);

#ifdef	MP
	if (panic_thread == NULL)
		resume_other_cpus();
#endif	/* MP */

}

/*
 * Halt the machine and return to the monitor
 */
void
__NORETURN
halt(char *s)
{
#ifdef	MP
	stop_other_cpus();	/* send stop signal to other CPUs */
#endif
	if (s)
		prom_printf("(%s) \n", s);
	prom_printf("Halted\n\n");
	prom_exit_to_mon();
	/*NOTREACHED*/
}

/*
 * Enter monitor.  Called via cross-call from stop_other_cpus().
 */
void
mp_halt(char *msg)
{
	if (msg)
		prom_printf("%s\n", msg);

	/*CONSTANTCONDITION*/
	while (1);
}

/*
 * Console put and get character routines.
 */
/*ARGSUSED1*/
void
cnputs(char *buf, uint_t bufsize, int device_in_use)
{
	(*putstrptr)(buf, bufsize);
}

/*ARGSUSED1*/
void
cnputc(int c, int device_in_use)
{
	if (c == '\n')
		(*putcharptr)('\r');
	(*putcharptr)(c);
}

int
cngetc()
{
	return ((int)prom_getchar());
}

/*
 * Get a character from the console.
 */
int
getchar()
{
	int c;

	c = cngetc();
	if (c == '\r')
		c = '\n';
	cnputc(c, 0);
	return (c);
}

/*
 * Get a line from the console.
 */
void
gets(char *cp)
{
	char *lp;
	int c;

	lp = cp;
	for (;;) {
		c = getchar() & 0177;
		switch (c) {

		case '\n':
			*lp++ = '\0';
			return;

		case 0177:
			cnputc('\b', 0);
			/*FALLTHROUGH*/
		case '\b':
			cnputc(' ', 0);
			cnputc('\b', 0);
			/*FALLTHROUGH*/
		case '#':
			lp--;
			if (lp < cp)
				lp = cp;
			continue;

		case 'u'&037:
			lp = cp;
			cnputc('\n', 0);
			continue;

		default:
			*lp++ = c;
			break;
		}
	}
}

/*
 * Allocate threads and stacks for interrupt handling.
 */
#define	NINTR_THREADS	(LOCK_LEVEL-1)	/* number of interrupt threads */

void
init_intr_threads(cp)
	struct cpu *cp;
{
	int i;

	for (i = 0; i < NINTR_THREADS; i++)
		(void) thread_create_intr(cp);

	cp->cpu_intr_stack = (caddr_t)segkp_get(segkp, INTR_STACK_SIZE,
		KPD_HASREDZONE | KPD_NO_ANON | KPD_LOCKED) +
		INTR_STACK_SIZE - SA(MINFRAME);
}

void
init_clock_thread()
{
	kthread_id_t tp;

	/*
	 * Create clock interrupt thread.
	 * The state is initially TS_FREE.  Think of this thread on
	 * a private free list until it runs.
	 * The priority is set by clock_intr (in locore) on every clock
	 * interrupt, therefore setting it here is unnecessary.
	 */
	tp = thread_create(NULL, INTR_STACK_SIZE, NULL, NULL, 0,
		&p0, TS_FREE, 0);
	tp->t_stk -= SA(MINFRAME);
	tp->t_flag |= T_INTR_THREAD;	/* for clock()'s tick checking */
	clock_thread = tp;
}

/*
 * XXX These probably ought to live somewhere else
 * XXX They are called from mem.c
 */

/*
 * Convert page frame number to an OBMEM page frame number
 * (i.e. put in the type bits -- zero for this implementation)
 */
int
impl_obmem_pfnum(int pf)
{
	return (pf);
}

/*
 * A run-time interface to the MAKE_PFNUM macro.
 */
int
impl_make_pfnum(struct pte *pte)
{
	return (ptep_to_pfn(pte));
}

int
debug_info()
{
	struct memlist *pmem;
	uint_t first_page, num_free_pages;

	// for (pmem = bootops->boot_mem->physavail; pmem; pmem = pmem->next) {
	for (pmem = NULL; pmem; pmem = pmem->next) {
		first_page = mmu_btop(pmem->address);
		num_free_pages = mmu_btopr(pmem->size);

		prom_printf("(DEBUG)Phys addr = %llx, Phys size = %llx\n",
		    pmem->address, pmem->size);
		prom_printf("(DEBUG)first_page: %x, last_page: %x\n",
		    first_page, first_page + num_free_pages);
	}
	return (0);
}

/*
 * Return the number of ticks per second of the highest
 * resolution clock or free running timer in the system.
 * XXX - if/when we provide higher resolution data, update this.
 */
int
getclkfreq()
{
	return (hz);
}

/*ARGSUSED1*/
static int
read_1275(cell_t args[], cell_t rets[], int nargs, int nrets)
{
	if (((int)args[2] >= 1) && (*ischarptr)()) {
		rets[0] = 1;
		*(char *)args[1] = (*getcharptr)();
	} else
		rets[0] = 0;
	return (0);
}

/*ARGSUSED1*/
static int
write_1275(cell_t args[], cell_t rets[], int nargs, int nrets)
{
	int i;
	char *cp;

	for (i = 0, cp = (char *)args[1]; i < (int)args[2]; i++)
		(*putsyscharptr)(cp[i]);
	rets[0] = (int)args[2];
	return (0);
}

/*ARGSUSED*/
static int
cif_reboot(cell_t args[], cell_t rets[], int nargs, int nrets)
{
	/* This function implements "halt" for now.  Change in 2.6? */
	prom_printf("Press any key to reboot... ");
	(void) prom_getchar();
	prom_printf("\n");
	prom_reboot("");
	return (0);
}

#define	ins(cbp) ((cell_t *)((char *)cbp + sizeof (*cbp)))
#define	outs(cbp) \
	((cell_t *)(((cell_t *)((char *)cbp + sizeof (*cbp))) + cbp->num_args))

struct callback_arg {
	char *callback_name;
	cell_t num_args;
	cell_t num_rets;
	/* cell_t args[num_args]; */
	/* cell_t rets[num_rets]; */
};

static struct callback {
	const char *const callback_name;
	int (* const callback_func)(cell_t [], cell_t [], int, int);
} callbacks[] = {
	"write", write_1275,
	"read", read_1275,
#if 0
	"boot", cif_reboot,
#endif
	"exit", cif_reboot,
	"enter", cif_reboot,
	(char *)0,
};

static char *passthru_callbacks[] = {
	"setprop",
	"boot",
	(char *)0,
};

static int (*passthru_cif_handler)(struct callback_arg *);

static int
kern_cif_handler(struct callback_arg *cbarg)
{
	struct callback *cbp;
	cell_t *args;
	cell_t *rets;
	int nargs;
	int nrets;
	int s;
	int retval = -1;
	char **cp;
	char *modname = NULL;
	struct modnameslist {
		char *name;
		struct modnameslist *next;
	};
	struct modnameslist *offender;
	static struct modnameslist *cif_offenders = NULL;
	extern caddr_t caller3(void);

	s = clear_int_flag();

	for (cbp = &callbacks[0]; cbp->callback_name != 0; ++cbp) {
		if (strcmp(cbp->callback_name, cbarg->callback_name))
			continue;
		args = ins(cbarg);
		rets = outs(cbarg);
		nargs = (int)cbarg->num_args;
		nrets = (int)cbarg->num_rets;

		retval = (*cbp->callback_func)(args, rets, nargs, nrets);
		goto done;
	}

	for (cp = &passthru_callbacks[0]; *cp != 0; ++cp) {
		if (strcmp(*cp, cbarg->callback_name))
			continue;

		retval = (**passthru_cif_handler)(cbarg);
		goto done;
	}

	/*
	 * Someone is calling into the prom when they should
	 * actually be using the prom_config routines.
	 * Since the prom is not yet being unmapped, we can let
	 * these calls through but we want to print a warning
	 * with the name of the offending module.  We only print
	 * the message once per module to avoid flooding someone's
	 * console with warning messages.
	 *
	 * To get to the module name, we need to walk the stack...
	 * normally called thru the promif library - so we need
	 * to find out who called the promif function by looking up
	 * 3 stack frames.
	 */

	modname = mod_containing_pc(caller3());
	if (modname == NULL)
		modname = "<unknown>";

	/*
	 * only print a warning once per offending module
	 */
	offender = cif_offenders;
	while (offender != NULL) {
		if (strcmp(offender->name, modname) == 0) {
			break;
		}
		offender = offender->next;
	}
	if (offender == NULL) {
		/*
		 * This module is a first time offender
		 * Print a warning and add him to the offender list
		 */
		cmn_err(CE_WARN, "Late prom call for the <%s> "
		    "service from module <%s>.",
		    cbarg->callback_name, modname);
		offender = kmem_alloc(
		    sizeof (struct modnameslist), KM_NOSLEEP);
		if (offender != NULL) {
			offender->name = modname;
			modname = NULL;
			offender->next = cif_offenders;
			cif_offenders = offender;
		}
	}

#if 1
	retval = (**passthru_cif_handler)(cbarg);
#else
	cmn_err(CE_PANIC, "prom_xxx calls not allowed this late\n");
#endif

done:
	restore_int_flag(s);
	return (retval);
}

void
release_bootstrap()
{
	extern void **bootopsp;
	extern void unload_boot();

	/*
	 * We're done with boot.
	 */
	*bootopsp = (struct bootops *)0;
	bootops = (struct bootops *)NULL;

	/*
	 * Now we can unload mapping(s) for boot itself.
	 *
	 * Note: The physical pages used by boot are already included in the
	 *	 phys_avail list, here we just unload the mappings for the boot.
	 */
	unload_boot();
}

void
release_Open_Firmware()
{
	extern int (**cifp)(struct callback_arg *);
	extern int (*cif_handler)(struct callback_arg *);

#ifdef	DEBUG
	/*
	 * Remember, you can turn these on with boot -v.
	 */
	cmn_err(CE_CONT,
	    "?release_Open_Firmware:  Switching kadb, /dev/console"
	    "%s to internal I/O.\n",
	    (putcharptr == prom_putchar) ? ", kernel" : "");
#endif

	/* save cif_handler for late calls */
	passthru_cif_handler = cif_handler;
	/* kadb uses kern_cif_handler */
	*cifp = kern_cif_handler;
	/* kernel uses kern_cif_handler */
	cif_handler = kern_cif_handler;

#ifdef	DEBUG
	cmn_err(CE_CONT,
	    "?release_Open_Firmware:  kadb, /dev/console"
	    "%s now on internal I/O.\n",
	    (putcharptr == prom_putchar) ? ", kernel" : "");
#endif
}

/*
 *	the interface to the outside world
 */

/*
 * poll_port:
 *   Wait for a register to achieve a specific state.
 *   Arguments are a mask of bits we care about, and two sub-masks.
 *   To return normally, all the bits in the first sub-mask must be ON,
 *   all the bits in the second sub-mask must be OFF.
 *   If about seconds pass without the register achieving
 *   the desired bit configuration, we return 1, else 0.
 */
int
poll_port(ushort_t port, ushort_t mask, ushort_t onbits, ushort_t offbits)
{
	int i;
	ushort_t maskval;

	for (i = 500000; i; i--) {
		maskval = inb(port) & mask;
		if (((maskval & onbits) == onbits) &&
			((maskval & offbits) == 0))
			return (0);
		drv_usecwait(10);
	}
	return (1);
}

int
getlongprop(int id, char *prop)
{
	if (id == 0) {
		if (strcmp(prop, "name") == 0)
			return ((int)"prep");
		prom_printf("getlongprop: root property '%s' not defined.\n",
			prop);
	}
	return (0);
}

int ticks_til_clock;
int unix_tick;
int klustsize = PAGESIZE;

#ifdef MP
/*
 * set_idle_cpu is called from idle() when a CPU becomes idle.
 */
static uint_t last_idle_cpu;

/*ARGSUSED*/
void
set_idle_cpu(int cpun)
{
	last_idle_cpu = cpun;
	(*psm_set_idle_cpuf)(cpun);
}

/*
 * unset_idle_cpu is called from idle() when a CPU is no longer idle.
 */
/*ARGSUSED*/
void
unset_idle_cpu(int cpun)
{
	(*psm_unset_idle_cpuf)(cpun);
}
#endif /* MP */

/*
 * Get hi-res time-of-day, in seconds and nanoseconds.
 */
void
gethrestime(timestruc_t *tp)
{
#if 0
	int		lock_prev;
	int		sec;
	int		nsec;
	hrtime_t	t;
	unsigned int 	fract;
	longlong_t	adj;
	volatile struct timespec *hrestimep;	/* need volatile here */

	hrestimep = (volatile struct timespec *)&hrestime;
	do {
		lock_prev = hres_lock;
		sec = hrestimep->tv_sec;
		nsec = hrestimep->tv_nsec;
		adj = hrestime_adj;
		t = get_time_base() - tb_at_tick;
		/*
		 * Ignore low-order bit of earlier lock value to detect whether
		 * the lock is now held, or was held earlier and now.
		 */
	} while (hres_lock != (lock_prev & ~1));

	/*
	 * Convert time base since tick to nanoseconds.
	 * The shifts allow timebase period to be in fractional nanoseconds
	 * and are done in two parts to maintain precision while
	 * avoiding overflow.
	 */
	t = ((t >> NSEC_SHIFT1) * timebase_period) >> NSEC_SHIFT2;
	nsec += t;

	/*
	 * Add partial adjustment based on time since tick.
	 */
	if (adj != 0) {
		fract = ((unsigned int) t) >> ADJ_SHIFT;
		if (adj > 0) {
			if (adj < fract)
				fract = (unsigned int)adj;
			nsec += fract;
		} else {
			if (-adj < fract)	/* remember, adj is neg */
				fract = -adj;
			nsec -= fract;
		}
	}

	/*
	 * correct for overflows.
	 */
	while (nsec >= NANOSEC) {
		nsec -= NANOSEC;
		sec++;
	}
	tp->tv_sec = sec;
	tp->tv_nsec = nsec;
#endif /* 0 */
}

/*
 * Initialize kernel thread's stack
 */

caddr_t
thread_stk_init(caddr_t stk)
{
	return (stk - SA(MINFRAME));
}

/*
 * Initialize lwp's kernel stack.
 */
caddr_t
lwp_stk_init(klwp_t *lwp, caddr_t stk)
{
	caddr_t oldstk;

	oldstk = stk;
	stk -= SA(sizeof (struct regs) + MINFRAME);
	bzero((caddr_t)stk, (size_t)(oldstk - stk));
	lwp->lwp_regs = (void *)(stk + REGOFF);

	return (stk);
}
