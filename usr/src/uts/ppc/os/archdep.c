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

#pragma ident	"@(#)archdep.c	1.54	96/06/18 SMI"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/vmparam.h>
#include <sys/systm.h>
#include <sys/signal.h>
#include <sys/stack.h>
#include <sys/reg.h>
#include <sys/frame.h>
#include <sys/proc.h>
#include <sys/ucontext.h>
#include <sys/siginfo.h>
#include <sys/cpuvar.h>
#include <sys/asm_linkage.h>
#include <sys/kmem.h>
#include <sys/errno.h>
#include <sys/callo.h>
#include <sys/bootconf.h>
#include <sys/archsystm.h>
#include <sys/fpu/fpusystm.h>
#include <sys/debug.h>
#include <sys/elf.h>
#include <sys/psw.h>
#include <sys/spl.h>
#include <sys/panic.h>

extern struct bootops *bootops;
extern struct fpu initial_fpu_state;

/*
 * Set floating-point registers.
 */
void
setfpregs(
	register klwp_id_t lwp,
	register fpregset_t *fp)
{

	register struct pcb *pcb;
	register fpregset_t *pfp;
	register struct regs *reg;


	if (fp->fpu_valid) { /* User specified a valid FP state */
		pcb = &lwp->lwp_pcb;
		pfp = &pcb->pcb_fpu;
		if (!(pcb->pcb_flags & PCB_FPU_INITIALIZED)) {
			/*
			 * This lwp didn't have a FP context setup before,
			 * do it now and copy the user specified state into
			 * pcb.
			 */
#if 0
			installctx(lwptot(lwp), pfp, fp_save, fp_restore,
				fp_fork, fp_free);
#endif
			pcb->pcb_flags |= PCB_FPU_INITIALIZED;
		} else if (!(pcb->pcb_fpu.fpu_valid)) {
			/*
			 * FPU context is still active, release the
			 * ownership.
			 */
			fp_free(&pcb->pcb_fpu);
		}
		kcopy((caddr_t)fp, (caddr_t)pfp, sizeof (struct fpu));
		pfp->fpu_valid = 1;
		pcb->pcb_flags |= PCB_FPU_STATE_MODIFIED;
		reg = lwptoregs(lwp);
		reg->r_msr &= ~MSR_FP; /* disable FP */
	}
}

/*
 * Get floating-point registers.  The u-block is mapped in here (not by
 * the caller).
 */
void
getfpregs(
	register klwp_id_t lwp,
	register fpregset_t *fp)
{
	register struct pcb *pcb;

	pcb = &lwp->lwp_pcb;

	if (pcb->pcb_flags & PCB_FPU_INITIALIZED) {
		kpreempt_disable();
		/*
		 * If we have FPU hw and the thread's pcb doesn't have
		 * a valid FPU state then get the state from the hw.
		 */
		if (!(pcb->pcb_fpu.fpu_valid)) {
			fp_save(&pcb->pcb_fpu); /* get the current FPU state */
		}
		kpreempt_enable();
		kcopy((caddr_t)&pcb->pcb_fpu, (caddr_t)fp, sizeof (struct fpu));
	} else {
		/*
		 * This lwp doesn't have a FP context setup so we just
		 * initialize the user buffer with initial state which is
		 * same as fpu_hw_init().
		 */
		kcopy((caddr_t)&initial_fpu_state, (caddr_t)fp,
			sizeof (struct fpu));
	}
}

/*
 * Return the general registers
 */
void
getgregs(lwp, rp)
	register klwp_id_t lwp;
	register gregset_t rp;
{
	bcopy((caddr_t)lwp->lwp_regs, (caddr_t)rp, sizeof (gregset_t));
}

/*
 * Return the user-level PC.
 * If in a system call, return the address of the syscall trap.
 */
greg_t
getuserpc()
{
	greg_t pc = lwptoregs(ttolwp(curthread))->r_pc;
	if (curthread->t_sysnum)
		pc -= 4;	/* size of an sc instruction */
	return (pc);
}

/*
 * Set general registers.
 */
void
setgregs(lwp, rp)
	register klwp_id_t lwp;
	register gregset_t rp;
{
	register greg_t *reg;
	register greg_t old_msr;

	reg = (greg_t *)lwp->lwp_regs;

	/*
	 * Except MSR register other registers are user modifiable.
	 */
	old_msr = reg[R_MSR]; /* save the old MSR */

	/* copy saved registers from user stack */
	bcopy((caddr_t)rp, (caddr_t)reg, sizeof (gregset_t));

	reg[R_MSR] = old_msr; /* restore the MSR */

	if (lwptot(lwp) == curthread) {
		lwp->lwp_eosys = JUSTRETURN;	/* so cond reg. wont change */
		curthread->t_post_sys = 1;	/* so regs will be restored */
	}
}

/*
 * Get a pc-only stacktrace.  Used for kmem_alloc() buffer ownership tracking.
 * Returns MIN(current stack depth, pcstack_limit).
 */
int
getpcstack(pc_t *pcstack, int pcstack_limit)
{
	minframe_t *stacktop = (minframe_t *)curthread->t_stk;
	minframe_t *prevfp = (minframe_t *)KERNELBASE;
	minframe_t *fp = (minframe_t *)getfp();
	int depth = 0;

	while (fp > prevfp && fp < stacktop && depth < pcstack_limit) {
		pcstack[depth++] = (uint_t)fp->fr_savpc;
		prevfp = fp;
		fp = (minframe_t *)fp->fr_savfp;
	}
	return (depth);
}

/*
 *	sync_icache() - this is called
 *	in proc/fs/prusrio.c.
 */
/* ARGSUSED */
void
sync_icache(caddr_t addr, uint_t len)
{
	sync_instruction_memory(addr, len);
}

/*ARGSUSED*/
void
sync_data_memory(caddr_t va, size_t len)
{
	/* Not implemented for this platform */
}

int
__ipltospl(int ipl)
{
	return (ipltospl(ipl));
}

/*
 * The panic code invokes panic_saveregs() to record the contents of a
 * regs structure into the specified panic_data structure for debuggers.
 */
void
panic_saveregs(panic_data_t *pdp, struct regs *rp)
{
	panic_nv_t *pnv = PANICNVGET(pdp);

#if 0
	struct cregs	creg;

	getcregs(&creg);
#endif

#if defined(__amd64)
	PANICNVADD(pnv, "rdi", rp->r_rdi);
	PANICNVADD(pnv, "rsi", rp->r_rsi);
	PANICNVADD(pnv, "rdx", rp->r_rdx);
	PANICNVADD(pnv, "rcx", rp->r_rcx);
	PANICNVADD(pnv, "r8", rp->r_r8);
	PANICNVADD(pnv, "r9", rp->r_r9);
	PANICNVADD(pnv, "rax", rp->r_rax);
	PANICNVADD(pnv, "rbx", rp->r_rbx);
	PANICNVADD(pnv, "rbp", rp->r_rbp);
	PANICNVADD(pnv, "r10", rp->r_r10);
	PANICNVADD(pnv, "r10", rp->r_r10);
	PANICNVADD(pnv, "r11", rp->r_r11);
	PANICNVADD(pnv, "r12", rp->r_r12);
	PANICNVADD(pnv, "r13", rp->r_r13);
	PANICNVADD(pnv, "r14", rp->r_r14);
	PANICNVADD(pnv, "r15", rp->r_r15);
	PANICNVADD(pnv, "fsbase", rp->r_fsbase);
	PANICNVADD(pnv, "gsbase", rp->r_gsbase);
	PANICNVADD(pnv, "ds", rp->r_ds);
	PANICNVADD(pnv, "es", rp->r_es);
	PANICNVADD(pnv, "fs", rp->r_fs);
	PANICNVADD(pnv, "gs", rp->r_gs);
	PANICNVADD(pnv, "trapno", rp->r_trapno);
	PANICNVADD(pnv, "err", rp->r_err);
	PANICNVADD(pnv, "rip", rp->r_rip);
	PANICNVADD(pnv, "cs", rp->r_cs);
	PANICNVADD(pnv, "rflags", rp->r_rfl);
	PANICNVADD(pnv, "rsp", rp->r_rsp);
	PANICNVADD(pnv, "ss", rp->r_ss);
	PANICNVADD(pnv, "gdt_hi", (uint64_t)(creg.cr_gdt._l[3]));
	PANICNVADD(pnv, "gdt_lo", (uint64_t)(creg.cr_gdt._l[0]));
	PANICNVADD(pnv, "idt_hi", (uint64_t)(creg.cr_idt._l[3]));
	PANICNVADD(pnv, "idt_lo", (uint64_t)(creg.cr_idt._l[0]));
#elif defined(__i386)
	PANICNVADD(pnv, "gs", (uint32_t)rp->r_gs);
	PANICNVADD(pnv, "fs", (uint32_t)rp->r_fs);
	PANICNVADD(pnv, "es", (uint32_t)rp->r_es);
	PANICNVADD(pnv, "ds", (uint32_t)rp->r_ds);
	PANICNVADD(pnv, "edi", (uint32_t)rp->r_edi);
	PANICNVADD(pnv, "esi", (uint32_t)rp->r_esi);
	PANICNVADD(pnv, "ebp", (uint32_t)rp->r_ebp);
	PANICNVADD(pnv, "esp", (uint32_t)rp->r_esp);
	PANICNVADD(pnv, "ebx", (uint32_t)rp->r_ebx);
	PANICNVADD(pnv, "edx", (uint32_t)rp->r_edx);
	PANICNVADD(pnv, "ecx", (uint32_t)rp->r_ecx);
	PANICNVADD(pnv, "eax", (uint32_t)rp->r_eax);
	PANICNVADD(pnv, "trapno", (uint32_t)rp->r_trapno);
	PANICNVADD(pnv, "err", (uint32_t)rp->r_err);
	PANICNVADD(pnv, "eip", (uint32_t)rp->r_eip);
	PANICNVADD(pnv, "cs", (uint32_t)rp->r_cs);
	PANICNVADD(pnv, "eflags", (uint32_t)rp->r_efl);
	PANICNVADD(pnv, "uesp", (uint32_t)rp->r_uesp);
	PANICNVADD(pnv, "ss", (uint32_t)rp->r_ss);
	PANICNVADD(pnv, "gdt", creg.cr_gdt);
	PANICNVADD(pnv, "idt", creg.cr_idt);
#endif	/* __i386 */
}

/*
 * Print a stack backtrace using the specified frame pointer.  We delay two
 * seconds before continuing, unless this is the panic traceback.  Note
 * that the frame for the starting stack pointer value is omitted because
 * the corresponding %eip is not known.
 */

void
traceback(caddr_t fpreg)
{
}

#if defined(__amd64)

void
traceback(caddr_t fpreg)
{
	struct frame	*fp = (struct frame *)fpreg;
	struct frame	*nextfp;
	uintptr_t	pc, nextpc;
	ulong_t		off;
	char		args[TR_ARG_MAX * 2 + 16], *sym;

	if (!panicstr)
		printf("traceback: %%fp = %p\n", (void *)fp);

	if ((uintptr_t)fp < KERNELBASE)
		goto out;

	pc = fp->fr_savpc;
	fp = (struct frame *)fp->fr_savfp;

	while ((uintptr_t)fp >= KERNELBASE) {
		/*
		 * XX64 Until port is complete tolerate 8-byte aligned
		 * frame pointers but flag with a warning so they can
		 * be fixed.
		 */
		if (((uintptr_t)fp & (STACK_ALIGN - 1)) != 0) {
			if (((uintptr_t)fp & (8 - 1)) == 0) {
				printf("  >> warning! 8-byte"
				    " aligned %%fp = %p\n", (void *)fp);
			} else {
				printf(
				    "  >> mis-aligned %%fp = %p\n", (void *)fp);
				break;
			}
		}

		args[0] = '\0';
		nextpc = (uintptr_t)fp->fr_savpc;
		nextfp = (struct frame *)fp->fr_savfp;
		if ((sym = kobj_getsymname(pc, &off)) != NULL) {
			printf("%016lx %s:%s+%lx (%s)\n", (uintptr_t)fp,
			    mod_containing_pc((caddr_t)pc), sym, off, args);
		} else {
			printf("%016lx %lx (%s)\n",
			    (uintptr_t)fp, pc, args);
		}

		pc = nextpc;
		fp = nextfp;
	}
out:
	if (!panicstr) {
		printf("end of traceback\n");
		DELAY(2 * MICROSEC);
	}
}

#elif defined(__i386)

void
traceback(caddr_t fpreg)
{
	struct frame *fp = (struct frame *)fpreg;
	struct frame *nextfp, *minfp, *stacktop;
	uintptr_t pc, nextpc;

	cpu_t *cpu;

	/*
	 * args[] holds TR_ARG_MAX hex long args, plus ", " or '\0'.
	 */
	char args[TR_ARG_MAX * 2 + 8], *p;

	int on_intr;
	ulong_t off;
	char *sym;

	if (!panicstr)
		printf("traceback: %%fp = %p\n", (void *)fp);

	/*
	 * If we are panicking, all high-level interrupt information in
	 * CPU was overwritten.  panic_cpu has the correct values.
	 */
	kpreempt_disable();			/* prevent migration */

	cpu = (panicstr && CPU->cpu_id == panic_cpu.cpu_id)? &panic_cpu : CPU;

	if ((on_intr = CPU_ON_INTR(cpu)) != 0)
		stacktop = (struct frame *)(cpu->cpu_intr_stack + SA(MINFRAME));
	else
		stacktop = (struct frame *)curthread->t_stk;

	kpreempt_enable();

	if ((uintptr_t)fp < KERNELBASE)
		goto out;

	minfp = fp;	/* Baseline minimum frame pointer */
	pc = fp->fr_savpc;
	fp = (struct frame *)fp->fr_savfp;

	while ((uintptr_t)fp >= KERNELBASE) {
		ulong_t argc;
		long *argv;

		if (fp <= minfp || fp >= stacktop) {
			if (on_intr) {
				/*
				 * Hop from interrupt stack to thread stack.
				 */
				stacktop = (struct frame *)curthread->t_stk;
				minfp = (struct frame *)curthread->t_stkbase;
				on_intr = 0;
				continue;
			}
			break; /* we're outside of the expected range */
		}

		if ((uintptr_t)fp & (STACK_ALIGN - 1)) {
			printf("  >> mis-aligned %%fp = %p\n", (void *)fp);
			break;
		}

		nextpc = fp->fr_savpc;
		nextfp = (struct frame *)fp->fr_savfp;
		argc = argcount(nextpc);
		argv = (long *)((char *)fp + sizeof (struct frame));

		args[0] = '\0';
		p = args;
		while (argc-- > 0 && argv < (long *)stacktop) {
			p += snprintf(p, args + sizeof (args) - p,
			    "%s%lx", (p == args) ? "" : ", ", *argv++);
		}

		if ((sym = kobj_getsymname(pc, &off)) != NULL) {
			printf("%08lx %s:%s+%lx (%s)\n", (uintptr_t)fp,
			    mod_containing_pc((caddr_t)pc), sym, off, args);
		} else {
			printf("%08lx %lx (%s)\n",
			    (uintptr_t)fp, pc, args);
		}

		minfp = fp;
		pc = nextpc;
		fp = nextfp;
	}
out:
	if (!panicstr) {
		printf("end of traceback\n");
		DELAY(2 * MICROSEC);
	}
}

#endif	/* __i386 */

/*
 * Generate a stack backtrace from a saved register set.
 */
void
traceregs(struct regs *rp)
{
	minframe_t *sp = (minframe_t *)rp->r_sp;

	traceback((caddr_t)sp->fr_savfp);
}

void
exec_set_sp(size_t stksize)
{
	klwp_t *lwp = ttolwp(curthread);

	lwptoregs(lwp)->r_sp = (uintptr_t)curproc->p_usrstack - stksize;
}

hrtime_t
gethrtime_waitfree(void)
{
	return (dtrace_gethrtime());
}

/*
 * x86 resolves gethrtime to a platform-specific function
 * by calling though a function pointer that gets set by
 * platform-specific initialization code.  On PowerPC, there
 * does not seem to be a need to do this, so gethrtime is
 * implemented directly in locore.s.
 */
#if 0
hrtime_t
gethrtime(void)
{
	return (gethrtimef());
}
#endif

hrtime_t
gethrtime_unscaled(void)
{
#if defined(__i386)
	return (gethrtimeunscaledf());
#else
	return (0);
#endif
}

void
scalehrtime(hrtime_t *hrt)
{
#if defined(__i386)
	scalehrtimef(hrt);
#endif
}

void
gethrestime(timespec_t *tp)
{
#if defined(__i386)
	gethrestimef(tp);
#endif
}
