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

#pragma ident "@(#)fpu.c 1.9	96/04/01 SMI"

/*
 * PowerPC Floating-Point support routines.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/reg.h>
#include <sys/psw.h>
#include <sys/systm.h>
#include <sys/file.h>
#include <sys/pcb.h>
#include <sys/lwp.h>
#include <sys/cpuvar.h>
#include <sys/thread.h>
#include <sys/fpu/fpusystm.h>
#include <sys/debug.h>

#define	FP_INV_STICKY_BITS 0x01F80700	/* sticky bits for invalid exceptions */

/*
 * Copy the floating point context of the forked thread.
 *
 */
void
fp_fork(kthread_id_t t, kthread_id_t ct)
{
	klwp_id_t clwp = ct->t_lwp;
	pcb_t *cpcb, *pcb;
	struct fpu *cfp;	/* child's fpu context */
	struct fpu *fp;		/* parent's fpu context */

	pcb = &t->t_lwp->lwp_pcb;
	fp = &pcb->pcb_fpu;
	cpcb = &clwp->lwp_pcb;
	cfp = &cpcb->pcb_fpu;

	/*
	 * If the parent FPU state is still associated with FPU hw then
	 * get a copy of it for the child.
	 */
	if (!fp->fpu_valid)
		fp_save(cfp);
	else
		*cfp = *fp; /* copy it to the child */
	installctx(ct, cfp, fp_save, fp_restore, fp_fork, fp_new_lwp,
		fp_exit, fp_free);
	cpcb->pcb_flags |= PCB_FPU_INITIALIZED;
}

void
fp_new_lwp(struct fpu *fp)
{
}

void
fp_exit(struct fpu *fp)
{
}

/*
 * Free any state associated with floating point context.
 * Fp_free can be called in three cases:
 * 1) from reaper -> thread_free -> ctxfree -> fp_free
 *	fp context belongs to a thread on deathrow
 *	nothing to do,  thread will never be resumed
 *	thread calling ctxfree is reaper
 *
 * 2) from exec -> ctxfree -> fp_free
 *	fp context belongs to the current thread
 *	must disable fpu, thread calling ctxfree is curthread
 *
 * 3) from restorecontext -> setfpregs -> fp_free
 *	we have a modified context in the memory (lwp->pcb_fpu)
 *	disable fpu and release the fp context for the CPU
 *
 */
void
fp_free(struct fpu *fp)
{
	kpreempt_disable();
	if (&curthread->t_lwp->lwp_pcb.pcb_fpu == fp) {
		struct regs *rp;

		rp = lwptoregs(ttolwp(curthread));
		rp->r_msr &= ~MSR_FP;
	}
	kpreempt_enable();
}

/*
 * fp_save(struct fpu *fp)
 *
 * Store the floating point state and disable the floating point unit.
 *
 */

/* ARGSUSED */
void
fp_save(struct fpu *fp)
{
	kpreempt_disable();
	if (!fp || fp->fpu_valid) {
		kpreempt_enable();
		return;
	}
	fpu_save(fp); 				/* save FPU context */
	kpreempt_enable();
}

/*
 * fp_restore(struct fpu *fp)
 *
 * Restore the FPU context for the current thread.
 *
 * UP:	If the FP hw still has the context for the thread then just enable
 *	the FPU, mark the memory copy as invalid and return. Otherwise let
 *	the thread take the no-fpu-available trap if it needs the context.
 * MP:	Disable FPU for the thread and the context will be restored thru
 *	no-fap-available trap.
 *
 */

/* ARGSUSED */
void
fp_restore(struct fpu *fp)
{
	struct regs *rp;

	kpreempt_disable();
#ifndef MP
	if (CPU->cpu_fpowner == ttolwp(curthread)) {
		rp = lwptoregs(ttolwp(curthread));
		rp->r_msr |= MSR_FP;
		fp->fpu_valid = 0;
		if (ttolwp(curthread)->lwp_pcb.pcb_flags &
			PCB_FPU_STATE_MODIFIED) {
			fpu_restore(fp); /* load the modified FPU context */
			ttolwp(curthread)->lwp_pcb.pcb_flags &=
				~PCB_FPU_STATE_MODIFIED;
		}
		kpreempt_enable();
		return;
	}
#endif
	rp = lwptoregs(ttolwp(curthread));
#if 0
	fpu_restore(fp); 	/* load the new FPU context */
	CPU->cpu_fpowner = ttolwp(curthread);
	rp->r_msr |= MSR_FP;
#else
	/*
	 * turnoff the FP for this thread and let it take the no-fpu-available
	 * fault to get the context.
	 */
	rp->r_msr &= ~MSR_FP;
	ttolwp(curthread)->lwp_pcb.pcb_flags &= ~PCB_FPU_STATE_MODIFIED;
#endif
	kpreempt_enable();
}

/*
 * no_fpu_fault(struct regs *rp)
 *
 * This routine is invoked from trap() when a user thread attempts
 * to execute a floating-point instruction, and MSR[FP] is disabled;
 * i.e., a Floating-Point Unavailable exception occurs.
 *
 * Possible scenarios are:
 *	1. User thread has executed a FP instruction for the first time.
 *	   Save current FPU context if any. Initialize FP hw state and FP
 *	   context for the thread and enable FPU.
 *	2. A user thread resuming execution has a valid pre-existing FPU
 *	   context. Restore FP context and enable FPU.
 *
 */
/* ARGSUSED */
int
no_fpu_fault(struct regs *rp)
{
	klwp_id_t lwp = ttolwp(curthread);
	pcb_t *pcb = &lwp->lwp_pcb;
	struct fpu *fp = &pcb->pcb_fpu;

	kpreempt_disable();

	if (!fpu_exists) { /* check for FPU hw exists */
		/*
		 * If we have neither a processor extension nor
		 * an emulator, kill the process OR panic the kernel.
		 */
		kpreempt_enable();
		return (-1); /* error */
	}

	if (pcb->pcb_flags & PCB_FPU_INITIALIZED) { /* case 2 */
		ASSERT(fp->fpu_valid);
		fpu_restore(fp); /* load the saved FP context */
	} else {			/* case 1 */
		installctx(curthread, fp, fp_save, fp_restore,
				fp_fork, fp_new_lwp, fp_exit, fp_free);
		fpu_hw_init(); 	/* initialize FPU hw */
		pcb->pcb_flags |= PCB_FPU_INITIALIZED;
		fp->fpu_valid = 0;
	}
	rp->r_msr |= MSR_FP;
	CPU->cpu_fpowner = lwp;
	kpreempt_enable();
	return (0);
}


/*
 * fp_assist_fault(struct regs *rp)
 *
 * XXXPPC
 * This routine is currently a stub, because the Floating-Point Assist
 * exception is optional to the PowerPC architecture, and is not implemented
 * on the MPC601.  This type of exception is used to provide software
 * assistance for some floating-point instructions that may be required
 * but not implemented, or partially implemented, but need software
 * assistance for certain operations, such as denormalized numbers.
 *
 * The MPC601 haldles all floating-point data types, and so does not
 * use this particular exception.
 *
 */
/* ARGSUSED */
int
fp_assist_fault(struct regs *rp)
{
	return (0);
}

/*
 * fpu_en_fault(struct regs *rp)
 *
 * Return the si_code corresponding to the FP exception from FPU status word.
 */
/* ARGSUSED */
int
fpu_en_fault(struct regs *rp)
{
	klwp_id_t lwp = ttolwp(curthread);
	pcb_t *pcb = &lwp->lwp_pcb;
	struct fpu *fp = &pcb->pcb_fpu;
	uint_t fpscr;
	int ret = FPE_FLTINV;

	kpreempt_disable();
	fpscr = fperr_reset(); /* reset the exception enable bits in the hw */
	kpreempt_enable();
	if (!fp->fpu_valid)
		fp_save(fp);
	fp->fpu_fpscr =  fpscr;
	pcb->pcb_flags |= PCB_FPU_STATE_MODIFIED;
	/*
	 * determine the current exception type
	 * and clear all trapped exceptions
	 */
	if (fpscr & FP_ENAB_EX_SUMM) {
		if (fpscr & FP_INEXACT_ENAB) {
			if (fpscr & FP_INEXACT)
				ret = FPE_FLTRES;
			fp->fpu_fpscr &= ~FP_INEXACT;
		}
		if (fpscr & FP_UNDRFLW_ENAB) {
			if (fpscr & FP_UNDERFLOW)
				ret = FPE_FLTUND;
			fp->fpu_fpscr &= ~FP_UNDERFLOW;
		}
		if (fpscr & FP_OVRFLW_ENAB) {
			if (fpscr & FP_OVERFLOW)
				ret = FPE_FLTOVF;
			fp->fpu_fpscr &= ~FP_OVERFLOW;
		}
		if (fpscr & FP_ZERODIV_ENAB) {
			if (fpscr & FP_ZERODIVIDE)
				ret = FPE_FLTDIV;
			fp->fpu_fpscr &= ~FP_ZERODIVIDE;
		}
		if (fpscr & FP_INV_OP_ENAB) {
			if (fpscr & FP_INV_OP_EX_SUMM)
				ret = FPE_FLTINV;
			fp->fpu_fpscr &= ~FP_INV_STICKY_BITS;
		}
	}
	return (ret);
}
