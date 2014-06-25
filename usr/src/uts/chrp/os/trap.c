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

#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/param.h>
#include <sys/signal.h>
#include <sys/systm.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/disp.h>
#include <sys/class.h>
#include <sys/syscall.h>
#include <sys/cpuvar.h>
#include <sys/vm.h>
#if defined(XXX_OBSOLETE_SOLARIS)
#include <sys/msgbuf.h>
#endif /* defined(XXX_OBSOLETE_SOLARIS) */
#include <sys/sysinfo.h>
#include <sys/fault.h>
#include <sys/frame.h>
#include <sys/stack.h>
#include <sys/mmu.h>
#include <sys/pcb.h>
#include <sys/cpu.h>
#include <sys/psw.h>
#include <sys/pte.h>
#include <sys/reg.h>
#include <sys/trap.h>
#include <sys/kmem.h>
#include <sys/vtrace.h>
#include <sys/archsystm.h>
#include <sys/fpu/fpusystm.h>
#include <sys/cmn_err.h>
#include <sys/prsystm.h>
#include <sys/mutex_impl.h>
#include <sys/archsystm.h>
#include <sys/machsystm.h>
#include <sys/ppc_subr.h>

#include <vm/hat.h>
#include <vm/hat_ppcmmu.h>

#include <vm/seg_kmem.h>
#include <vm/as.h>
#include <vm/seg.h>

#include <sys/procfs.h>

#include <sys/modctl.h>
#include <sys/aio_impl.h>

#include <sys/reboot.h>
#include <sys/debug.h>

#include <sys/tnf.h>
#include <sys/tnf_probe.h>
#include <sys/traptrace.h>

#define	USER	0x10000		/* user-mode flag added to trap type */

static const char *trap_desc(uint_t);

static int tudebug = 0;
static int tudebugbpt = 0;
static int tudebugfpe = 0;
static int alignfaults = 0;
int panic_reg;	/* used by 'adb -k' */

#if defined(TRAPDEBUG) || defined(lint)
static int tdebug = 0;
static int lodebug = 0;
static int faultdebug = 0;
#else
#define	tdebug	0
#define	lodebug	0
#define	faultdebug	0
#endif /* defined(TRAPDEBUG) || defined(lint) */

static void
showregs(uint_t type, struct regs *rp, caddr_t addr, uint_t dsisr);
void traceback(caddr_t);
void tracedump(void);

struct trap_info {
	struct regs *trap_regs;
	uint_t trap_type;
	caddr_t trap_addr;
};

static void __NORETURN
die(uint_t type, struct regs *rp, caddr_t addr, uint_t dsisr,
		processorid_t cpuid)
{
	struct trap_info ti;
	const char *trap_name;

	panic_reg = (int)rp;
	type &= ~USER;
	trap_name = trap_desc(type);

#ifdef TRAPTRACE
	TRAPTRACE_FREEZE;
#endif

	ti.trap_regs = rp;
	ti.trap_type = type;
	ti.trap_addr = addr;

	curthread->t_panic_trap = &ti;

	if (type == T_DATA_FAULT && addr < (caddr_t)KERNELBASE) {
		panic("BAD TRAP: type=%x (%s) rp=%p addr=%p dsisr=0x%x "
		    "occurred in module \"%s\" due to %s",
		    type, trap_name, (void *)rp, (void *)addr, dsisr,
		    mod_containing_pc((caddr_t)rp->r_pc),
		    addr < (caddr_t)PAGESIZE ?
		    "a NULL pointer dereference" :
		    "an illegal access to a user address");
	} else {
		panic("BAD TRAP: type=%x (%s) rp=%p addr=%p dsisr=0x%x",
		    type, trap_name, (void *)rp, (void *)addr, dsisr);
	}
}

// XXX showregs(type, rp, addr, dsisr);
// XXX traceback((caddr_t)rp->r_sp);

/* this is *very* temporary */
#define	instr_size(rp, addrp, rw)	(4)

/*
 * Level2 trap handler:
 *
 * Called from the level1 trap handler when a processor trap occurs.
 *
 * Note: All user-level traps that might call stop() must exit
 * trap() by 'goto out' or by falling through.
 * Note: Argument "type" is trap type (vector offset)
 */
void
trap(struct regs *rp, uint_t type, caddr_t dar, uint_t dsisr,
		processorid_t cpuid)
{
	kthread_t *cur_thread = curthread;
	caddr_t addr = 0;
	enum seg_rw rw;
	extern int stop_on_fault(uint_t, k_siginfo_t *);
	proc_t *p = ttoproc(cur_thread);
	klwp_id_t lwp = ttolwp(cur_thread);
	clock_t syst;
	uint_t lofault;
	faultcode_t pagefault(), res;
	k_siginfo_t siginfo;
	uint_t fault = 0;
	int driver_mutex = 0;	/* get unsafe_driver before returning */
	int mstate;
	int sicode = 0;
	char *badaddr;
	ulong_t inst;
	int watchcode;
	int watchpage;
	caddr_t vaddr;
	size_t sz;
	int ta;

       	prom_printf("Saw exception number 0x%x\n", type);


#ifdef XXXPPC
        /* XXX dtrace plays here just trying to get through the 1st time */
        CPU_STATS_ADDQ(CPU, sys, trap, 1);
#endif
	syst = p->p_stime;

	ASSERT(cur_thread->t_schedflag & TS_DONT_SWAP);

	if (type == T_DATA_FAULT) {
		if (dsisr & DSISR_STORE)
			rw = S_WRITE;
		else
			rw = S_READ;
		addr = dar;
	} else {
		rw = S_READ;
		addr = (caddr_t)rp->r_srr0;
	}

	if (tdebug)
		showregs(type, rp, addr, dsisr);

	if (USERMODE(rp->r_msr)) {
		/*
		 * Set up the current cred to use during this trap.
		 * u_cred no longer exists.  t_cred is used instead.
		 * The current process credential applies to the thread
		 * for the entire trap.  If trapping from the kernel,
		 * this should already be set up.
		 */
		if (cur_thread->t_cred != p->p_cred) {
			cred_t *oldcred = cur_thread->t_cred;
			/*
			 * DTrace accesses t_cred in probe context.
			 * t_cred must always be either NULL,
			 * or point to a valid, allocated cred structure.
			 */
			cur_thread->t_cred = crgetcred();
			crfree(oldcred);
		}
		ASSERT(lwp != NULL);
		type |= USER;
		ASSERT(lwptoregs(lwp) == rp);
		lwp->lwp_state = LWP_SYS;

		switch (type) {
		case T_DATA_FAULT + USER:
			mstate = LMS_DFAULT;
			break;
		case T_TEXT_FAULT + USER:
			mstate = LMS_TFAULT;
			break;
		default:
			mstate = LMS_TRAP;
			break;
		}
		/* Kernel probe */
		TNF_PROBE_1(thread_state, "thread", /* CSTYLED */,
		    tnf_microstate, state, mstate);

		mstate = new_mstate(cur_thread, mstate);
		bzero(&siginfo, sizeof (siginfo));
	} else {
#if defined(XXX_OBSOLETE_SOLARIS)
		if (MUTEX_OWNER_LOCK(&unsafe_driver) &&
		    UNSAFE_DRIVER_LOCK_HELD()) {
			driver_mutex = 1;
			mutex_exit(&unsafe_driver);
		}
#endif /* defined(XXX_OBSOLETE_SOLARIS) */
	}

	switch (type) {

	default:
		if (! (type & USER))
			die(type, rp, addr, dsisr, cpuid);

		if (tudebug)
			showregs(type, rp, addr, dsisr);
		printf("trap: Unknown trap type %d in user mode\n",
			(type & ~USER));
		bzero((caddr_t)&siginfo, sizeof (siginfo));
		siginfo.si_signo = SIGILL;
		siginfo.si_code  = ILL_ILLTRP;
		siginfo.si_addr  = (caddr_t)rp->r_pc;
		siginfo.si_trapno = type & ~USER;
		fault = FLTILL;
		break;

	case T_MACH_CHECK + USER:	/* machine check trap */
		/*
		 * Normally machine check interrupts are not recoverable.
		 * And the least we can do is to try to continue by sending
		 * a signal to the current process (which may not have
		 * caused it if this happend asynchronously).
		 */
		if (tudebug)
			showregs(type, rp, addr, dsisr);
		bzero((caddr_t)&siginfo, sizeof (siginfo));
		siginfo.si_signo = SIGBUS;
		siginfo.si_code = BUS_ADRERR;
		siginfo.si_addr = (caddr_t)rp->r_pc;
		fault = FLTACCESS;
		break;
	case T_MACH_CHECK:		/* machine check trap */
		/*
		 * Normally machine check interrupts are not recoverable.
		 * If the current thread is expecting this (e.g during
		 * bus probe) then we try to continue by doing a longjmp
		 * back to the probe function assuming that it caused
		 * this error.
		 *
		 * Note: if this has happend asynchronously (which we can't
		 * tell easily) then the above may not be right.
		 */
		if (tudebug)
			showregs(type, rp, (caddr_t)0, 0);
#if defined(XXX_OBSOLETE_SOLARIS)
		/* may have been expected by C (e.g. bus probe) */
		if (cur_thread->t_nofault) {
			label_t *ftmp;

			ftmp = cur_thread->t_nofault;
			cur_thread->t_nofault = 0;
#ifndef LOCKNEST
			if (driver_mutex)
				mutex_enter(&unsafe_driver);
#endif /* LOCKNEST */
			longjmp(ftmp);
		}
#endif /* defined(XXX_OBSOLETE_SOLARIS) */

		if (cur_thread->t_lofault) {
			/* lofault is set (e.g bcopy()); set the return pc */
			rp->r_r3 = EFAULT;
			rp->r_pc = cur_thread->t_lofault;
			goto cleanup;
		}

		if (boothowto & RB_DEBUG)
			debug_enter((char *)NULL);
		die(type, rp, addr, dsisr, cpuid);
	case T_RESET:			/* reset trap */
	case T_RESET + USER:		/* reset trap */

		if (tudebug)
			showregs(type, rp, (caddr_t)0, 0);
		if (boothowto & RB_DEBUG)
			debug_enter((char *)NULL);
		die(type, rp, addr, dsisr, cpuid);

	case T_ALIGNMENT:	/* alignment error in system mode */

		/* Registers SRR0, SRR1, and DSISR have the information */
		if (cur_thread->t_lofault && cur_thread->t_onfault) {
			label_t *ftmp;

			ftmp = cur_thread->t_onfault;
			cur_thread->t_onfault = NULL;
			cur_thread->t_lofault = 0;
#if defined(XXX_OBSOLETE_SOLARIS)
#ifndef	LOCKNEST
			if (driver_mutex)
				mutex_enter(&unsafe_driver);
#endif	LOCKNEST
#endif /* defined(XXX_OBSOLETE_SOLARIS) */
			longjmp(ftmp);
		}
		die(type, rp, addr, dsisr, cpuid);

	case T_ALIGNMENT + USER:	/* user alignment error */

		/* Registers SRR0, SRR1, and DSISR have the information */
		if (tudebug)
			showregs(type, rp, addr, dsisr);
		alignfaults++;
		bzero((caddr_t)&siginfo, sizeof (siginfo));
		siginfo.si_signo = SIGBUS;
		siginfo.si_code = BUS_ADRALN;
		if (rp->r_pc & 3) 	/* offending address, if pc */
			siginfo.si_addr = (caddr_t)rp->r_pc;
		else
			siginfo.si_addr = dar;
		fault = FLTACCESS;
		break;

	case T_TEXT_FAULT:	/* system text access exception */

		if (lodebug)
			showregs(type, rp, addr, dsisr);

		die(type, rp, addr, dsisr, cpuid);

	case T_DATA_FAULT:	/* system data access exception */

		/* Registers SRR0, SRR1, DAR, and DSISR have the information */

#if defined(XXX_OBSOLETE_SOLARIS)
		/* may have been expected by C (e.g. bus probe) */
		if (cur_thread->t_nofault) {
			label_t *ftmp;

			ftmp = cur_thread->t_nofault;
			cur_thread->t_nofault = 0;
#ifndef LOCKNEST
			if (driver_mutex)
				mutex_enter(&unsafe_driver);
#endif /* LOCKNEST */
			longjmp(ftmp);
		}
#endif /* defined(XXX_OBSOLETE_SOLARIS) */

		if (dsisr & DSISR_DATA_BKPT) {
			/* HW break point data access trap */
			if (boothowto & RB_DEBUG)
				debug_enter((char *)NULL);
			else
				die(type, rp, addr, dsisr, cpuid);
		}

		/*
		 * See if we can handle as pagefault. Save lofault
		 * across this. Here we assume that an address
		 * less than KERNELBASE is a user fault.
		 * We can do this as copy.s routines verify that the
		 * starting address is less than KERNELBASE before
		 * starting and because we know that we always have
		 * KERNELBASE mapped as invalid to serve as a "barrier".
		 */
		lofault = cur_thread->t_lofault;
		cur_thread->t_lofault = 0;

		if (cur_thread->t_proc_flag & TP_MSACCT)
			mstate = new_mstate(cur_thread, LMS_KFAULT);
		else
			mstate = LMS_SYSTEM;

		if (addr < (caddr_t)KERNELBASE) {
			enum fault_type flt_type;

			if (lofault == 0)
				die(type, rp, addr, dsisr, cpuid);
			flt_type = (dsisr & DSISR_PROT) ? F_PROT : F_INVAL;
			res = pagefault(addr, flt_type, rw, 0);
			if (res == FC_NOMAP && addr < (caddr_t)USRSTACK &&
			    grow(addr))
				res = 0;
		} else {
			enum fault_type flt_type;

			flt_type = (dsisr & DSISR_PROT) ? F_PROT : F_INVAL;
			res = pagefault(addr, flt_type, rw, 1);
		}

		if (cur_thread->t_proc_flag & TP_MSACCT)
			(void) new_mstate(cur_thread, mstate);

		/*
		 * Restore lofault.  If we resolved the fault, exit.
		 * If we didn't and lofault wasn't set, die.
		 */
		cur_thread->t_lofault = lofault;
		if (res == 0)
			goto cleanup;

#ifdef XXX_NEEDED
		/*
		 * Check for mutex_exit hook.  This is to protect mutex_exit
		 * from deallocated locks.  It would be too expensive to use
		 * nofault there.
		 * XXXPPC - review.  Brian, is this needed?
		 */
		{
			void	mutex_exit_nofault(void);
			void	mutex_exit_fault(kmutex_t *);

			if (rp->r_pc == (int)mutex_exit_nofault) {
				rp->r_pc = (int)mutex_exit_fault;
				goto cleanup;
			}
		}
#endif

		if (lofault == 0)
			die(type, rp, addr, dsisr, cpuid);

		/*
		 * Cannot resolve fault.  Return to lofault.
		 */
		if (lodebug) {
			showregs(type, rp, addr, dsisr);
			traceback((caddr_t)rp->r_sp);
		}
		if (FC_CODE(res) == FC_OBJERR)
			res = FC_ERRNO(res);
		else
			res = EFAULT;
		rp->r_r3 = res;
		rp->r_pc = cur_thread->t_lofault;
		goto cleanup;

	case T_TEXT_FAULT + USER:	/* user instruction access exception */
		rw = S_EXEC;
		goto user_fault;
	case T_DATA_FAULT + USER:	/* user data access exception */

		/* Registers SRR0, SRR1, DAR, and DSISR have the information */

		if (dsisr & DSISR_DATA_BKPT) {
			/* HW break point data access trap */
			/* XXXPPC - debug registers support? */
			if (tudebug && tudebugbpt)
				showregs(type, rp, addr, dsisr);
			bzero((caddr_t)&siginfo, sizeof (siginfo));
			siginfo.si_signo = SIGTRAP;
			siginfo.si_code  = TRAP_BRKPT;
			siginfo.si_addr  = (caddr_t)rp->r_pc;
			fault = FLTBPT;
			break;
		}
		/*
		 * Logic common to {T_TEXT_FAULT + USER, T_DATA_FAULT + USER}
		 */
	user_fault:

		if (faultdebug) {
			char *fault_str;

			switch (rw) {
			case S_READ:
				fault_str = "read";
				break;
			case S_WRITE:
				fault_str = "write";
				break;
			case S_EXEC:
				fault_str = "instruction";
				break;
			default:
				fault_str = "";
				break;
			}
			printf("user %s fault:  addr=0x%x dsisr=0x%x\n",
			    fault_str, (int)addr, dsisr);
		}

		ASSERT(!(cur_thread->t_flag & T_WATCHPT));
		watchpage = (pr_watch_active(p) && pr_is_watchpage(addr, rw));
		vaddr = addr;
		if (watchpage &&
		    (sz = instr_size(rp, &vaddr, rw)) > 0 &&
		    (watchcode = pr_is_watchpoint(&vaddr, &ta,
		    sz, NULL, rw)) != 0) {
			if (ta) {
				do_watch_step(vaddr, sz, rw,
					watchcode, rp->r_pc);
				res = pagefault(addr, F_INVAL, rw, 0);
			} else {
				bzero((caddr_t)&siginfo, sizeof (siginfo));
				siginfo.si_signo = SIGTRAP;
				siginfo.si_code = watchcode;
				siginfo.si_addr = vaddr;
				siginfo.si_trapafter = 0;
				siginfo.si_pc = (caddr_t)rp->r_pc;
				fault = FLTWATCH;
				break;
			}
		} else if (watchpage && rw == S_EXEC) {
			do_watch_step(vaddr, 4, rw, 0, 0);
			res = pagefault(addr, F_INVAL, rw, 0);
		} else if (watchpage) {
			/* XXX never succeeds (for now) */
			if (pr_watch_emul(rp, vaddr, rw))
				goto out;
			do_watch_step(vaddr, 8, rw, 0, 0);
			res = pagefault(addr, F_INVAL, rw, 0);
			break;
		} else if (type == T_TEXT_FAULT + USER) {
			enum fault_type flt_type;

			flt_type = (rp->r_srr1 & SRR1_PROT) ? F_PROT : F_INVAL;
			res = pagefault(addr, flt_type, rw, 0);
		} else {
			enum fault_type flt_type;

			flt_type = (dsisr & DSISR_PROT) ? F_PROT : F_INVAL;
			res = pagefault(addr, flt_type, rw, 0);
		}
		/*
		 * If pagefault() succeeded, ok.
		 * Otherwise attempt to grow the stack.
		 */
		if (res == 0 ||
		    (res == FC_NOMAP &&
		    type != T_TEXT_FAULT + USER &&
		    addr < (caddr_t)USRSTACK &&
		    grow(addr))) {
			lwp->lwp_lastfault = FLTPAGE;
			lwp->lwp_lastfaddr = addr;
			if (prismember(&p->p_fltmask, FLTPAGE)) {
				bzero((caddr_t)&siginfo, sizeof (siginfo));
				siginfo.si_addr = addr;
				(void) stop_on_fault(FLTPAGE, &siginfo);
			}
			goto out;
		}

		if (tudebug)
			showregs(type, rp, addr, dsisr);
		/*
		 * In the case where both pagefault and grow fail,
		 * set the code to the value provided by pagefault.
		 * We map all errors returned from pagefault() to SIGSEGV.
		 */
		bzero((caddr_t)&siginfo, sizeof (siginfo));
		siginfo.si_addr = addr;
		switch (FC_CODE(res)) {
		case FC_HWERR:
		case FC_NOSUPPORT:
			siginfo.si_signo = SIGBUS;
			siginfo.si_code = BUS_ADRERR;
			fault = FLTACCESS;
			break;
		case FC_ALIGN:
			siginfo.si_signo = SIGBUS;
			siginfo.si_code = BUS_ADRALN;
			fault = FLTACCESS;
			break;
		case FC_OBJERR:
			if ((siginfo.si_errno = FC_ERRNO(res)) != EINTR) {
				siginfo.si_signo = SIGBUS;
				siginfo.si_code = BUS_OBJERR;
				fault = FLTACCESS;
			}
			break;
		default:	/* FC_NOMAP or FC_PROT */
			siginfo.si_signo = SIGSEGV;
			siginfo.si_code =
			    (res == FC_NOMAP) ? SEGV_MAPERR : SEGV_ACCERR;
			fault = FLTBOUNDS;
			break;
		}
		break;

	case T_NO_FPU: /* FPU Not Available */
	case T_FP_ASSIST: /* Floating Point Assist trap (PowerPC) */

		/* This shouldn't happen */
		die(type, rp, (caddr_t)0, 0, cpuid);

	case T_FP_ASSIST + USER: /* Floating Point Assist trap (PowerPC) */

		if (tudebug && tudebugfpe)
			showregs(type, rp, (caddr_t)0, 0);

		if (sicode = fp_assist_fault(rp)) {
			bzero((caddr_t)&siginfo, sizeof (siginfo));
			siginfo.si_signo = SIGFPE;
			siginfo.si_code  = sicode;
			siginfo.si_addr  = (caddr_t)rp->r_pc;
			fault = FLTFPE;
		}
		break;

	case T_NO_FPU + USER: /* FPU Not Available */

		if (tudebug && tudebugfpe)
			showregs(type, rp, (caddr_t)0, 0);

		if (sicode = no_fpu_fault(rp)) {
			bzero((caddr_t)&siginfo, sizeof (siginfo));
			siginfo.si_signo = SIGFPE;
			siginfo.si_code  = sicode;
			siginfo.si_addr  = (caddr_t)rp->r_pc;
			fault = FLTFPE;
		}
		break;

	case T_PGM_CHECK + USER:	/* Program Check exception */

		/* Registers SRR0, and SRR1 have the information */

		if (tudebug)
			showregs(type, rp, (caddr_t)0, 0);

		if (rp->r_srr1 & SRR1_SRR0_VALID)
			badaddr = (caddr_t)(rp->r_pc - 4);
		else
			badaddr = (caddr_t)rp->r_pc;

		bzero((caddr_t)&siginfo, sizeof (siginfo));
		switch (rp->r_srr1 & SRR1_PCHK_MASK) {
			case SRR1_ILL_INST:
				/* Illegal Instruction */
				siginfo.si_signo = SIGILL;
				siginfo.si_code  = ILL_ILLOPC;
				siginfo.si_addr  = (caddr_t)badaddr;
				fault = FLTILL;
				break;
			case SRR1_PRIV_INST:
				/* Privileged Instruction */
				siginfo.si_signo = SIGILL;
				siginfo.si_code  = ILL_PRVOPC;
				siginfo.si_addr  = (caddr_t)badaddr;
				fault = FLTILL;
				break;

			case SRR1_TRAP:
				/* get the trap instruction */
				fulword((void *)rp->r_pc, &inst);
				if (inst == BPT_INST) { /* breakpoint trap */
				    siginfo.si_signo = SIGTRAP;
				    siginfo.si_code = TRAP_BRKPT;
				    siginfo.si_addr = (caddr_t)rp->r_pc;
				    fault = FLTBPT;
				    break;
				} else {
				    /* Trap conditional exception */
				    printf("Unrecognized TRAP instruction ");
				    printf("(pc %x instruction %lx)\n",
					rp->r_pc, inst);
				    siginfo.si_signo = SIGILL;
				    siginfo.si_code  = ILL_ILLOPC;
				    siginfo.si_addr  = (caddr_t)badaddr;
				    fault = FLTILL;
				    break;
				}

			case SRR1_FP_EN:
				/* FP Enabled exception */
				if (sicode = fpu_en_fault(rp)) {
					siginfo.si_signo = SIGFPE;
					siginfo.si_code  = sicode;
					siginfo.si_addr  = (caddr_t)rp->r_pc;
					fault = FLTFPE;
					/*
					 * Increment PC for user signal
					 * handler.
					 */
					rp->r_pc += 4;
				}
				break;
		}
		break;

	case T_IO_ERROR:	/* I/O Error exception (on 601 only?) */

		/* Registers SRR0, SRR1, and DAR have the information */

		if (tudebug)
			showregs(type, rp, (caddr_t)0, 0);
		printf("I/O Error in system mode: addr=%p npc=0x%x\n",
			dar, rp->r_srr0);
		goto cleanup;

	case T_IO_ERROR + USER:	/* I/O Error exception (on 601 only?) */

		/* Registers SRR0, SRR1, and DAR have the information */

		if (tudebug)
			showregs(type, rp, (caddr_t)0, 0);
		printf("I/O Error in user mode: dar=%p pc=0x%x msr=0x%x\n",
			dar, rp->r_pc - 4, rp->r_msr & 0xffff);
		break;

	case T_SINGLE_STEP: /* Trace trap (PowerPC) */
	case T_EXEC_MODE: /* Run Mode trap - MPC601 only */

		if (boothowto & RB_DEBUG)
			debug_enter((char *)NULL);
		else
			die(type, rp, (caddr_t)0, 0, cpuid);
		goto cleanup;

	case T_SINGLE_STEP + USER: /* Trace trap (PowerPC) */
	case T_EXEC_MODE + USER: /* Run Mode trap */

		if (tudebug && tudebugbpt)
			showregs(type, rp, (caddr_t)0, 0);

		bzero((caddr_t)&siginfo, sizeof (siginfo));
		if (type == T_SINGLE_STEP + USER || (rp->r_msr & MSR_SE)) {
			pcb_t *pcb = &lwp->lwp_pcb;

			rp->r_msr &= ~MSR_SE;
			if (pcb->pcb_flags & NORMAL_STEP) {
				siginfo.si_signo = SIGTRAP;
				siginfo.si_code = TRAP_TRACE;
				siginfo.si_addr  = (caddr_t)rp->r_pc;
				fault = FLTTRACE;
			}
			if (pcb->pcb_flags & WATCH_STEP)
				fault = undo_watch_step(&siginfo);
			pcb->pcb_flags &= ~(NORMAL_STEP|WATCH_STEP);
		} else {
			siginfo.si_signo = SIGTRAP;
			siginfo.si_code  = TRAP_BRKPT;
			siginfo.si_addr  = (caddr_t)rp->r_pc;
			fault = FLTBPT;
		}
		break;

	case T_AST + USER:		/* profiling or resched psuedo trap */

		if (lwp->lwp_oweupc && p->p_prof.pr_scale) {
			mutex_enter(&p->p_pflock);
			if (p->p_prof.pr_scale) {
				addupc((void(*)())rp->r_pc, &p->p_prof, 1);
			}
			mutex_exit(&p->p_pflock);
			lwp->lwp_oweupc = 0;
		}
		break;
	}

	/*
	 * We can't get here from a system trap
	 */
	ASSERT(type & USER);

	if (fault) {
		/*
		 * Remember the fault and fault adddress
		 * for real-time (SIGPROF) profiling.
		 */
		lwp->lwp_lastfault = fault;
		lwp->lwp_lastfaddr = siginfo.si_addr;
		/*
		 * If a debugger has declared this fault to be an
		 * event of interest, stop the lwp.  Otherwise just
		 * deliver the associated signal.
		 */
		if (siginfo.si_signo != SIGKILL &&
		    prismember(&p->p_fltmask, fault) &&
		    stop_on_fault(fault, &siginfo) == 0)
			siginfo.si_signo = 0;
	}

	if (siginfo.si_signo)
		trapsig(&siginfo, 1);

	if (p->p_prof.pr_scale) {
		int ticks;
		clock_t tv = p->p_stime;

		ticks = tv - syst;

		if (ticks) {
			mutex_enter(&p->p_pflock);
			if (p->p_prof.pr_scale) {
				addupc((void(*)())rp->r_pc, &p->p_prof, ticks);
			}
			mutex_exit(&p->p_pflock);
		}
	}

	if (cur_thread->t_astflag | cur_thread->t_sig_check) {
		/*
		 * Turn off the AST flag before checking all the conditions that
		 * may have caused an AST.  This flag is on whenever a signal or
		 * unusual condition should be handled after the next trap or
		 * syscall.
		 */
		astoff(cur_thread);
		cur_thread->t_sig_check = 0;

		/*
		 * for kaio requests that are on the per-process poll queue,
		 * aiop->aio_pollq, they're AIO_POLL bit is set, the kernel
		 * should copyout their result_t to user memory. by copying
		 * out the result_t, the user can poll on memory waiting
		 * for the kaio request to complete.
		 */
		if (p->p_aio)
			aio_cleanup(0);

		/*
		 * If this LWP was asked to hold, call holdlwp(), which will
		 * stop.  holdlwps() sets this up and calls pokelwps() which
		 * sets the AST flag.
		 *
		 * Also check TP_EXITLWP, since this is used by fresh new LWPs
		 * through lwp_rtt().  That flag is set if the lwp_create(2)
		 * syscall failed after creating the LWP.
		 */
		if (ISHOLD(p))
			holdlwp();

		/*
		 * All code that sets signals and makes ISSIG evaluate true must
		 * set t_astflag afterwards.
		 */
		if (ISSIG_PENDING(cur_thread, lwp, p)) {
			if (issig(FORREAL))
				psig();
			cur_thread->t_sig_check = 1;
		}

		if (cur_thread->t_rprof != NULL) {
			realsigprof(0, 0);
			cur_thread->t_sig_check = 1;
		}
	}

out:	/* We can't get here from a system trap */
	ASSERT(type & USER);

	if (ISHOLD(p))
		holdlwp();

	/*
	 * Set state to LWP_USER here so preempt won't give us a kernel
	 * priority if it occurs after this point.  Call CL_TRAPRET() to
	 * restore the user-level priority.
	 *
	 * It is important that no locks (other than spinlocks) be entered
	 * after this point before returning to user mode (unless lwp_state
	 * is set back to LWP_SYS).
	 */
	lwp->lwp_state = LWP_USER;
	if (cur_thread->t_trapret) {
		cur_thread->t_trapret = 0;
		thread_lock(cur_thread);
		CL_TRAPRET(cur_thread);
		thread_unlock(cur_thread);
	}
	if (CPU->cpu_runrun)
		preempt();
	if (cur_thread->t_proc_flag & TP_MSACCT)
		(void) new_mstate(cur_thread, mstate);

	/* Kernel probe */
	TNF_PROBE_1(thread_state, "thread", /* CSTYLED */,
	    tnf_microstate, state, LMS_USER);

	return;

cleanup:	/* system traps end up here */
	ASSERT(!(type & USER));

#if defined(XXX_OBSOLETE_SOLARIS)
	/*
	 * If the unsafe_driver mutex was held by the thread on entry,
	 * we released it so we could call other drivers.  We re-enter it here.
	 */
	if (driver_mutex)
		mutex_enter(&unsafe_driver);
#endif /* defined(XXX_OBSOLETE_SOLARIS) */
}

/*
 * Patch non-zero to disable preemption of threads in the kernel.
 */
int IGNORE_KERNEL_PREEMPTION = 0; /* XXX - delete this someday */

struct kpreempt_cnts {		/* kernel preemption statistics */
	int	kpc_idle;	/* executing idle thread */
	int	kpc_intr;	/* executing interrupt thread */
	int	kpc_clock;	/* executing clock thread */
	int	kpc_blocked;	/* thread has blocked preemption (t_preempt) */
	int	kpc_notonproc;	/* thread is surrendering processor */
	int	kpc_inswtch;	/* thread has ratified scheduling decision */
	int	kpc_prilevel;	/* processor interrupt level is too high */
	int	kpc_apreempt;	/* asynchronous preemption */
	int	kpc_spreempt;	/* synchronous preemption */
}	kpreempt_cnts;

/*
 * kernel preemption: forced rescheduling, preempt the running kernel thread.
 *	the argument is old PIL for an interrupt,
 *	or the distingished value KPREEMPT_SYNC.
 */
void
kpreempt(int asyncspl)
{
	if (IGNORE_KERNEL_PREEMPTION) {
		aston(CPU->cpu_dispthread);
		return;
	}
	/*
	 * Check that conditions are right for kernel preemption
	 */
	do {
		if (curthread->t_preempt) {
			extern kthread_id_t	clock_thread;
			/*
			 * either a privileged thread (idle, panic, interrupt)
			 *	or will check when t_preempt is lowered
			 */
			if (curthread->t_pri < 0)
				kpreempt_cnts.kpc_idle++;
			else if (curthread->t_flag & T_INTR_THREAD) {
				kpreempt_cnts.kpc_intr++;
				if (curthread == clock_thread)
					kpreempt_cnts.kpc_clock++;
			} else
				kpreempt_cnts.kpc_blocked++;
			aston(CPU->cpu_dispthread);
			return;
		}
		if (curthread->t_state != TS_ONPROC ||
		    curthread->t_disp_queue != CPU->cpu_disp) {
			/* this thread will be calling swtch() shortly */
			kpreempt_cnts.kpc_notonproc++;
			if (CPU->cpu_thread != CPU->cpu_dispthread) {
				/* already in swtch(), force another */
				kpreempt_cnts.kpc_inswtch++;
				siron();
			}
			return;
		}
		if (getpil() >= LOCK_LEVEL) {
			/*
			 * We can't preempt this thread if it is at
			 * a PIL > LOCK_LEVEL since it may be holding
			 * a spin lock (like sched_lock).
			 */
			siron();	/* check back later */
			kpreempt_cnts.kpc_prilevel++;
			return;
		}

		if (asyncspl != KPREEMPT_SYNC)
			kpreempt_cnts.kpc_apreempt++;
		else
			kpreempt_cnts.kpc_spreempt++;

		curthread->t_preempt++;
		preempt();
		curthread->t_preempt--;
	} while (CPU->cpu_kprunrun);
}

// XXX Move traceback() to usr/src/uts/ppc/os/archdep.c

/*
 * Print out a traceback for kernel traps
 */
/* ARGSUSED */
void
traceback(caddr_t sp)
{
}

// XXX Move tracedump() to usr/src/uts/ppc/os/archdep.c
// XXX tracedump does not appear in onnv on any architecture.
// XXX Maybe we should just get rid of it and nuke the declaration
// XXX in usr/src/uts/ppc/sys/stack.h

/*
 * General system stack backtrace
 */
void
tracedump(void)
{
	label_t l;

	(void) setjmp(&l);
	traceback((caddr_t)l.val[1]);
}

/*
 * Print out debugging info.
 */
static void
showregs(uint_t type, struct regs *rp, caddr_t addr, uint_t dsisr)
{
	int s;
	int usermode;

	s = spl7();
	usermode = (type & USER);
	type &= ~USER;
	if (u.u_comm[0])
		printf("%s: ", u.u_comm);
	if (type <= T_EXEC_MODE)
		printf("%s\n", trap_desc(type));
	if (type == T_DATA_FAULT || type == T_TEXT_FAULT) {
		char *str_mode;
		pte_t pte;
		uint_t pte0;
		uint_t pte1;

		str_mode = usermode ? "User" : "Kernel";
		ppcmmu_getpte(addr, &pte);
		pte0 = pte.ptew[PTEWORD0];
		pte1 = pte.ptew[PTEWORD1];
		printf("%s fault at addr=%p, dsisr=0x%x\n",
			str_mode, addr, dsisr);
#if defined(DEBUG)
		printf("    pte=[0]0x%x = ", pte0);
		print_pteu(pte0);
		printf("\n");
		printf("        [1]0x%x = ", pte1);
		print_pteu(pte1);
		printf("\n");
#else
		printf("    pte=[0]0x%x  [1]0x%x\n", pte0, pte1);
#endif
	} else if (addr) {
		printf("addr=%p\n", addr);
	}

	printf("pid=%d, pc=0x%x, sp=0x%x, msr=0x%x\n",
	    (ttoproc(curthread) && ttoproc(curthread)->p_pidp) ?
	    (ttoproc(curthread)->p_pid) : 0, rp->r_pc, rp->r_sp,
	    rp->r_msr);
	printf("gpr0-gpr7:   %x, %x, %x, %x, %x, %x, %x, %x\n",
	    rp->r_r0, rp->r_r1, rp->r_r2, rp->r_r3, rp->r_r4,
	    rp->r_r5, rp->r_r6, rp->r_r7);
	printf("gpr8-gpr15:  %x, %x, %x, %x, %x, %x, %x, %x\n",
	    rp->r_r8, rp->r_r9, rp->r_r10, rp->r_r11, rp->r_r12,
	    rp->r_r13, rp->r_r14, rp->r_r15);
	printf("gpr16-gpr23: %x, %x, %x, %x, %x, %x, %x, %x\n",
	    rp->r_r16, rp->r_r17, rp->r_r18, rp->r_r19, rp->r_r20,
	    rp->r_r21, rp->r_r22, rp->r_r23);
	printf("gpr24-gpr31: %x, %x, %x, %x, %x, %x, %x, %x\n",
	    rp->r_r24, rp->r_r25, rp->r_r26, rp->r_r27, rp->r_r28,
	    rp->r_r29, rp->r_r30, rp->r_r31);
	printf("cr %x, lr %x, ctr %x, xer %x, mq %x\n",
	    rp->r_cr, rp->r_lr, rp->r_ctr, rp->r_xer, rp->r_mq);
	printf("srr1 %x dsisr %x\n", rp->r_srr1, dsisr);

	(void) splx(s);
}

static const char *trap_desc_table[] = {
/* 0x0000 Impossible		*/ "Impossible",
/* 0x0100 T_RESET		*/ "Reset Trap",
/* 0x0200 T_MACH_CHECK		*/ "Machine Check Trap",
/* 0x0300 T_DATA_FAULT		*/ "Data Access Trap",
/* 0x0400 T_TEXT_FAULT		*/ "Instruction Access Trap",
/* 0x0500 T_INTERRUPT		*/ "External Interrupt",
/* 0x0600 T_ALIGNMENT		*/ "Alignment Trap",
/* 0x0700 T_PGM_CHECK		*/ "Program Check Trap",
/* 0x0800 T_NO_FPU		*/ "FPU Not Available",
/* 0x0900 T_DECR		*/ "Decrementer Trap",
/* 0x0A00 T_IO_ERROR		*/ "I/O Error Trap",
/* 0x0B00			*/ "RESERVED",
/* 0x0C00 T_SYSCALL		*/ "System Call Trap",
/* 0x0D00 T_SINGLE_STEP		*/ "Single Step Trap",
/* 0x0E00 T_FP_ASSIST		*/ "Floating-point Assist Trap",
/* 0x0F00 T_PERF_MI		*/ "Performance Monitor Interrupt",
/* 0x1000 T_TLB_IMISS		*/ "TLB Instruction Miss exception",
/* 0x1100 T_TLB_DLOADMISS	*/ "TLB Data Load Miss exception",
/* 0x1200 T_TLB_DSTOREMISS	*/ "TLB Data Store Miss exception",
/* 0x1300 T_IABR		*/ "Instruction Address Breakpoint exception",
/* 0x1400 T_SYS_MGMT		*/ "System Management Exception",
/* 0x1500			*/ "RESERVED",
/* 0x1600 T_AV_ASSIST		*/ "AltiVec Assist Exception",
};

#define	ELEMENTS(array) ((sizeof (array)) / (sizeof (*(array))))

/*
 * Given a trap type (vector address), return trap description.
 */
static const char *
trap_desc(uint_t type)
{
	const char *desc;

	/*
	 * Handle the vast majority of cases by table lookup.
	 */
	if ((type & ~0xFF00) == 0) {
		uint_t tndx;

		tndx = type >> 8;
		if (tndx < ELEMENTS(trap_desc_table))
			desc = trap_desc_table[tndx];
		else if (type < 0x2FFF)
			desc = "RESERVED";
		else
			desc = "INVALID";
		return (desc);
	}

	/*
	 * Handle the vector addresses that do not easily
	 * fit into the pattern of a dense, regular table.
	 */
	switch (type) {
	case T_NO_AV:
		desc = "AltiVec Unavailable Exception";
		break;
	case T_EXEC_MODE:
		desc = "Run Mode Exception";
		break;
	case T_AST:
		desc = "AST";
		break;
	default:
		desc = "Unknown Trap";
	}
	return (desc);
}
