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

#ifndef _SYS_PPC_SUBR_H
#define	_SYS_PPC_SUBR_H

#pragma ident	"%Z%%M%	%I%	%E% SMI"

/*
 * General inline functions for PowerPC.
 * On other platforms, these have not been inlined;
 * they are assembly language subroutines.
 *
 * For Solaris/PPC, we decided to go ahead and take
 * advantage of the superior inlining capabilities
 * of the GNU C compiler.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/psw.h>
#include <sys/types.h>
#include <sys/machthread.h>
#include <sys/thread.h>
#if (defined(_KERNEL) || defined(_KMEMUSER)) && defined(_MACHDEP)
#include <sys/machcpuvar.h>
#endif
#include <sys/frame.h>
#include <sys/cpuvar.h>
#include <sys/reg.h>
#include <sys/ppc_instr.h>
#include <sys/msr.h>

/*
 * return the current program counter
 */
static __inline__ greg_t
getpc(void)
{
	greg_t self;
	greg_t save_lr;

	__asm__(
	"	mflr   %[save_lr]\n"
	"	bl	0f\n"
	"0:\n"
	"	mflr	%[self]\n"
	"	mtlr	%[save_lr]\n"
	: [save_lr] "=r" (save_lr), [self] "=r" (self)
	: /* No inputs */
	/* */);
	return (self);
}

/*
 * return the current stack pointer
 */
static __inline__ greg_t
getsp(void)
{
	register greg_t val __asm__("r1");

	return (val);
}

/*
 * return the current frame pointer
 */

static __inline__ greg_t
getfp(void)
{
	minframe_t *sp = (minframe_t *)getsp();

	return ((greg_t)sp->fr_savfp);
}

/*
 * if a() calls b() calls caller(),
 * caller() returns return address in a().
 * NOTE: We assume a() and b() are C routines
 * that do the normal entry/exit sequence.
 */

static __inline__ caddr_t
caller(void)
{
	minframe_t *sp = (minframe_t *)getsp();

	return ((caddr_t)sp->fr_savpc);
}

/*
 * if a() calls b() calls c() calls caller2(),
 * caller2() returns return address in a().
 * NOTE: We assume a(),b() and c() are C routines
 * that do the normal entry/exit sequence.
 */

static __inline__ caddr_t
caller2(void)
{
	minframe_t *sp = (minframe_t *)getsp();
	minframe_t *back = (minframe_t *)(sp->fr_savfp);

	return ((caddr_t)back->fr_savpc);
}

/*
 * if a() calls b() calls c() calls d() calls caller3(),
 * caller3() returns return address in a().
 * NOTE: We assume a(), b(), c() and d() are C routines
 * that do the normal entry/exit sequence.
 */

static __inline__ caddr_t
caller3(void)
{
	minframe_t *sp = (minframe_t *)getsp();
	minframe_t *back = (minframe_t *)(sp->fr_savfp);
	minframe_t *back2 = (minframe_t *)(back->fr_savfp);

	return ((caddr_t)back2->fr_savpc);
}

/*
 * if a() calls callee(), callee() returns the
 * return address in a();
 */

static __inline__ caddr_t
callee(void)
{
	caddr_t val;

	__asm__(
	"	mflr   %[val]"
	: [val] "=r" (val)
	: /* No inputs */
	/* */);
	return (val);
}

static __inline__ uint_t
clear_int_flag(void)
{
	uint_t msr_old, msr_dis;

	msr_old = mfmsr();
	msr_dis = MSR_SET_EE(msr_old, 0);	// clear msr.EE
	mtmsr(msr_dis);				// disable interrupts
	return (msr_old);			// return old msr
}

/*
 *	Enable interrupts, the first time.
 */

static __inline__  uint_t
enable_interrupts(void)
{
	uint_t msr_old, msr_dis;

	msr_old = mfmsr();
	msr_dis = MSR_SET_EE(msr_old, 1);	// set msr.EE
	mtmsr(msr_dis);				// disable interrupts
	return (msr_old);			// return old msr
}

// XXX Should not be needed, anymore; directly use mtmsr.
//
static __inline__ void
restore_int_flag(uint_t msr_val)
{
	mtmsr(msr_val);
}

extern int catch_fault(void);
extern int on_fault(label_t *);
extern int setjmp(label_t *);
extern void longjmp(label_t *);

/*
 * no_fault()
 * turn off fault catching.
 */

static __inline__ void
no_fault(void)
{
	kthread_t *cur_thread = curthread;

	cur_thread->t_onfault = NULL;
	cur_thread->t_lofault = NULL;
}

#if (defined(_KERNEL) || defined(_KMEMUSER)) && defined(_MACHDEP)

/*
 * Get current processor interrupt level
 */
static __inline__ greg_t
getpil(void)
{
	kthread_t *cur_thread = curthread;

	return (cur_thread->t_cpu->cpu_m.mcpu_pri);
}

#else

extern greg_t getpil(void);

#endif

/*
 * SPL routines
 * ------------
 * Berkley 4.3 introduced symbolically named interrupt levels
 * as a way deal with priority in a machine independent fashion.
 * Numbered priorities are machine specific, and should be
 * discouraged where possible.
 *
 * Note, for the machine specific priorities there are
 * examples listed for devices that use a particular priority.
 * It should not be construed that all devices of that
 * type should be at that priority.  It is currently were
 * the current devices fit into the priority scheme based
 * upon time criticalness.
 *
 * The underlying assumption of these assignments is that
 * IPL 10 is the highest level from which a device
 * routine can call wakeup.  Devices that interrupt from higher
 * levels are restricted in what they can do.  If they need
 * kernels services they should schedule a routine at a lower
 * level (via software interrupt) to do the required
 * processing.
 *
 * Examples of this higher usage:
 *	Level	Usage
 *	14	Profiling clock (and PROM uart polling clock)
 *	12	Serial ports
 *
 * The serial ports request lower level processing on level 6.
 *
 * Also, almost all splN routines (where N is a number or a
 * mnemonic) will do a RAISE(), on the assumption that they are
 * never used to lower our priority.
 * The exceptions are:
 *	spl8()		Because you can't be above 15 to begin with!
 *	splzs()		Because this is used at boot time to lower our
 *			priority, to allow the PROM to poll the uart.
 *	spl0()		Used to lower priority to 0.
 *	splsoftclock()	Used by hardclock to lower priority.
 *
 * Underlying functions
 * --------------------
 * splr - Raise processor priority level.
 *   Avoid dropping processor priority if already at high level.
 *   Also avoid going below CPU->cpu_base_spl, which could've
 *   just been set by a higher-level interrupt thread
 *   that just blocked.
 *
 * splx - Disable interrupts, then spl().
 *        NOTE: On PowerPC, spl() disables interrupts, anyway.
 *              So splx() and spl() are the same.
 *		This is not so on all processors.
 *
 * spl - Set the priority to a specified level.
 *   Avoid dropping the priority below CPU->cpu_base_spl.
 */

/*
 * splr() and splx() are declared in usr/src/uts/common/sys/systm.h.
 * spl() is not declared.  Many non-machdep source files feel free to
 * declare variables named 'spl' and never try to call the function spl(),
 * but they do call the function, splx().
 *
 * We avoid possible conflicts by using the underlying function,
 * spl_impl(), so the identifier 'spl' is up for grabs.
 */

#define	splx(new_level) spl_impl(new_level)

extern int spl_impl(int);

static __inline__ int
splr(int new_level)
{
	kthread_t *cur_thread;
	int cur_level;

	cur_thread = curthread;
	cur_level = getpil();
	if (cur_level >= new_level)
		return (cur_level);
	return (spl_impl(new_level));
}

/* locks out all interrupts, including memory errors */
static __inline__ int
spl8(void)
{
	return (splr(15));
}

/* just below the level that profiling runs */
static __inline__ int
spl7(void)
{
	return (splr(13));
}

/* sun specific - highest priority onboard serial I/O asy ports */
static __inline__ int
splzs(void)
{
	/* Can't be splr(), as it's used to lower us */
	return (spl_impl(12));
}

/*
 * Should lock out clocks and all interrupts.
 * As you can see, there are exceptions.
 */
static __inline__ int
splhigh(void)
{
	return (splr(10));
}

static __inline__ int
splhi(void)
{
	return (splr(10));
}

/* the standard clock interrupt priority */
static __inline__ int
splclock(void)
{
	return (splr(10));
}

/* highest priority for any tty handling */
static __inline__ int
spltty(void)
{
	return (splr(10));
}

/* highest priority required for protection of buffered IO system */
static __inline__ int
splbio(void)
{
	return (splr(10));
}

/* machine specific */
static __inline__ int
spl6(void)
{
	return (splr(10));
}

/* machine specific */
static __inline__ int
spl5(void)
{
	return (splr(10));
}

/*
 * machine specific for Sun.
 * Some frame buffers must be at this priority.
 */
static __inline__ int
spl4(void)
{
	return (splr(8));
}

/* highest level that any network device will use */
static __inline__ int
splimp(void)
{
	return (splr(6));
}

/*
 * machine specific for Sun.
 * Devices with limited buffering: tapes, ethernet
 */
static __inline__ int
spl3(void)
{
	return (splr(6));
}

/*
 * Machine specific - not as time critical as above.
 * For sun, disks
 */
static __inline__ int
spl2(void)
{
	return (splr(4));
}

static __inline__ int
spl1(void)
{
	return (splr(2));
}

/* highest level that any protocol handler will run */
static __inline__ int
splnet(void)
{
	return (splr(1));
}

/*
 * Softcall priority.
 * Used by hardclock to LOWER priority.
 */
static __inline__ int
splsoftclock(void)
{
	return (spl_impl(1));
}

/* Allow all interrupts */
static __inline__ int
spl0(void)
{
	return (spl_impl(0));
}

#ifdef __cplusplus
}
#endif

#endif /* _SYS_PPC_SUBR_H */
