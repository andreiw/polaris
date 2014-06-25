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
 * Copyright (c) 1994,2006 by Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/* This may not exist anymore... verify later. */
#define M_WAITERS	0

#pragma ident   "%Z%%M% %I%     %E% SML"

#include <sys/asm_linkage.h>
#include <sys/machthread.h>
#include <sys/mutex_impl.h>

#if defined(lint) || defined(__lint)

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/thread.h>
#include <sys/cpuvar.h>
#include <sys/cmn_err.h>

#else	/* lint */

#include "assym.h"

	.set	.LOCKED,0x01
	.set	.UNLOCKED,0
	.set	.MUTEX_FREE,0
	.set	.ATOMIC,1
	.set	.NOT_ATOMIC,0

	.globl	ncpus

#endif	/* lint */

/*
 * lock_try(lp), ulock_try(lp)
 *	- returns non-zero on success.
 *	- doesn't block interrupts so don't use this to spin on a lock.
 * NOTE: ulock_try is for user locks, which we treat the same as
 *	 kernel locks.
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
int
lock_try(lock_t *lp)
{ return (0); }

int
lock_spin_try(lock_t *lp)
{ return (0); }

/* ARGSUSED */
int
ulock_try(lock_t *lp)
{ return (0); }

#else	/* lint */

	ENTRY(lock_try)
	ALTENTRY(lock_spin_try)
	ALTENTRY(ulock_try)
	andi.	%r10,%r3,3		/* %r10 is byte offset in word */
	rlwinm	%r10,%r10,3,0,31	/* %r10 is bit offset in word */
	li	%r9,.LOCKED		/* %r9 is byte mask */
	rlwnm	%r9,%r9,%r10,0,31	/* %r9 is byte mask shifted */
	rlwinm	%r8,%r3,0,0,29		/* %r8 is the word to reserve */
.L5:
	lwarx	%r5,0,%r8		/* fetch and reserve lock */
	and.	%r3,%r5,%r9		/* already locked? */
	bne-	.L6			/* yes */
	or	%r5,%r5,%r9		/* set lock values in word */
	stwcx.	%r5,0,%r8		/* still reserved? */
	bne-	.L5			/* no, try again */
	li	%r3,.LOCKED		/* wasn't locked */
	isync				/* context synchronize */
	blr
.L6:
	stwcx.	%r5,0,%r8		/* clear reservation */
	li	%r3,0			/* was locked */
	blr
	SET_SIZE(lock_try)
	SET_SIZE(lock_spin_try)
	SET_SIZE(ulock_try)

#endif	/* lint */

/*
 * lock_clear(lp), ulock_clear(lp)
 *	- unlock lock without changing interrupt priority level.
 * NOTE: ulock_clear is for user locks, which we treat the same as
 *	 kernel locks.
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
lock_clear(lock_t *lp)
{}

/* ARGSUSED */
void
ulock_clear(lock_t *lp)
{}

#else	/* lint */

	ENTRY(lock_clear)
	ALTENTRY(ulock_clear)
	lbz	%r4,0(%r3)		/* get lock byte */
	andi.	%r4,%r4,(~.LOCKED)&0xff	/* clear lock bit */
	eieio				/* synchronize */
	stb	%r4,0(%r3)		/* unlock */
	blr
	SET_SIZE(lock_clear)
	SET_SIZE(ulock_clear)

#endif	/* lint */

/*
 * lock_set_spl(lp, pl)
 * 	Returns old pl.
 */

#if defined(lint) || defined(__lint)

/*ARGSUSED*/
int
lock_set_spl(lock_t *lp, int pl)
{ return (0); }

#else	/* lint */

	ENTRY(lock_set_spl)
	mflr	%r0				/* get link register */
	stw	%r0,4(%r1)			/* save link register */
	stwu	%r1,-SA(MINFRAME)(%r1)		/* establish stack frame */
	stw	%r3,8(%r1)			/* save our arguments */
	stw	%r4,12(%r1)
.L7:
	lwz	%r3,12(%r1)			/* splr(pl) */
	bl	splr
	lwz	%r7,8(%r1)			/* get our argument in %r7 */
	andi.	%r10,%r7,3			/* %r10 is byte offset in word */
	rlwinm	%r10,%r10,3,0,31		/* %r10 is bit offset in word */
	li	%r9,.LOCKED			/* %r9 is byte mask */
	rlwnm	%r9,%r9,%r10,0,31		/* %r9 is byte mask shifted */
	rlwinm	%r8,%r7,0,0,29			/* %r8 is the word to reserve */
.lost_res:
	lwarx	%r5,0,%r8			/* fetch and reserve lock */
	and.	%r0,%r5,%r9			/* already locked? */
	bne-	.L8				/* yes */
	or	%r5,%r5,%r9			/* set lock values in word */
	stwcx.	%r5,0,%r8			/* still reserved? */
	bne-	.lost_res			/* nope, try the load again */
.exit2:
	addi	%r1,%r1,SA(MINFRAME)		/* remove stack frame */
	lwz	%r0,4(%r1)			/* restore link register */
	mtlr	%r0
	isync					/* don't allow instr look ahead */
	blr					/* return with lock */
.L8:
	bl	spl_impl			/* splx(); restore spl */
	lis	%r4,ncpus@ha			/* get ncpus */
	lwz	%r4,ncpus@l(%r4)
	cmpi	%r4,%r4,1			/* npus == 1? */
	bne+	.L9				/* no, we're ok */
	lis	%r3,.lock_panic_msg@h		/* we're dead */
	ori	%r3,%r3,.lock_panic_msg@l
	bl	panic				/* break the bad news */
.L9:
	lis	%r5,panicstr@h
	lwz	%r5,panicstr@l(%r5)
	cmpwi	%r5,0				/* panicstr still NULL? */
	beq+	.L10				/* good */
	lis	%r6,panic_cpu@h			/* get panic_cpu.cpu_id */
	ori	%r6,%r6,panic_cpu@l
	lwz	%r6,CPU_ID(%r6)
	lwz	%r3,T_CPU(%THREAD_REG)
	lwz	%r3,CPU_ID(%r3)			/* curthread->t_cpu->cpu_id */
	cmp	%r3,%r3,%r6				/* same cpu? */
	beq	.exit2				/* yes, give it up */
.L10:
	lbz	%r6,0(%r7)			/* lock free yet? */
	andi.	%r6,%r6,.LOCKED			/* check lock bit */
	bne-	.L10				/* no, spin */
	b	.L7				/* yes, start all over */
	SET_SIZE(lock_set_spl)

	.rodata
.lock_panic_msg:
	.string "lock_set: lock held and only one CPU"


#endif	/* lint */

/*
 * void
 * lock_set(lp)
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
lock_set(lock_t *lp)
{}

#else	/* lint */

	ENTRY(lock_set)
	ALTENTRY(disp_lock_enter_high)
	andi.	%r10,%r3,3			/* %r10 is byte offset in word */
	rlwinm	%r10,%r10,3,0,31		/* %r10 is bit offset in word */
	li	%r9,.LOCKED			/* %r9 is byte mask */
	rlwnm	%r9,%r9,%r10,0,31		/* %r9 is byte mask shifted */
	rlwinm	%r8,%r3,0,0,29			/* %r8 is the word to reserve */
.L11:
	lwarx	%r5,0,%r8			/* fetch and reserve lock */
	and.	%r0,%r5,%r9			/* already locked? */
	bne-	.L12				/* yes */
	or	%r5,%r5,%r9			/* set lock values in word */
	stwcx.	%r5,0,%r8			/* still reserved? */
	bne-	.L11
.exit3:
	isync					/* don't allow instr look ahead */
	blr					/* return with lock */
.L12:
	lis	%r4,ncpus@ha			/* get ncpus */
	lwz	%r4,ncpus@l(%r4)
	cmpi	%r4,%r4,1			/* npus == 1? */
	bne+	.L13				/* no, we're ok */
	lis	%r3,.lock_panic_msg@h		/* we're dead */
	ori	%r3,%r3,.lock_panic_msg@l
	bl	panic				/* break the bad news */
.L13:
	lis	%r5,panicstr@h
	lwz	%r5,panicstr@l(%r5)
	cmpwi	%r5,0				/* panicstr still NULL? */
	beq+	.L14				/* good */
	lis	%r6,panic_cpu@h			/* get panic_cpu.cpu_id */
	ori	%r6,%r6,panic_cpu@l
	lwz	%r6,CPU_ID(%r6)
	lwz	%r4,T_CPU(%THREAD_REG)
	lwz	%r4,CPU_ID(%r3)			/* curthread->t_cpu->cpu_id */
	cmp	%r4,%r4,%r6			/* same cpu? */
	beq	.exit3				/* yes, give it up */
.L14:
	lbz	%r6,0(%r3)			/* lock free yet? */
	andi.	%r6,%r6,.LOCKED			/* check bit */
	bne-	.L14				/* no, spin */
	b	.L11				/* yes, start all over */
	SET_SIZE(lock_set)


#endif	/* lint */


/*
 * lock_clear_splx(lp, s)
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
lock_clear_splx(lock_t *lp, int s)
{}

#else	/* lint */

	ENTRY(lock_clear_splx)
	lbz	%r5,0(%r3)		/* get byte */
	andi.	%r5,%r5,(~.LOCKED)&0xff	/* clear lock bit */
	eieio				/* make sure it is seen */
	stb	%r5,0(%r3)		/* clear lock */
	mr	%r3,%r4			/* splx(s) */
	b	spl_impl		/* goto splx */
	SET_SIZE(lock_clear_splx)

#endif	/* lint */


/*
 * mutex_enter() and mutex_exit().
 *
 * These routines do the simple case of mutex_enter when the lock is not
 * held, and mutex_exit when no thread is waiting for the lock.
 * If anything complicated is going on, the C version (mutex_adaptive_enter)
 * is called.
 *
 * These routines do not do lock tracing.  If that is needed,
 * a separate mutex type could be used.
 *
 */
#if defined (lint) || defined(__lint)

/*ARGSUSED*/
void
mutex_enter(kmutex_t *lp)
{}

#else

	ENTRY(mutex_enter)
	lwarx	%r4,0,%r3		/* pick up and reserve owner field */
	cmpwi	%r4,.MUTEX_FREE		/* no owner? */
	bne-	1f			/* yes, there was.  Do it the hard way. */
	stwcx.	%THREAD_REG,0,%r3	/* we are the owner, right? */
	isync				/* synchronize */
	beqlr+				/* it worked, let's leave */
	b	mutex_enter		/* lost reservation, try again */
1:
	bl	mutex_vector_enter
	SET_SIZE(mutex_enter)

#endif /* lint */

#if defined (lint) || defined(__lint)

/*ARGSUSED*/
int
mutex_tryenter(kmutex_t *lp)
{}

#else

	ENTRY(mutex_tryenter)
	lwarx	%r4,0,%r3		/* pick up and reserve owner field */
	cmpwi	%r4,.MUTEX_FREE		/* no owner? */
	bne-	1f			/* yes, there was.  return failure */
	stwcx.	%THREAD_REG,0,%r3	/* we are the owner, right? */
	isync				/* synchronize */
	li	%r3, 1
	beqlr+				/* it worked, let's leave */
1:
	li	%r3, 0
	blr
	SET_SIZE(mutex_tryenter)

#endif /* lint */

#if defined (lint) || defined(__lint)

/*
 * Similar to mutex_enter() above, but here we return zero if the lock
 * cannot be acquired, and nonzero on success.  Same tricks here.
 * This is only in assembler because of the tricks.
 */
/*ARGSUSED*/
int
mutex_adaptive_tryenter(mutex_impl_t *lp)
{ return (0); }

#else
	ENTRY(mutex_adaptive_tryenter)
	lwarx	%r4,0,%r3			/* fetch owner field */
	cmpwi	%r4,.MUTEX_FREE			/* free? */
	bne-	.L15				/* nope, return failure */
	stwcx.	%THREAD_REG,0,%r3		/* store owner */
	isync					/* synchronize */
	beqlr+					/* it worked, leave */
	b	mutex_adaptive_tryenter		/* lost reservation */
.L15:
	stwcx.	%r4,0,%r3			/* clear reservation */
	li	%r3,0				/* someone else has the mutex */
	blr					/* indicate cannot acquire */
	SET_SIZE(mutex_adaptive_tryenter)



#endif /* lint */


#if defined(lint) || defined(__lint)

/*ARGSUSED*/
void
mutex_exit(kmutex_t *lp)
{}

#else
	ENTRY(mutex_exit)
	eieio				/* synchronize stores */
	li	%r5,.MUTEX_FREE		/* value to store */
.L16:
	lwarx	%r4,0,%r3		/* get current owner */
	cmpw	%THREAD_REG,%r4		/* same as us? */
	bne-	.L16.1			/* no, must be non-adaptive mutex */
	lhz	%r4,M_WAITERS(%r3)	/* check for waiters	XXXPPC (window) */
	stwcx.	%r5,0,%r3		/* free the mutex */
	bne-	.L16			/* lost reservation... */
	cmpi	%r4,%r4,0		/* are there any waiters? */
	beqlr+				/* no waiters, we are done */
#if 0
	b	mutex_adaptive_release	/* let the c code handle it */
#endif
.L16.1:
	stwcx.	%r4,0,%r3		/* clear reservation */
	b	mutex_vector_exit	/* let the c code handle it */
	SET_SIZE(mutex_exit)

#endif /* lint */


/*
 *
 * mutex_insert_waiter(kmutex_t *lp, turnstile_id_t tsid)
 *
 */

#if defined(lint) || defined(__lint)

/*ARGSUSED*/
int
mutex_insert_waiter(kmutex_t *lp, turnstile_id_t tsid)
{ return (0); }

#else /* ! defined(lint) */

	ENTRY(mutex_insert_waiter)
	lwarx	%r5,0,%r3		/* get current mutex owner and reserve */
	sth	%r4,M_WAITERS(%r3)	/* insert waiter */
	eieio				/* force waiter to be seen prior to next */
	stwcx.	%r5,0,%r3		/* still same mutex owner? */
	bne-	.changed
	li	%r3,.ATOMIC		/* indicate inserted atomically */
	blr
.changed:
	li	%r3,.NOT_ATOMIC		/* indicate failed to atomically insert */
	blr
	SET_SIZE(mutex_insert_waiter)

#endif /* ! defined(lint) */
/*
 * lock_mutex_flush()
 *
 * guarantee writes are flushed.
 */

#if defined(lint) || defined(__lint)

void
lock_mutex_flush(void)
{}

#else	/* lint */

	ENTRY(lock_mutex_flush)
	eieio
	blr
	SET_SIZE(lock_mutex_flush)

#endif	/* lint */

/*
 * thread_onproc()
 * Set thread in onproc state for the specified CPU.
 * Also set the thread lock pointer to the CPU's onproc lock.
 * Since the new lock isn't held, the store ordering is important.
 * If not done in assembler, the compiler could reorder the stores.
 */
#if defined(lint) || defined(__lint)

/*ARGSUSED*/
void
thread_onproc(kthread_id_t t, cpu_t *cp)
{}

#else	/* lint */

	ENTRY(thread_onproc)
#if 0 /* FIXME - Get this to compile */
	li	%r0,ONPROC_THREAD
	addi	%r4,%r4,CPU_THREAD_LOCK	/* set %r4 to &cp->cpu_thread_lock */
#endif
	stw	%r0,T_STATE(%r3)	/* set state to ONPROC_THREAD */
	eieio				/* make sure stores happen in order! */
	stw	%r4,T_LOCKP(%r3)	/* set lockp */
	blr
	SET_SIZE(thread_onproc)

#endif	/* lint */

#if defined(lint) || defined(__lint)

/*ARGSUSED*/
void
unlock_hres_lock(void)
{}

#else	/* lint */

	ENTRY(unlock_hres_lock)
	lis	%r3,hres_lock@ha	/* %r3 == &hres_lock */
	la	%r3,hres_lock@l(%r3)
	eieio
.unlock_hres_loop:
	lwarx	%r5,0,%r3
	addi	%r5,%r5,256-.LOCKED	/* overflow in upper 3-byte counter */
	stwcx.	%r5,0,%r3
	bne-	.unlock_hres_loop
	blr
	SET_SIZE(unlock_hres_lock)

#endif	/* lint */

#if defined(lint) || defined(__lint)

/*ARGSUSED*/
void
lock_hres_lock(void)
{}

#else	/* lint */

	ENTRY(lock_hres_lock)
	mfmsr	%r3			/* retval is current value of MSR */
	rlwinm	%r5,%r3,0,17,15		/* disable interrupts (clear MSR_EE) */
	mtmsr	%r5

	lis	%r4,hres_lock@ha	/* %r4 == &hres_lock */
	la	%r4,hres_lock@l(%r4)
	eieio
.lock_hres_loop:
	lwarx	%r5,0,%r4
	andi.	%r6,%r5,.LOCKED
	ori	%r5,%r5,.LOCKED		/* overflow in upper 3-byte counter */
	bne-	.lock_hres_loop
	stwcx.	%r5,0,%r4
	beqlr+
	b	.lock_hres_loop
	SET_SIZE(lock_hres_lock)

#endif	/* lint */


/*
 * rw_enter() and rw_exit().
 *
 * These routines handle the simple cases of rw_enter (write-locking an unheld
 * lock or read-locking a lock that's neither write-locked nor write-wanted)
 * and rw_exit (no waiters or not the last reader).  If anything complicated
 * is going on we punt to rw_enter_sleep() and rw_exit_wakeup(), respectively.
 */
#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
rw_enter(krwlock_t *lp, krw_t rw)
{}

/* ARGSUSED */
void
rw_exit(krwlock_t *lp)
{}

#else	/* lint */

	ENTRY(rw_enter)
	blr
	SET_SIZE(rw_enter)

	ENTRY(rw_exit)
	blr
	SET_SIZE(rw_exit)

#endif	/* lint */
