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

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/


#pragma ident	"@(#)disp.c	1.208	06/05/15 SMI"	/* from SVr4.0 1.30 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/signal.h>
#include <sys/user.h>
#include <sys/systm.h>
#include <sys/sysinfo.h>
#include <sys/var.h>
#include <sys/errno.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/inline.h>
#include <sys/disp.h>
#include <sys/class.h>
#include <sys/bitmap.h>
#include <sys/kmem.h>
#include <sys/cpuvar.h>
#include <sys/vtrace.h>
#include <sys/tnf.h>
#include <sys/cpupart.h>
#include <sys/lgrp.h>
#include <sys/chip.h>
#include <sys/schedctl.h>
#include <sys/atomic.h>
#include <sys/dtrace.h>
#include <sys/sdt.h>

#include <vm/as.h>

#define	BOUND_CPU	0x1
#define	BOUND_PARTITION	0x2
#define	BOUND_INTR	0x4

/* Dispatch queue allocation structure and functions */
struct disp_queue_info {
	disp_t	*dp;
	dispq_t *olddispq;
	dispq_t *newdispq;
	ulong_t	*olddqactmap;
	ulong_t	*newdqactmap;
	int	oldnglobpris;
};
static void	disp_dq_alloc(struct disp_queue_info *dptr, int numpris,
    disp_t *dp);
static void	disp_dq_assign(struct disp_queue_info *dptr, int numpris);
static void	disp_dq_free(struct disp_queue_info *dptr);

/* platform-specific routine to call when processor is idle */
static void	generic_idle_cpu();
void		(*idle_cpu)() = generic_idle_cpu;

/* routines invoked when a CPU enters/exits the idle loop */
static void	idle_enter();
static void	idle_exit();

/* platform-specific routine to call when thread is enqueued */
static void	generic_enq_thread(cpu_t *, int);
void		(*disp_enq_thread)(cpu_t *, int) = generic_enq_thread;

pri_t	kpreemptpri;		/* priority where kernel preemption applies */
pri_t	upreemptpri = 0; 	/* priority where normal preemption applies */
pri_t	intr_pri;		/* interrupt thread priority base level */

#define	KPQPRI	-1 		/* pri where cpu affinity is dropped for kpq */
pri_t	kpqpri = KPQPRI; 	/* can be set in /etc/system */
disp_t	cpu0_disp;		/* boot CPU's dispatch queue */
disp_lock_t	swapped_lock;	/* lock swapped threads and swap queue */
int	nswapped;		/* total number of swapped threads */
void	disp_swapped_enq(kthread_t *tp);
static void	disp_swapped_setrun(kthread_t *tp);
static void	cpu_resched(cpu_t *cp, pri_t tpri);

/*
 * If this is set, only interrupt threads will cause kernel preemptions.
 * This is done by changing the value of kpreemptpri.  kpreemptpri
 * will either be the max sysclass pri + 1 or the min interrupt pri.
 */
int	only_intr_kpreempt;

extern void set_idle_cpu(int cpun);
extern void unset_idle_cpu(int cpun);
static void setkpdq(kthread_t *tp, int borf);
#define	SETKP_BACK	0
#define	SETKP_FRONT	1
/*
 * Parameter that determines how recently a thread must have run
 * on the CPU to be considered loosely-bound to that CPU to reduce
 * cold cache effects.  The interval is in hertz.
 *
 * The platform may define a per physical processor adjustment of
 * this parameter. For efficiency, the effective rechoose interval
 * (rechoose_interval + per chip adjustment) is maintained in the
 * cpu structures. See cpu_choose()
 */
int	rechoose_interval = RECHOOSE_INTERVAL;
static cpu_t	*cpu_choose(kthread_t *, pri_t);

/*
 * Parameter that determines how long (in nanoseconds) a thread must
 * be sitting on a run queue before it can be stolen by another CPU
 * to reduce migrations.  The interval is in nanoseconds.
 *
 * The nosteal_nsec should be set by a platform code to an appropriate value.
 *
 */
hrtime_t nosteal_nsec = 0;

/*
 * Value of nosteal_nsec meaning that nosteal optimization should be disabled
 */
#define	NOSTEAL_DISABLED 1

id_t	defaultcid;	/* system "default" class; see dispadmin(1M) */

disp_lock_t	transition_lock;	/* lock on transitioning threads */
disp_lock_t	stop_lock;		/* lock on stopped threads */

static void	cpu_dispqalloc(int numpris);

/*
 * This gets returned by disp_getwork/disp_getbest if we couldn't steal
 * a thread because it was sitting on its run queue for a very short
 * period of time.
 */
#define	T_DONTSTEAL	(kthread_t *)(-1) /* returned by disp_getwork/getbest */

static kthread_t	*disp_getwork(cpu_t *to);
static kthread_t	*disp_getbest(disp_t *from);
static kthread_t	*disp_ratify(kthread_t *tp, disp_t *kpq);

void	swtch_to(kthread_t *);

/*
 * dispatcher and scheduler initialization
 */

/*
 * disp_setup - Common code to calculate and allocate dispatcher
 *		variables and structures based on the maximum priority.
 */
static void
disp_setup(pri_t maxglobpri, pri_t oldnglobpris)
{
	pri_t	newnglobpris;

	ASSERT(MUTEX_HELD(&cpu_lock));

	newnglobpris = maxglobpri + 1 + LOCK_LEVEL;

	if (newnglobpris > oldnglobpris) {
		/*
		 * Allocate new kp queues for each CPU partition.
		 */
		cpupart_kpqalloc(newnglobpris);

		/*
		 * Allocate new dispatch queues for each CPU.
		 */
		cpu_dispqalloc(newnglobpris);

		/*
		 * compute new interrupt thread base priority
		 */
		intr_pri = maxglobpri;
		if (only_intr_kpreempt) {
			kpreemptpri = intr_pri + 1;
			if (kpqpri == KPQPRI)
				kpqpri = kpreemptpri;
		}
		v.v_nglobpris = newnglobpris;
	}
}

/*
 * dispinit - Called to initialize all loaded classes and the
 *	      dispatcher framework.
 */
void
dispinit(void)
{
	id_t	cid;
	pri_t	maxglobpri;
	pri_t	cl_maxglobpri;

	maxglobpri = -1;

	/*
	 * Initialize transition lock, which will always be set.
	 */
	DISP_LOCK_INIT(&transition_lock);
	disp_lock_enter_high(&transition_lock);
	DISP_LOCK_INIT(&stop_lock);

	mutex_enter(&cpu_lock);
	CPU->cpu_disp->disp_maxrunpri = -1;
	CPU->cpu_disp->disp_max_unbound_pri = -1;
	/*
	 * Initialize the default CPU partition.
	 */
	cpupart_initialize_default();
	/*
	 * Call the class specific initialization functions for
	 * all pre-installed schedulers.
	 *
	 * We pass the size of a class specific parameter
	 * buffer to each of the initialization functions
	 * to try to catch problems with backward compatibility
	 * of class modules.
	 *
	 * For example a new class module running on an old system
	 * which didn't provide sufficiently large parameter buffers
	 * would be bad news. Class initialization modules can check for
	 * this and take action if they detect a problem.
	 */

	for (cid = 0; cid < nclass; cid++) {
		sclass_t	*sc;

		sc = &sclass[cid];
		if (SCHED_INSTALLED(sc)) {
			cl_maxglobpri = sc->cl_init(cid, PC_CLPARMSZ,
			    &sc->cl_funcs);
			if (cl_maxglobpri > maxglobpri)
				maxglobpri = cl_maxglobpri;
		}
	}
	kpreemptpri = (pri_t)v.v_maxsyspri + 1;
	if (kpqpri == KPQPRI)
		kpqpri = kpreemptpri;

	ASSERT(maxglobpri >= 0);
	disp_setup(maxglobpri, 0);

	mutex_exit(&cpu_lock);

	/*
	 * Get the default class ID; this may be later modified via
	 * dispadmin(1M).  This will load the class (normally TS) and that will
	 * call disp_add(), which is why we had to drop cpu_lock first.
	 */
	if (getcid(defaultclass, &defaultcid) != 0) {
		cmn_err(CE_PANIC, "Couldn't load default scheduling class '%s'",
		    defaultclass);
	}
}

/*
 * disp_add - Called with class pointer to initialize the dispatcher
 *	      for a newly loaded class.
 */
void
disp_add(sclass_t *clp)
{
	pri_t	maxglobpri;
	pri_t	cl_maxglobpri;

	mutex_enter(&cpu_lock);
	/*
	 * Initialize the scheduler class.
	 */
	maxglobpri = (pri_t)(v.v_nglobpris - LOCK_LEVEL - 1);
	cl_maxglobpri = clp->cl_init(clp - sclass, PC_CLPARMSZ, &clp->cl_funcs);
	if (cl_maxglobpri > maxglobpri)
		maxglobpri = cl_maxglobpri;

	/*
	 * Save old queue information.  Since we're initializing a
	 * new scheduling class which has just been loaded, then
	 * the size of the dispq may have changed.  We need to handle
	 * that here.
	 */
	disp_setup(maxglobpri, v.v_nglobpris);

	mutex_exit(&cpu_lock);
}


/*
 * For each CPU, allocate new dispatch queues
 * with the stated number of priorities.
 */
static void
cpu_dispqalloc(int numpris)
{
	cpu_t	*cpup;
	struct disp_queue_info	*disp_mem;
	int i, num;

	ASSERT(MUTEX_HELD(&cpu_lock));

	disp_mem = kmem_zalloc(NCPU *
	    sizeof (struct disp_queue_info), KM_SLEEP);

	/*
	 * This routine must allocate all of the memory before stopping
	 * the cpus because it must not sleep in kmem_alloc while the
	 * CPUs are stopped.  Locks they hold will not be freed until they
	 * are restarted.
	 */
	i = 0;
	cpup = cpu_list;
	do {
		disp_dq_alloc(&disp_mem[i], numpris, cpup->cpu_disp);
		i++;
		cpup = cpup->cpu_next;
	} while (cpup != cpu_list);
	num = i;

	pause_cpus(NULL);
	for (i = 0; i < num; i++)
		disp_dq_assign(&disp_mem[i], numpris);
	start_cpus();

	/*
	 * I must free all of the memory after starting the cpus because
	 * I can not risk sleeping in kmem_free while the cpus are stopped.
	 */
	for (i = 0; i < num; i++)
		disp_dq_free(&disp_mem[i]);

	kmem_free(disp_mem, NCPU * sizeof (struct disp_queue_info));
}

static void
disp_dq_alloc(struct disp_queue_info *dptr, int numpris, disp_t	*dp)
{
	dptr->newdispq = kmem_zalloc(numpris * sizeof (dispq_t), KM_SLEEP);
	dptr->newdqactmap = kmem_zalloc(((numpris / BT_NBIPUL) + 1) *
	    sizeof (long), KM_SLEEP);
	dptr->dp = dp;
}

static void
disp_dq_assign(struct disp_queue_info *dptr, int numpris)
{
	disp_t	*dp;

	dp = dptr->dp;
	dptr->olddispq = dp->disp_q;
	dptr->olddqactmap = dp->disp_qactmap;
	dptr->oldnglobpris = dp->disp_npri;

	ASSERT(dptr->oldnglobpris < numpris);

	if (dptr->olddispq != NULL) {
		/*
		 * Use kcopy because bcopy is platform-specific
		 * and could block while we might have paused the cpus.
		 */
		(void) kcopy(dptr->olddispq, dptr->newdispq,
		    dptr->oldnglobpris * sizeof (dispq_t));
		(void) kcopy(dptr->olddqactmap, dptr->newdqactmap,
		    ((dptr->oldnglobpris / BT_NBIPUL) + 1) *
		    sizeof (long));
	}
	dp->disp_q = dptr->newdispq;
	dp->disp_qactmap = dptr->newdqactmap;
	dp->disp_q_limit = &dptr->newdispq[numpris];
	dp->disp_npri = numpris;
}

static void
disp_dq_free(struct disp_queue_info *dptr)
{
	if (dptr->olddispq != NULL)
		kmem_free(dptr->olddispq,
		    dptr->oldnglobpris * sizeof (dispq_t));
	if (dptr->olddqactmap != NULL)
		kmem_free(dptr->olddqactmap,
		    ((dptr->oldnglobpris / BT_NBIPUL) + 1) * sizeof (long));
}

/*
 * For a newly created CPU, initialize the dispatch queue.
 * This is called before the CPU is known through cpu[] or on any lists.
 */
void
disp_cpu_init(cpu_t *cp)
{
	disp_t	*dp;
	dispq_t	*newdispq;
	ulong_t	*newdqactmap;

	ASSERT(MUTEX_HELD(&cpu_lock));	/* protect dispatcher queue sizes */

	if (cp == cpu0_disp.disp_cpu)
		dp = &cpu0_disp;
	else
		dp = kmem_alloc(sizeof (disp_t), KM_SLEEP);
	bzero(dp, sizeof (disp_t));
	cp->cpu_disp = dp;
	dp->disp_cpu = cp;
	dp->disp_maxrunpri = -1;
	dp->disp_max_unbound_pri = -1;
	DISP_LOCK_INIT(&cp->cpu_thread_lock);
	/*
	 * Allocate memory for the dispatcher queue headers
	 * and the active queue bitmap.
	 */
	newdispq = kmem_zalloc(v.v_nglobpris * sizeof (dispq_t), KM_SLEEP);
	newdqactmap = kmem_zalloc(((v.v_nglobpris / BT_NBIPUL) + 1) *
	    sizeof (long), KM_SLEEP);
	dp->disp_q = newdispq;
	dp->disp_qactmap = newdqactmap;
	dp->disp_q_limit = &newdispq[v.v_nglobpris];
	dp->disp_npri = v.v_nglobpris;
}

void
disp_cpu_fini(cpu_t *cp)
{
	ASSERT(MUTEX_HELD(&cpu_lock));

	disp_kp_free(cp->cpu_disp);
	if (cp->cpu_disp != &cpu0_disp)
		kmem_free(cp->cpu_disp, sizeof (disp_t));
}

/*
 * Allocate new, larger kpreempt dispatch queue to replace the old one.
 */
void
disp_kp_alloc(disp_t *dq, pri_t npri)
{
	struct disp_queue_info	mem_info;

	if (npri > dq->disp_npri) {
		/*
		 * Allocate memory for the new array.
		 */
		disp_dq_alloc(&mem_info, npri, dq);

		/*
		 * We need to copy the old structures to the new
		 * and free the old.
		 */
		disp_dq_assign(&mem_info, npri);
		disp_dq_free(&mem_info);
	}
}

/*
 * Free dispatch queue.
 * Used for the kpreempt queues for a removed CPU partition and
 * for the per-CPU queues of deleted CPUs.
 */
void
disp_kp_free(disp_t *dq)
{
	struct disp_queue_info	mem_info;

	mem_info.olddispq = dq->disp_q;
	mem_info.olddqactmap = dq->disp_qactmap;
	mem_info.oldnglobpris = dq->disp_npri;
	disp_dq_free(&mem_info);
}

/*
 * End dispatcher and scheduler initialization.
 */

/*
 * See if there's anything to do other than remain idle.
 * Return non-zero if there is.
 *
 * This function must be called with high spl, or with
 * kernel preemption disabled to prevent the partition's
 * active cpu list from changing while being traversed.
 *
 */
int
disp_anywork(void)
{
	cpu_t   *cp = CPU;
	cpu_t   *ocp;

	if (cp->cpu_disp->disp_nrunnable != 0)
		return (1);

	if (!(cp->cpu_flags & CPU_OFFLINE)) {
		if (CP_MAXRUNPRI(cp->cpu_part) >= 0)
			return (1);

		/*
		 * Work can be taken from another CPU if:
		 *	- There is unbound work on the run queue
		 *	- That work isn't a thread undergoing a
		 *	- context switch on an otherwise empty queue.
		 *	- The CPU isn't running the idle loop.
		 */
		for (ocp = cp->cpu_next_part; ocp != cp;
		    ocp = ocp->cpu_next_part) {
			ASSERT(CPU_ACTIVE(ocp));

			if (ocp->cpu_disp->disp_max_unbound_pri != -1 &&
			    !((ocp->cpu_disp_flags & CPU_DISP_DONTSTEAL) &&
			    ocp->cpu_disp->disp_nrunnable == 1) &&
			    ocp->cpu_dispatch_pri != -1)
				return (1);
		}
	}
	return (0);
}

/*
 * Called when CPU enters the idle loop
 */
static void
idle_enter()
{
	cpu_t		*cp = CPU;

	new_cpu_mstate(CMS_IDLE, gethrtime_unscaled());
	CPU_STATS_ADDQ(cp, sys, idlethread, 1);
	set_idle_cpu(cp->cpu_id);	/* arch-dependent hook */
}

/*
 * Called when CPU exits the idle loop
 */
static void
idle_exit()
{
	cpu_t		*cp = CPU;

	new_cpu_mstate(CMS_SYSTEM, gethrtime_unscaled());
	unset_idle_cpu(cp->cpu_id);	/* arch-dependent hook */
}

/*
 * Idle loop.
 */
void
idle()
{
	struct cpu	*cp = CPU;		/* pointer to this CPU */
	kthread_t	*t;			/* taken thread */

	idle_enter();

	/*
	 * Uniprocessor version of idle loop.
	 * Do this until notified that we're on an actual multiprocessor.
	 */
	while (ncpus == 1) {
		if (cp->cpu_disp->disp_nrunnable == 0) {
			(*idle_cpu)();
			continue;
		}
		idle_exit();
		swtch();

		idle_enter(); /* returned from swtch */
	}

	/*
	 * Multiprocessor idle loop.
	 */
	for (;;) {
		/*
		 * If CPU is completely quiesced by p_online(2), just wait
		 * here with minimal bus traffic until put online.
		 */
		while (cp->cpu_flags & CPU_QUIESCED)
			(*idle_cpu)();

		if (cp->cpu_disp->disp_nrunnable != 0) {
			idle_exit();
			swtch();
		} else {
			if (cp->cpu_flags & CPU_OFFLINE)
				continue;
			if ((t = disp_getwork(cp)) == NULL) {
				if (cp->cpu_chosen_level != -1) {
					disp_t *dp = cp->cpu_disp;
					disp_t *kpq;

					disp_lock_enter(&dp->disp_lock);
					/*
					 * Set kpq under lock to prevent
					 * migration between partitions.
					 */
					kpq = &cp->cpu_part->cp_kp_queue;
					if (kpq->disp_maxrunpri == -1)
						cp->cpu_chosen_level = -1;
					disp_lock_exit(&dp->disp_lock);
				}
				(*idle_cpu)();
				continue;
			}
			/*
			 * If there was a thread but we couldn't steal
			 * it, then keep trying.
			 */
			if (t == T_DONTSTEAL)
				continue;
			idle_exit();
			restore_mstate(t);
			swtch_to(t);
		}
		idle_enter(); /* returned from swtch/swtch_to */
	}
}


/*
 * Preempt the currently running thread in favor of the highest
 * priority thread.  The class of the current thread controls
 * where it goes on the dispatcher queues. If panicking, turn
 * preemption off.
 */
void
preempt()
{
	kthread_t 	*t = curthread;
	klwp_t 		*lwp = ttolwp(curthread);

	if (panicstr)
		return;

	TRACE_0(TR_FAC_DISP, TR_PREEMPT_START, "preempt_start");

	thread_lock(t);

	if (t->t_state != TS_ONPROC || t->t_disp_queue != CPU->cpu_disp) {
		/*
		 * this thread has already been chosen to be run on
		 * another CPU. Clear kprunrun on this CPU since we're
		 * already headed for swtch().
		 */
		CPU->cpu_kprunrun = 0;
		thread_unlock_nopreempt(t);
		TRACE_0(TR_FAC_DISP, TR_PREEMPT_END, "preempt_end");
	} else {
		if (lwp != NULL)
			lwp->lwp_ru.nivcsw++;
		CPU_STATS_ADDQ(CPU, sys, inv_swtch, 1);
		THREAD_TRANSITION(t);
		CL_PREEMPT(t);
		DTRACE_SCHED(preempt);
		thread_unlock_nopreempt(t);

		TRACE_0(TR_FAC_DISP, TR_PREEMPT_END, "preempt_end");

		swtch();		/* clears CPU->cpu_runrun via disp() */
	}
}

extern kthread_t *thread_unpin();

/*
 * disp() - find the highest priority thread for this processor to run, and
 * set it in TS_ONPROC state so that resume() can be called to run it.
 */
static kthread_t *
disp()
{
	cpu_t		*cpup;
	disp_t		*dp;
	kthread_t	*tp;
	dispq_t		*dq;
	int		maxrunword;
	pri_t		pri;
	disp_t		*kpq;

	TRACE_0(TR_FAC_DISP, TR_DISP_START, "disp_start");

	cpup = CPU;
	/*
	 * Find the highest priority loaded, runnable thread.
	 */
	dp = cpup->cpu_disp;

reschedule:
	/*
	 * If there is more important work on the global queue with a better
	 * priority than the maximum on this CPU, take it now.
	 */
	kpq = &cpup->cpu_part->cp_kp_queue;
	while ((pri = kpq->disp_maxrunpri) >= 0 &&
	    pri >= dp->disp_maxrunpri &&
	    (cpup->cpu_flags & CPU_OFFLINE) == 0 &&
	    (tp = disp_getbest(kpq)) != NULL) {
		if (disp_ratify(tp, kpq) != NULL) {
			TRACE_1(TR_FAC_DISP, TR_DISP_END,
			    "disp_end:tid %p", tp);
			restore_mstate(tp);
			return (tp);
		}
	}

	disp_lock_enter(&dp->disp_lock);
	pri = dp->disp_maxrunpri;

	/*
	 * If there is nothing to run, look at what's runnable on other queues.
	 * Choose the idle thread if the CPU is quiesced.
	 * Note that CPUs that have the CPU_OFFLINE flag set can still run
	 * interrupt threads, which will be the only threads on the CPU's own
	 * queue, but cannot run threads from other queues.
	 */
	if (pri == -1) {
		if (!(cpup->cpu_flags & CPU_OFFLINE)) {
			disp_lock_exit(&dp->disp_lock);
			if ((tp = disp_getwork(cpup)) == NULL ||
			    tp == T_DONTSTEAL) {
				tp = cpup->cpu_idle_thread;
				(void) splhigh();
				THREAD_ONPROC(tp, cpup);
				cpup->cpu_dispthread = tp;
				cpup->cpu_dispatch_pri = -1;
				cpup->cpu_runrun = cpup->cpu_kprunrun = 0;
				cpup->cpu_chosen_level = -1;
			}
		} else {
			disp_lock_exit_high(&dp->disp_lock);
			tp = cpup->cpu_idle_thread;
			THREAD_ONPROC(tp, cpup);
			cpup->cpu_dispthread = tp;
			cpup->cpu_dispatch_pri = -1;
			cpup->cpu_runrun = cpup->cpu_kprunrun = 0;
			cpup->cpu_chosen_level = -1;
		}
		TRACE_1(TR_FAC_DISP, TR_DISP_END,
			"disp_end:tid %p", tp);
		restore_mstate(tp);
		return (tp);
	}

	dq = &dp->disp_q[pri];
	tp = dq->dq_first;

	ASSERT(tp != NULL);
	ASSERT(tp->t_schedflag & TS_LOAD);	/* thread must be swapped in */

	DTRACE_SCHED2(dequeue, kthread_t *, tp, disp_t *, dp);

	/*
	 * Found it so remove it from queue.
	 */
	dp->disp_nrunnable--;
	dq->dq_sruncnt--;
	if ((dq->dq_first = tp->t_link) == NULL) {
		ulong_t	*dqactmap = dp->disp_qactmap;

		ASSERT(dq->dq_sruncnt == 0);
		dq->dq_last = NULL;

		/*
		 * The queue is empty, so the corresponding bit needs to be
		 * turned off in dqactmap.   If nrunnable != 0 just took the
		 * last runnable thread off the
		 * highest queue, so recompute disp_maxrunpri.
		 */
		maxrunword = pri >> BT_ULSHIFT;
		dqactmap[maxrunword] &= ~BT_BIW(pri);

		if (dp->disp_nrunnable == 0) {
			dp->disp_max_unbound_pri = -1;
			dp->disp_maxrunpri = -1;
		} else {
			int ipri;

			ipri = bt_gethighbit(dqactmap, maxrunword);
			dp->disp_maxrunpri = ipri;
			if (ipri < dp->disp_max_unbound_pri)
				dp->disp_max_unbound_pri = ipri;
		}
	} else {
		tp->t_link = NULL;
	}

	/*
	 * Set TS_DONT_SWAP flag to prevent another processor from swapping
	 * out this thread before we have a chance to run it.
	 * While running, it is protected against swapping by t_lock.
	 */
	tp->t_schedflag |= TS_DONT_SWAP;
	cpup->cpu_dispthread = tp;		/* protected by spl only */
	cpup->cpu_dispatch_pri = pri;
	ASSERT(pri == DISP_PRIO(tp));
	thread_onproc(tp, cpup);  		/* set t_state to TS_ONPROC */
	disp_lock_exit_high(&dp->disp_lock);	/* drop run queue lock */

	ASSERT(tp != NULL);
	TRACE_1(TR_FAC_DISP, TR_DISP_END,
		"disp_end:tid %p", tp);

	if (disp_ratify(tp, kpq) == NULL)
		goto reschedule;

	restore_mstate(tp);
	return (tp);
}

/*
 * swtch()
 *	Find best runnable thread and run it.
 *	Called with the current thread already switched to a new state,
 *	on a sleep queue, run queue, stopped, and not zombied.
 *	May be called at any spl level less than or equal to LOCK_LEVEL.
 *	Always drops spl to the base level (spl0()).
 */
void
swtch()
{
	kthread_t	*t = curthread;
	kthread_t	*next;
	cpu_t		*cp;

	TRACE_0(TR_FAC_DISP, TR_SWTCH_START, "swtch_start");

	if (t->t_flag & T_INTR_THREAD)
		cpu_intr_swtch_enter(t);

	if (t->t_intr != NULL) {
		/*
		 * We are an interrupt thread.  Setup and return
		 * the interrupted thread to be resumed.
		 */
		(void) splhigh();	/* block other scheduler action */
		cp = CPU;		/* now protected against migration */
		ASSERT(CPU_ON_INTR(cp) == 0);	/* not called with PIL > 10 */
		CPU_STATS_ADDQ(cp, sys, pswitch, 1);
		CPU_STATS_ADDQ(cp, sys, intrblk, 1);
		next = thread_unpin();
		TRACE_0(TR_FAC_DISP, TR_RESUME_START, "resume_start");
		resume_from_intr(next);
	} else {
#ifdef	DEBUG
		if (t->t_state == TS_ONPROC &&
		    t->t_disp_queue->disp_cpu == CPU &&
		    t->t_preempt == 0) {
			thread_lock(t);
			ASSERT(t->t_state != TS_ONPROC ||
			    t->t_disp_queue->disp_cpu != CPU ||
			    t->t_preempt != 0);	/* cannot migrate */
			thread_unlock_nopreempt(t);
		}
#endif	/* DEBUG */
		cp = CPU;
		next = disp();		/* returns with spl high */
		ASSERT(CPU_ON_INTR(cp) == 0);	/* not called with PIL > 10 */

		/* OK to steal anything left on run queue */
		cp->cpu_disp_flags &= ~CPU_DISP_DONTSTEAL;

		if (next != t) {
			if (t == cp->cpu_idle_thread) {
				CHIP_NRUNNING(cp->cpu_chip, 1);
			} else if (next == cp->cpu_idle_thread) {
				CHIP_NRUNNING(cp->cpu_chip, -1);
			}

			CPU_STATS_ADDQ(cp, sys, pswitch, 1);
			cp->cpu_last_swtch = t->t_disp_time = lbolt;
			TRACE_0(TR_FAC_DISP, TR_RESUME_START, "resume_start");

			if (dtrace_vtime_active)
				dtrace_vtime_switch(next);

			resume(next);
			/*
			 * The TR_RESUME_END and TR_SWTCH_END trace points
			 * appear at the end of resume(), because we may not
			 * return here
			 */
		} else {
			if (t->t_flag & T_INTR_THREAD)
				cpu_intr_swtch_exit(t);

			DTRACE_SCHED(remain__cpu);
			TRACE_0(TR_FAC_DISP, TR_SWTCH_END, "swtch_end");
			(void) spl0();
		}
	}
}

/*
 * swtch_from_zombie()
 *	Special case of swtch(), which allows checks for TS_ZOMB to be
 *	eliminated from normal resume.
 *	Find best runnable thread and run it.
 *	Called with the current thread zombied.
 *	Zombies cannot migrate, so CPU references are safe.
 */
void
swtch_from_zombie()
{
	kthread_t	*next;
	cpu_t		*cpu = CPU;

	TRACE_0(TR_FAC_DISP, TR_SWTCH_START, "swtch_start");

	ASSERT(curthread->t_state == TS_ZOMB);

	next = disp();			/* returns with spl high */
	ASSERT(CPU_ON_INTR(CPU) == 0);	/* not called with PIL > 10 */
	CPU_STATS_ADDQ(CPU, sys, pswitch, 1);
	ASSERT(next != curthread);
	TRACE_0(TR_FAC_DISP, TR_RESUME_START, "resume_start");

	if (next == cpu->cpu_idle_thread)
		CHIP_NRUNNING(cpu->cpu_chip, -1);

	if (dtrace_vtime_active)
		dtrace_vtime_switch(next);

	resume_from_zombie(next);
	/*
	 * The TR_RESUME_END and TR_SWTCH_END trace points
	 * appear at the end of resume(), because we certainly will not
	 * return here
	 */
}

#if defined(DEBUG) && (defined(DISP_DEBUG) || defined(lint))
static int
thread_on_queue(kthread_t *tp)
{
	cpu_t	*cp;
	cpu_t	*self;
	disp_t	*dp;

	self = CPU;
	cp = self->cpu_next_onln;
	dp = cp->cpu_disp;
	for (;;) {
		dispq_t		*dq;
		dispq_t		*eq;

		disp_lock_enter_high(&dp->disp_lock);
		for (dq = dp->disp_q, eq = dp->disp_q_limit; dq < eq; ++dq) {
			kthread_t	*rp;

			ASSERT(dq->dq_last == NULL ||
				dq->dq_last->t_link == NULL);
			for (rp = dq->dq_first; rp; rp = rp->t_link)
				if (tp == rp) {
					disp_lock_exit_high(&dp->disp_lock);
					return (1);
				}
		}
		disp_lock_exit_high(&dp->disp_lock);
		if (cp == NULL)
			break;
		if (cp == self) {
			cp = NULL;
			dp = &cp->cpu_part->cp_kp_queue;
		} else {
			cp = cp->cpu_next_onln;
			dp = cp->cpu_disp;
		}
	}
	return (0);
}	/* end of thread_on_queue */
#else

#define	thread_on_queue(tp)	0	/* ASSERT must be !thread_on_queue */

#endif  /* DEBUG */

/*
 * like swtch(), but switch to a specified thread taken from another CPU.
 *	called with spl high..
 */
void
swtch_to(kthread_t *next)
{
	cpu_t			*cp = CPU;

	TRACE_0(TR_FAC_DISP, TR_SWTCH_START, "swtch_start");

	/*
	 * Update context switch statistics.
	 */
	CPU_STATS_ADDQ(cp, sys, pswitch, 1);

	TRACE_0(TR_FAC_DISP, TR_RESUME_START, "resume_start");

	if (curthread == cp->cpu_idle_thread)
		CHIP_NRUNNING(cp->cpu_chip, 1);

	/* OK to steal anything left on run queue */
	cp->cpu_disp_flags &= ~CPU_DISP_DONTSTEAL;

	/* record last execution time */
	cp->cpu_last_swtch = curthread->t_disp_time = lbolt;

	if (dtrace_vtime_active)
		dtrace_vtime_switch(next);

	resume(next);
	/*
	 * The TR_RESUME_END and TR_SWTCH_END trace points
	 * appear at the end of resume(), because we may not
	 * return here
	 */
}



#define	CPU_IDLING(pri)	((pri) == -1)

static void
cpu_resched(cpu_t *cp, pri_t tpri)
{
	int	call_poke_cpu = 0;
	pri_t   cpupri = cp->cpu_dispatch_pri;

	if (!CPU_IDLING(cpupri) && (cpupri < tpri)) {
		TRACE_2(TR_FAC_DISP, TR_CPU_RESCHED,
		    "CPU_RESCHED:Tpri %d Cpupri %d", tpri, cpupri);
		if (tpri >= upreemptpri && cp->cpu_runrun == 0) {
			cp->cpu_runrun = 1;
			aston(cp->cpu_dispthread);
			if (tpri < kpreemptpri && cp != CPU)
				call_poke_cpu = 1;
		}
		if (tpri >= kpreemptpri && cp->cpu_kprunrun == 0) {
			cp->cpu_kprunrun = 1;
			if (cp != CPU)
				call_poke_cpu = 1;
		}
	}

	/*
	 * Propagate cpu_runrun, and cpu_kprunrun to global visibility.
	 */
	membar_enter();

	if (call_poke_cpu)
		poke_cpu(cp->cpu_id);
}

/*
 * Routine used by setbackdq() to balance load across the physical
 * processors. Returns a CPU of a lesser loaded chip in the lgroup
 * if balancing is necessary, or the "hint" CPU if it's not.
 *
 * - tp is the thread being enqueued
 * - cp is a hint CPU (chosen by cpu_choose()).
 * - curchip (if not NULL) is the chip on which the current thread
 *   is running.
 *
 * The thread lock for "tp" must be held while calling this routine.
 */
static cpu_t *
chip_balance(kthread_t *tp, cpu_t *cp, chip_t *curchip)
{
	int	chp_nrun, ochp_nrun;
	chip_t	*chp, *nchp;

	chp = cp->cpu_chip;
	chp_nrun = chp->chip_nrunning;

	if (chp == curchip)
		chp_nrun--;	/* Ignore curthread */

	/*
	 * If this chip isn't at all idle, then let
	 * run queue balancing do the work.
	 */
	if (chp_nrun == chp->chip_ncpu)
		return (cp);

	nchp = chp->chip_balance;
	do {
		if (nchp == chp ||
		    !CHIP_IN_CPUPART(nchp, tp->t_cpupart))
			continue;

		ochp_nrun = nchp->chip_nrunning;

		/*
		 * If the other chip is running less threads,
		 * or if it's running the same number of threads, but
		 * has more online logical CPUs, then choose to balance.
		 */
		if (chp_nrun > ochp_nrun ||
		    (chp_nrun == ochp_nrun &&
		    nchp->chip_ncpu > chp->chip_ncpu)) {
			cp = nchp->chip_cpus;
			nchp->chip_cpus = cp->cpu_next_chip;

			/*
			 * Find a CPU on the chip in the correct
			 * partition. We know at least one exists
			 * because of the CHIP_IN_CPUPART() check above.
			 */
			while (cp->cpu_part != tp->t_cpupart)
				cp = cp->cpu_next_chip;
		}
		chp->chip_balance = nchp->chip_next_lgrp;
		break;
	} while ((nchp = nchp->chip_next_lgrp) != chp->chip_balance);

	ASSERT(CHIP_IN_CPUPART(cp->cpu_chip, tp->t_cpupart));
	return (cp);
}

/*
 * setbackdq() keeps runqs balanced such that the difference in length
 * between the chosen runq and the next one is no more than RUNQ_MAX_DIFF.
 * For threads with priorities below RUNQ_MATCH_PRI levels, the runq's lengths
 * must match.  When per-thread TS_RUNQMATCH flag is set, setbackdq() will
 * try to keep runqs perfectly balanced regardless of the thread priority.
 */
#define	RUNQ_MATCH_PRI	16	/* pri below which queue lengths must match */
#define	RUNQ_MAX_DIFF	2	/* maximum runq length difference */
#define	RUNQ_LEN(cp, pri)	((cp)->cpu_disp->disp_q[pri].dq_sruncnt)

/*
 * Put the specified thread on the back of the dispatcher
 * queue corresponding to its current priority.
 *
 * Called with the thread in transition, onproc or stopped state
 * and locked (transition implies locked) and at high spl.
 * Returns with the thread in TS_RUN state and still locked.
 */
void
setbackdq(kthread_t *tp)
{
	dispq_t	*dq;
	disp_t		*dp;
	chip_t		*curchip = NULL;
	cpu_t		*cp;
	pri_t		tpri;
	int		bound;

	ASSERT(THREAD_LOCK_HELD(tp));
	ASSERT((tp->t_schedflag & TS_ALLSTART) == 0);

	if (tp->t_waitrq == 0) {
		hrtime_t curtime;

		curtime = gethrtime_unscaled();
		(void) cpu_update_pct(tp, curtime);
		tp->t_waitrq = curtime;
	} else {
		(void) cpu_update_pct(tp, gethrtime_unscaled());
	}

	ASSERT(!thread_on_queue(tp));	/* make sure tp isn't on a runq */

	/*
	 * If thread is "swapped" or on the swap queue don't
	 * queue it, but wake sched.
	 */
	if ((tp->t_schedflag & (TS_LOAD | TS_ON_SWAPQ)) != TS_LOAD) {
		disp_swapped_setrun(tp);
		return;
	}

	tpri = DISP_PRIO(tp);
	if (tp == curthread) {
		curchip = CPU->cpu_chip;
	}

	if (ncpus == 1)
		cp = tp->t_cpu;
	else if (!tp->t_bound_cpu && !tp->t_weakbound_cpu) {
		if (tpri >= kpqpri) {
			setkpdq(tp, SETKP_BACK);
			return;
		}
		/*
		 * Let cpu_choose suggest a CPU.
		 */
		cp = cpu_choose(tp, tpri);

		if (tp->t_cpupart == cp->cpu_part) {
			int	qlen;

			/*
			 * Select another CPU if we need
			 * to do some load balancing across the
			 * physical processors.
			 */
			if (CHIP_SHOULD_BALANCE(cp->cpu_chip))
				cp = chip_balance(tp, cp, curchip);

			/*
			 * Balance across the run queues
			 */
			qlen = RUNQ_LEN(cp, tpri);
			if (tpri >= RUNQ_MATCH_PRI &&
			    !(tp->t_schedflag & TS_RUNQMATCH))
				qlen -= RUNQ_MAX_DIFF;
			if (qlen > 0) {
				cpu_t *newcp;

				if (tp->t_lpl->lpl_lgrpid == LGRP_ROOTID) {
					newcp = cp->cpu_next_part;
				} else if ((newcp = cp->cpu_next_lpl) == cp) {
					newcp = cp->cpu_next_part;
				}

				if (RUNQ_LEN(newcp, tpri) < qlen) {
					DTRACE_PROBE3(runq__balance,
					    kthread_t *, tp,
					    cpu_t *, cp, cpu_t *, newcp);
					cp = newcp;
				}
			}
		} else {
			/*
			 * Migrate to a cpu in the new partition.
			 */
			cp = disp_lowpri_cpu(tp->t_cpupart->cp_cpulist,
			    tp->t_lpl, tp->t_pri, NULL);
		}
		bound = 0;
		ASSERT((cp->cpu_flags & CPU_QUIESCED) == 0);
	} else {
		/*
		 * It is possible that t_weakbound_cpu != t_bound_cpu (for
		 * a short time until weak binding that existed when the
		 * strong binding was established has dropped) so we must
		 * favour weak binding over strong.
		 */
		cp = tp->t_weakbound_cpu ?
		    tp->t_weakbound_cpu : tp->t_bound_cpu;
		bound = 1;
	}
	dp = cp->cpu_disp;
	disp_lock_enter_high(&dp->disp_lock);

	DTRACE_SCHED3(enqueue, kthread_t *, tp, disp_t *, dp, int, 0);
	TRACE_3(TR_FAC_DISP, TR_BACKQ, "setbackdq:pri %d cpu %p tid %p",
		tpri, cp, tp);

#ifndef NPROBE
	/* Kernel probe */
	if (tnf_tracing_active)
		tnf_thread_queue(tp, cp, tpri);
#endif /* NPROBE */

	ASSERT(tpri >= 0 && tpri < dp->disp_npri);

	THREAD_RUN(tp, &dp->disp_lock);		/* set t_state to TS_RUN */
	tp->t_disp_queue = dp;
	tp->t_link = NULL;

	dq = &dp->disp_q[tpri];
	dp->disp_nrunnable++;
	if (!bound)
		dp->disp_steal = 0;
	membar_enter();

	if (dq->dq_sruncnt++ != 0) {
		ASSERT(dq->dq_first != NULL);
		dq->dq_last->t_link = tp;
		dq->dq_last = tp;
	} else {
		ASSERT(dq->dq_first == NULL);
		ASSERT(dq->dq_last == NULL);
		dq->dq_first = dq->dq_last = tp;
		BT_SET(dp->disp_qactmap, tpri);
		if (tpri > dp->disp_maxrunpri) {
			dp->disp_maxrunpri = tpri;
			membar_enter();
			cpu_resched(cp, tpri);
		}
	}

	if (!bound && tpri > dp->disp_max_unbound_pri) {
		if (tp == curthread && dp->disp_max_unbound_pri == -1 &&
		    cp == CPU) {
			/*
			 * If there are no other unbound threads on the
			 * run queue, don't allow other CPUs to steal
			 * this thread while we are in the middle of a
			 * context switch. We may just switch to it
			 * again right away. CPU_DISP_DONTSTEAL is cleared
			 * in swtch and swtch_to.
			 */
			cp->cpu_disp_flags |= CPU_DISP_DONTSTEAL;
		}
		dp->disp_max_unbound_pri = tpri;
	}
	(*disp_enq_thread)(cp, bound);
}

/*
 * Put the specified thread on the front of the dispatcher
 * queue corresponding to its current priority.
 *
 * Called with the thread in transition, onproc or stopped state
 * and locked (transition implies locked) and at high spl.
 * Returns with the thread in TS_RUN state and still locked.
 */
void
setfrontdq(kthread_t *tp)
{
	disp_t		*dp;
	dispq_t		*dq;
	cpu_t		*cp;
	pri_t		tpri;
	int		bound;

	ASSERT(THREAD_LOCK_HELD(tp));
	ASSERT((tp->t_schedflag & TS_ALLSTART) == 0);

	if (tp->t_waitrq == 0) {
		hrtime_t curtime;

		curtime = gethrtime_unscaled();
		(void) cpu_update_pct(tp, curtime);
		tp->t_waitrq = curtime;
	} else {
		(void) cpu_update_pct(tp, gethrtime_unscaled());
	}

	ASSERT(!thread_on_queue(tp));	/* make sure tp isn't on a runq */

	/*
	 * If thread is "swapped" or on the swap queue don't
	 * queue it, but wake sched.
	 */
	if ((tp->t_schedflag & (TS_LOAD | TS_ON_SWAPQ)) != TS_LOAD) {
		disp_swapped_setrun(tp);
		return;
	}

	tpri = DISP_PRIO(tp);
	if (ncpus == 1)
		cp = tp->t_cpu;
	else if (!tp->t_bound_cpu && !tp->t_weakbound_cpu) {
		if (tpri >= kpqpri) {
			setkpdq(tp, SETKP_FRONT);
			return;
		}
		cp = tp->t_cpu;
		if (tp->t_cpupart == cp->cpu_part) {
			/*
			 * If we are of higher or equal priority than
			 * the highest priority runnable thread of
			 * the current CPU, just pick this CPU.  Otherwise
			 * Let cpu_choose() select the CPU.  If this cpu
			 * is the target of an offline request then do not
			 * pick it - a thread_nomigrate() on the in motion
			 * cpu relies on this when it forces a preempt.
			 */
			if (tpri < cp->cpu_disp->disp_maxrunpri ||
			    cp == cpu_inmotion)
				cp = cpu_choose(tp, tpri);
		} else {
			/*
			 * Migrate to a cpu in the new partition.
			 */
			cp = disp_lowpri_cpu(tp->t_cpupart->cp_cpulist,
			    tp->t_lpl, tp->t_pri, NULL);
		}
		bound = 0;
		ASSERT((cp->cpu_flags & CPU_QUIESCED) == 0);
	} else {
		/*
		 * It is possible that t_weakbound_cpu != t_bound_cpu (for
		 * a short time until weak binding that existed when the
		 * strong binding was established has dropped) so we must
		 * favour weak binding over strong.
		 */
		cp = tp->t_weakbound_cpu ?
		    tp->t_weakbound_cpu : tp->t_bound_cpu;
		bound = 1;
	}
	dp = cp->cpu_disp;
	disp_lock_enter_high(&dp->disp_lock);

	TRACE_2(TR_FAC_DISP, TR_FRONTQ, "frontq:pri %d tid %p", tpri, tp);
	DTRACE_SCHED3(enqueue, kthread_t *, tp, disp_t *, dp, int, 1);

#ifndef NPROBE
	/* Kernel probe */
	if (tnf_tracing_active)
		tnf_thread_queue(tp, cp, tpri);
#endif /* NPROBE */

	ASSERT(tpri >= 0 && tpri < dp->disp_npri);

	THREAD_RUN(tp, &dp->disp_lock);		/* set TS_RUN state and lock */
	tp->t_disp_queue = dp;

	dq = &dp->disp_q[tpri];
	dp->disp_nrunnable++;
	if (!bound)
		dp->disp_steal = 0;
	membar_enter();

	if (dq->dq_sruncnt++ != 0) {
		ASSERT(dq->dq_last != NULL);
		tp->t_link = dq->dq_first;
		dq->dq_first = tp;
	} else {
		ASSERT(dq->dq_last == NULL);
		ASSERT(dq->dq_first == NULL);
		tp->t_link = NULL;
		dq->dq_first = dq->dq_last = tp;
		BT_SET(dp->disp_qactmap, tpri);
		if (tpri > dp->disp_maxrunpri) {
			dp->disp_maxrunpri = tpri;
			membar_enter();
			cpu_resched(cp, tpri);
		}
	}

	if (!bound && tpri > dp->disp_max_unbound_pri) {
		if (tp == curthread && dp->disp_max_unbound_pri == -1 &&
		    cp == CPU) {
			/*
			 * If there are no other unbound threads on the
			 * run queue, don't allow other CPUs to steal
			 * this thread while we are in the middle of a
			 * context switch. We may just switch to it
			 * again right away. CPU_DISP_DONTSTEAL is cleared
			 * in swtch and swtch_to.
			 */
			cp->cpu_disp_flags |= CPU_DISP_DONTSTEAL;
		}
		dp->disp_max_unbound_pri = tpri;
	}
	(*disp_enq_thread)(cp, bound);
}

/*
 * Put a high-priority unbound thread on the kp queue
 */
static void
setkpdq(kthread_t *tp, int borf)
{
	dispq_t	*dq;
	disp_t	*dp;
	cpu_t	*cp;
	pri_t	tpri;

	tpri = DISP_PRIO(tp);

	dp = &tp->t_cpupart->cp_kp_queue;
	disp_lock_enter_high(&dp->disp_lock);

	TRACE_2(TR_FAC_DISP, TR_FRONTQ, "frontq:pri %d tid %p", tpri, tp);

	ASSERT(tpri >= 0 && tpri < dp->disp_npri);
	DTRACE_SCHED3(enqueue, kthread_t *, tp, disp_t *, dp, int, borf);
	THREAD_RUN(tp, &dp->disp_lock);		/* set t_state to TS_RUN */
	tp->t_disp_queue = dp;
	dp->disp_nrunnable++;
	dq = &dp->disp_q[tpri];

	if (dq->dq_sruncnt++ != 0) {
		if (borf == SETKP_BACK) {
			ASSERT(dq->dq_first != NULL);
			tp->t_link = NULL;
			dq->dq_last->t_link = tp;
			dq->dq_last = tp;
		} else {
			ASSERT(dq->dq_last != NULL);
			tp->t_link = dq->dq_first;
			dq->dq_first = tp;
		}
	} else {
		if (borf == SETKP_BACK) {
			ASSERT(dq->dq_first == NULL);
			ASSERT(dq->dq_last == NULL);
			dq->dq_first = dq->dq_last = tp;
		} else {
			ASSERT(dq->dq_last == NULL);
			ASSERT(dq->dq_first == NULL);
			tp->t_link = NULL;
			dq->dq_first = dq->dq_last = tp;
		}
		BT_SET(dp->disp_qactmap, tpri);
		if (tpri > dp->disp_max_unbound_pri)
			dp->disp_max_unbound_pri = tpri;
		if (tpri > dp->disp_maxrunpri) {
			dp->disp_maxrunpri = tpri;
			membar_enter();
		}
	}

	cp = tp->t_cpu;
	if (tp->t_cpupart != cp->cpu_part) {
		/* migrate to a cpu in the new partition */
		cp = tp->t_cpupart->cp_cpulist;
	}
	cp = disp_lowpri_cpu(cp, tp->t_lpl, tp->t_pri, NULL);
	disp_lock_enter_high(&cp->cpu_disp->disp_lock);
	ASSERT((cp->cpu_flags & CPU_QUIESCED) == 0);

#ifndef NPROBE
	/* Kernel probe */
	if (tnf_tracing_active)
		tnf_thread_queue(tp, cp, tpri);
#endif /* NPROBE */

	if (cp->cpu_chosen_level < tpri)
		cp->cpu_chosen_level = tpri;
	cpu_resched(cp, tpri);
	disp_lock_exit_high(&cp->cpu_disp->disp_lock);
	(*disp_enq_thread)(cp, 0);
}

/*
 * Remove a thread from the dispatcher queue if it is on it.
 * It is not an error if it is not found but we return whether
 * or not it was found in case the caller wants to check.
 */
int
dispdeq(kthread_t *tp)
{
	disp_t		*dp;
	dispq_t		*dq;
	kthread_t	*rp;
	kthread_t	*trp;
	kthread_t	**ptp;
	int		tpri;

	ASSERT(THREAD_LOCK_HELD(tp));

	if (tp->t_state != TS_RUN)
		return (0);

	/*
	 * The thread is "swapped" or is on the swap queue and
	 * hence no longer on the run queue, so return true.
	 */
	if ((tp->t_schedflag & (TS_LOAD | TS_ON_SWAPQ)) != TS_LOAD)
		return (1);

	tpri = DISP_PRIO(tp);
	dp = tp->t_disp_queue;
	ASSERT(tpri < dp->disp_npri);
	dq = &dp->disp_q[tpri];
	ptp = &dq->dq_first;
	rp = *ptp;
	trp = NULL;

	ASSERT(dq->dq_last == NULL || dq->dq_last->t_link == NULL);

	/*
	 * Search for thread in queue.
	 * Double links would simplify this at the expense of disp/setrun.
	 */
	while (rp != tp && rp != NULL) {
		trp = rp;
		ptp = &trp->t_link;
		rp = trp->t_link;
	}

	if (rp == NULL) {
		panic("dispdeq: thread not on queue");
	}

	DTRACE_SCHED2(dequeue, kthread_t *, tp, disp_t *, dp);

	/*
	 * Found it so remove it from queue.
	 */
	if ((*ptp = rp->t_link) == NULL)
		dq->dq_last = trp;

	dp->disp_nrunnable--;
	if (--dq->dq_sruncnt == 0) {
		dp->disp_qactmap[tpri >> BT_ULSHIFT] &= ~BT_BIW(tpri);
		if (dp->disp_nrunnable == 0) {
			dp->disp_max_unbound_pri = -1;
			dp->disp_maxrunpri = -1;
		} else if (tpri == dp->disp_maxrunpri) {
			int ipri;

			ipri = bt_gethighbit(dp->disp_qactmap,
			    dp->disp_maxrunpri >> BT_ULSHIFT);
			if (ipri < dp->disp_max_unbound_pri)
				dp->disp_max_unbound_pri = ipri;
			dp->disp_maxrunpri = ipri;
		}
	}
	tp->t_link = NULL;
	THREAD_TRANSITION(tp);		/* put in intermediate state */
	return (1);
}


/*
 * dq_sruninc and dq_srundec are public functions for
 * incrementing/decrementing the sruncnts when a thread on
 * a dispatcher queue is made schedulable/unschedulable by
 * resetting the TS_LOAD flag.
 *
 * The caller MUST have the thread lock and therefore the dispatcher
 * queue lock so that the operation which changes
 * the flag, the operation that checks the status of the thread to
 * determine if it's on a disp queue AND the call to this function
 * are one atomic operation with respect to interrupts.
 */

/*
 * Called by sched AFTER TS_LOAD flag is set on a swapped, runnable thread.
 */
void
dq_sruninc(kthread_t *t)
{
	ASSERT(t->t_state == TS_RUN);
	ASSERT(t->t_schedflag & TS_LOAD);

	THREAD_TRANSITION(t);
	setfrontdq(t);
}

/*
 * See comment on calling conventions above.
 * Called by sched BEFORE TS_LOAD flag is cleared on a runnable thread.
 */
void
dq_srundec(kthread_t *t)
{
	ASSERT(t->t_schedflag & TS_LOAD);

	(void) dispdeq(t);
	disp_swapped_enq(t);
}

/*
 * Change the dispatcher lock of thread to the "swapped_lock"
 * and return with thread lock still held.
 *
 * Called with thread_lock held, in transition state, and at high spl.
 */
void
disp_swapped_enq(kthread_t *tp)
{
	ASSERT(THREAD_LOCK_HELD(tp));
	ASSERT(tp->t_schedflag & TS_LOAD);

	switch (tp->t_state) {
	case TS_RUN:
		disp_lock_enter_high(&swapped_lock);
		THREAD_SWAP(tp, &swapped_lock);	/* set TS_RUN state and lock */
		break;
	case TS_ONPROC:
		disp_lock_enter_high(&swapped_lock);
		THREAD_TRANSITION(tp);
		wake_sched_sec = 1;		/* tell clock to wake sched */
		THREAD_SWAP(tp, &swapped_lock);	/* set TS_RUN state and lock */
		break;
	default:
		panic("disp_swapped: tp: %p bad t_state", (void *)tp);
	}
}

/*
 * This routine is called by setbackdq/setfrontdq if the thread is
 * not loaded or loaded and on the swap queue.
 *
 * Thread state TS_SLEEP implies that a swapped thread
 * has been woken up and needs to be swapped in by the swapper.
 *
 * Thread state TS_RUN, it implies that the priority of a swapped
 * thread is being increased by scheduling class (e.g. ts_update).
 */
static void
disp_swapped_setrun(kthread_t *tp)
{
	ASSERT(THREAD_LOCK_HELD(tp));
	ASSERT((tp->t_schedflag & (TS_LOAD | TS_ON_SWAPQ)) != TS_LOAD);

	switch (tp->t_state) {
	case TS_SLEEP:
		disp_lock_enter_high(&swapped_lock);
		/*
		 * Wakeup sched immediately (i.e., next tick) if the
		 * thread priority is above maxclsyspri.
		 */
		if (DISP_PRIO(tp) > maxclsyspri)
			wake_sched = 1;
		else
			wake_sched_sec = 1;
		THREAD_RUN(tp, &swapped_lock); /* set TS_RUN state and lock */
		break;
	case TS_RUN:				/* called from ts_update */
		break;
	default:
		panic("disp_swapped_setrun: tp: %p bad t_state", tp);
	}
}


/*
 *	Make a thread give up its processor.  Find the processor on
 *	which this thread is executing, and have that processor
 *	preempt.
 */
void
cpu_surrender(kthread_t *tp)
{
	cpu_t	*cpup;
	int	max_pri;
	int	max_run_pri;
	klwp_t	*lwp;

	ASSERT(THREAD_LOCK_HELD(tp));

	if (tp->t_state != TS_ONPROC)
		return;
	cpup = tp->t_disp_queue->disp_cpu;	/* CPU thread dispatched to */
	max_pri = cpup->cpu_disp->disp_maxrunpri; /* best pri of that CPU */
	max_run_pri = CP_MAXRUNPRI(cpup->cpu_part);
	if (max_pri < max_run_pri)
		max_pri = max_run_pri;

	cpup->cpu_runrun = 1;
	if (max_pri >= kpreemptpri && cpup->cpu_kprunrun == 0) {
		cpup->cpu_kprunrun = 1;
	}

	/*
	 * Propagate cpu_runrun, and cpu_kprunrun to global visibility.
	 */
	membar_enter();

	DTRACE_SCHED1(surrender, kthread_t *, tp);

	/*
	 * Make the target thread take an excursion through trap()
	 * to do preempt() (unless we're already in trap or post_syscall,
	 * calling cpu_surrender via CL_TRAPRET).
	 */
	if (tp != curthread || (lwp = tp->t_lwp) == NULL ||
	    lwp->lwp_state != LWP_USER) {
		aston(tp);
		if (cpup != CPU)
			poke_cpu(cpup->cpu_id);
	}
	TRACE_2(TR_FAC_DISP, TR_CPU_SURRENDER,
	    "cpu_surrender:tid %p cpu %p", tp, cpup);
}


/*
 * Commit to and ratify a scheduling decision
 */
/*ARGSUSED*/
static kthread_t *
disp_ratify(kthread_t *tp, disp_t *kpq)
{
	pri_t	tpri, maxpri;
	pri_t	maxkpri;
	cpu_t	*cpup;

	ASSERT(tp != NULL);
	/*
	 * Commit to, then ratify scheduling decision
	 */
	cpup = CPU;
	if (cpup->cpu_runrun != 0)
		cpup->cpu_runrun = 0;
	if (cpup->cpu_kprunrun != 0)
		cpup->cpu_kprunrun = 0;
	if (cpup->cpu_chosen_level != -1)
		cpup->cpu_chosen_level = -1;
	membar_enter();
	tpri = DISP_PRIO(tp);
	maxpri = cpup->cpu_disp->disp_maxrunpri;
	maxkpri = kpq->disp_maxrunpri;
	if (maxpri < maxkpri)
		maxpri = maxkpri;
	if (tpri < maxpri) {
		/*
		 * should have done better
		 * put this one back and indicate to try again
		 */
		cpup->cpu_dispthread = curthread;	/* fixup dispthread */
		cpup->cpu_dispatch_pri = DISP_PRIO(curthread);
		thread_lock_high(tp);
		THREAD_TRANSITION(tp);
		setfrontdq(tp);
		thread_unlock_nopreempt(tp);

		tp = NULL;
	}
	return (tp);
}

/*
 * See if there is any work on the dispatcher queue for other CPUs.
 * If there is, dequeue the best thread and return.
 */
static kthread_t *
disp_getwork(cpu_t *cp)
{
	cpu_t		*ocp;		/* other CPU */
	cpu_t		*ocp_start;
	cpu_t		*tcp;		/* target local CPU */
	kthread_t	*tp;
	kthread_t	*retval = NULL;
	pri_t		maxpri;
	disp_t		*kpq;		/* kp queue for this partition */
	lpl_t		*lpl, *lpl_leaf;
	int		hint, leafidx;
	hrtime_t	stealtime;

	maxpri = -1;
	tcp = NULL;

	kpq = &cp->cpu_part->cp_kp_queue;
	while (kpq->disp_maxrunpri >= 0) {
		/*
		 * Try to take a thread from the kp_queue.
		 */
		tp = (disp_getbest(kpq));
		if (tp)
			return (disp_ratify(tp, kpq));
	}

	kpreempt_disable();		/* protect the cpu_active list */

	/*
	 * Try to find something to do on another CPU's run queue.
	 * Loop through all other CPUs looking for the one with the highest
	 * priority unbound thread.
	 *
	 * On NUMA machines, the partition's CPUs are consulted in order of
	 * distance from the current CPU. This way, the first available
	 * work found is also the closest, and will suffer the least
	 * from being migrated.
	 */
	lpl = lpl_leaf = cp->cpu_lpl;
	hint = leafidx = 0;

	/*
	 * This loop traverses the lpl hierarchy. Higher level lpls represent
	 * broader levels of locality
	 */
	do {
		/* This loop iterates over the lpl's leaves */
		do {
			if (lpl_leaf != cp->cpu_lpl)
				ocp = lpl_leaf->lpl_cpus;
			else
				ocp = cp->cpu_next_lpl;

			/* This loop iterates over the CPUs in the leaf */
			ocp_start = ocp;
			do {
				pri_t pri;

				ASSERT(CPU_ACTIVE(ocp));

				/*
				 * End our stroll around the partition if:
				 *
				 * - Something became runnable on the local
				 *	queue
				 *
				 * - We're at the broadest level of locality and
				 *   we happen across another idle CPU. At the
				 *   highest level of locality, all CPUs will
				 *   walk the partition's CPUs in the same
				 *   order, so we can end our stroll taking
				 *   comfort in knowing the other idle CPU is
				 *   already covering the next portion of the
				 *   list.
				 */
				if (cp->cpu_disp->disp_nrunnable != 0)
					break;
				if (ocp->cpu_dispatch_pri == -1) {
					if (ocp->cpu_disp_flags &
					    CPU_DISP_HALTED)
						continue;
					else if (lpl->lpl_parent == NULL)
						break;
				}

				/*
				 * If there's only one thread and the CPU
				 * is in the middle of a context switch,
				 * or it's currently running the idle thread,
				 * don't steal it.
				 */
				if ((ocp->cpu_disp_flags &
					CPU_DISP_DONTSTEAL) &&
				    ocp->cpu_disp->disp_nrunnable == 1)
					continue;

				pri = ocp->cpu_disp->disp_max_unbound_pri;
				if (pri > maxpri) {
					/*
					 * Don't steal threads that we attempted
					 * to be stolen very recently until
					 * they're ready to be stolen again.
					 */
					stealtime = ocp->cpu_disp->disp_steal;
					if (stealtime == 0 ||
					    stealtime - gethrtime() <= 0) {
						maxpri = pri;
						tcp = ocp;
					} else {
						/*
						 * Don't update tcp, just set
						 * the retval to T_DONTSTEAL, so
						 * that if no acceptable CPUs
						 * are found the return value
						 * will be T_DONTSTEAL rather
						 * then NULL.
						 */
						retval = T_DONTSTEAL;
					}
				}
			} while ((ocp = ocp->cpu_next_lpl) != ocp_start);

			if ((lpl_leaf = lpl->lpl_rset[++leafidx]) == NULL) {
				leafidx = 0;
				lpl_leaf = lpl->lpl_rset[leafidx];
			}
		} while (leafidx != hint);

		hint = leafidx = lpl->lpl_hint;
		if ((lpl = lpl->lpl_parent) != NULL)
			lpl_leaf = lpl->lpl_rset[hint];
	} while (!tcp && lpl);

	kpreempt_enable();

	/*
	 * If another queue looks good, and there is still nothing on
	 * the local queue, try to transfer one or more threads
	 * from it to our queue.
	 */
	if (tcp && cp->cpu_disp->disp_nrunnable == 0) {
		tp = disp_getbest(tcp->cpu_disp);
		if (tp == NULL || tp == T_DONTSTEAL)
			return (tp);
		return (disp_ratify(tp, kpq));
	}
	return (retval);
}


/*
 * disp_fix_unbound_pri()
 *	Determines the maximum priority of unbound threads on the queue.
 *	The priority is kept for the queue, but is only increased, never
 *	reduced unless some CPU is looking for something on that queue.
 *
 *	The priority argument is the known upper limit.
 *
 *	Perhaps this should be kept accurately, but that probably means
 *	separate bitmaps for bound and unbound threads.  Since only idled
 *	CPUs will have to do this recalculation, it seems better this way.
 */
static void
disp_fix_unbound_pri(disp_t *dp, pri_t pri)
{
	kthread_t	*tp;
	dispq_t		*dq;
	ulong_t		*dqactmap = dp->disp_qactmap;
	ulong_t		mapword;
	int		wx;

	ASSERT(DISP_LOCK_HELD(&dp->disp_lock));

	ASSERT(pri >= 0);			/* checked by caller */

	/*
	 * Start the search at the next lowest priority below the supplied
	 * priority.  This depends on the bitmap implementation.
	 */
	do {
		wx = pri >> BT_ULSHIFT;		/* index of word in map */

		/*
		 * Form mask for all lower priorities in the word.
		 */
		mapword = dqactmap[wx] & (BT_BIW(pri) - 1);

		/*
		 * Get next lower active priority.
		 */
		if (mapword != 0) {
			pri = (wx << BT_ULSHIFT) + highbit(mapword) - 1;
		} else if (wx > 0) {
			pri = bt_gethighbit(dqactmap, wx - 1); /* sign extend */
			if (pri < 0)
				break;
		} else {
			pri = -1;
			break;
		}

		/*
		 * Search the queue for unbound, runnable threads.
		 */
		dq = &dp->disp_q[pri];
		tp = dq->dq_first;

		while (tp && (tp->t_bound_cpu || tp->t_weakbound_cpu)) {
			tp = tp->t_link;
		}

		/*
		 * If a thread was found, set the priority and return.
		 */
	} while (tp == NULL);

	/*
	 * pri holds the maximum unbound thread priority or -1.
	 */
	if (dp->disp_max_unbound_pri != pri)
		dp->disp_max_unbound_pri = pri;
}

/*
 * disp_adjust_unbound_pri() - thread is becoming unbound, so we should
 * 	check if the CPU to which is was previously bound should have
 * 	its disp_max_unbound_pri increased.
 */
void
disp_adjust_unbound_pri(kthread_t *tp)
{
	disp_t *dp;
	pri_t tpri;

	ASSERT(THREAD_LOCK_HELD(tp));

	/*
	 * Don't do anything if the thread is not bound, or
	 * currently not runnable or swapped out.
	 */
	if (tp->t_bound_cpu == NULL ||
	    tp->t_state != TS_RUN ||
	    tp->t_schedflag & TS_ON_SWAPQ)
		return;

	tpri = DISP_PRIO(tp);
	dp = tp->t_bound_cpu->cpu_disp;
	ASSERT(tpri >= 0 && tpri < dp->disp_npri);
	if (tpri > dp->disp_max_unbound_pri)
		dp->disp_max_unbound_pri = tpri;
}

/*
 * disp_getbest()
 *   De-queue the highest priority unbound runnable thread.
 *   Returns with the thread unlocked and onproc but at splhigh (like disp()).
 *   Returns NULL if nothing found.
 *   Returns T_DONTSTEAL if the thread was not stealable.
 *   so that the caller will try again later.
 *
 *   Passed a pointer to a dispatch queue not associated with this CPU, and
 *   its type.
 */
static kthread_t *
disp_getbest(disp_t *dp)
{
	kthread_t	*tp;
	dispq_t		*dq;
	pri_t		pri;
	cpu_t		*cp, *tcp;
	boolean_t	allbound;

	disp_lock_enter(&dp->disp_lock);

	/*
	 * If there is nothing to run, or the CPU is in the middle of a
	 * context switch of the only thread, return NULL.
	 */
	tcp = dp->disp_cpu;
	cp = CPU;
	pri = dp->disp_max_unbound_pri;
	if (pri == -1 ||
	    (tcp != NULL && (tcp->cpu_disp_flags & CPU_DISP_DONTSTEAL) &&
	    tcp->cpu_disp->disp_nrunnable == 1)) {
		disp_lock_exit_nopreempt(&dp->disp_lock);
		return (NULL);
	}

	dq = &dp->disp_q[pri];


	/*
	 * Assume that all threads are bound on this queue, and change it
	 * later when we find out that it is not the case.
	 */
	allbound = B_TRUE;
	for (tp = dq->dq_first; tp != NULL; tp = tp->t_link) {
		hrtime_t now, nosteal, rqtime;
		chip_type_t chtype;
		chip_t *chip;

		/*
		 * Skip over bound threads which could be here even
		 * though disp_max_unbound_pri indicated this level.
		 */
		if (tp->t_bound_cpu || tp->t_weakbound_cpu)
			continue;

		/*
		 * We've got some unbound threads on this queue, so turn
		 * the allbound flag off now.
		 */
		allbound = B_FALSE;

		/*
		 * The thread is a candidate for stealing from its run queue. We
		 * don't want to steal threads that became runnable just a
		 * moment ago. This improves CPU affinity for threads that get
		 * preempted for short periods of time and go back on the run
		 * queue.
		 *
		 * We want to let it stay on its run queue if it was only placed
		 * there recently and it was running on the same CPU before that
		 * to preserve its cache investment. For the thread to remain on
		 * its run queue, ALL of the following conditions must be
		 * satisfied:
		 *
		 * - the disp queue should not be the kernel preemption queue
		 * - delayed idle stealing should not be disabled
		 * - nosteal_nsec should be non-zero
		 * - it should run with user priority
		 * - it should be on the run queue of the CPU where it was
		 *   running before being placed on the run queue
		 * - it should be the only thread on the run queue (to prevent
		 *   extra scheduling latency for other threads)
		 * - it should sit on the run queue for less than per-chip
		 *   nosteal interval or global nosteal interval
		 * - in case of CPUs with shared cache it should sit in a run
		 *   queue of a CPU from a different chip
		 *
		 * The checks are arranged so that the ones that are faster are
		 * placed earlier.
		 */
		if (tcp == NULL ||
		    pri >= minclsyspri ||
		    tp->t_cpu != tcp)
			break;

		/*
		 * Steal immediately if the chip has shared cache and we are
		 * sharing the chip with the target thread's CPU.
		 */
		chip = tcp->cpu_chip;
		chtype = chip->chip_type;
		if ((chtype == CHIP_SMT || chtype == CHIP_CMP_SHARED_CACHE) &&
		    chip == cp->cpu_chip)
			break;

		/*
		 * Get the value of nosteal interval either from nosteal_nsec
		 * global variable or from a value specified by a chip
		 */
		nosteal = nosteal_nsec ? nosteal_nsec : chip->chip_nosteal;
		if (nosteal == 0 || nosteal == NOSTEAL_DISABLED)
			break;

		/*
		 * Calculate time spent sitting on run queue
		 */
		now = gethrtime_unscaled();
		rqtime = now - tp->t_waitrq;
		scalehrtime(&rqtime);

		/*
		 * Steal immediately if the time spent on this run queue is more
		 * than allowed nosteal delay.
		 *
		 * Negative rqtime check is needed here to avoid infinite
		 * stealing delays caused by unlikely but not impossible
		 * drifts between CPU times on different CPUs.
		 */
		if (rqtime > nosteal || rqtime < 0)
			break;

		DTRACE_PROBE4(nosteal, kthread_t *, tp,
		    cpu_t *, tcp, cpu_t *, cp, hrtime_t, rqtime);
		scalehrtime(&now);
		/*
		 * Calculate when this thread becomes stealable
		 */
		now += (nosteal - rqtime);

		/*
		 * Calculate time when some thread becomes stealable
		 */
		if (now < dp->disp_steal)
			dp->disp_steal = now;
	}

	/*
	 * If there were no unbound threads on this queue, find the queue
	 * where they are and then return later. The value of
	 * disp_max_unbound_pri is not always accurate because it isn't
	 * reduced until another idle CPU looks for work.
	 */
	if (allbound)
		disp_fix_unbound_pri(dp, pri);

	/*
	 * If we reached the end of the queue and found no unbound threads
	 * then return NULL so that other CPUs will be considered.  If there
	 * are unbound threads but they cannot yet be stolen, then
	 * return T_DONTSTEAL and try again later.
	 */
	if (tp == NULL) {
		disp_lock_exit_nopreempt(&dp->disp_lock);
		return (allbound ? NULL : T_DONTSTEAL);
	}

	/*
	 * Found a runnable, unbound thread, so remove it from queue.
	 * dispdeq() requires that we have the thread locked, and we do,
	 * by virtue of holding the dispatch queue lock.  dispdeq() will
	 * put the thread in transition state, thereby dropping the dispq
	 * lock.
	 */

#ifdef DEBUG
	{
		int	thread_was_on_queue;

		thread_was_on_queue = dispdeq(tp);	/* drops disp_lock */
		ASSERT(thread_was_on_queue);
	}

#else /* DEBUG */
	(void) dispdeq(tp);			/* drops disp_lock */
#endif /* DEBUG */

	/*
	 * Reset the disp_queue steal time - we do not know what is the smallest
	 * value across the queue is.
	 */
	dp->disp_steal = 0;

	tp->t_schedflag |= TS_DONT_SWAP;

	/*
	 * Setup thread to run on the current CPU.
	 */
	tp->t_disp_queue = cp->cpu_disp;

	cp->cpu_dispthread = tp;		/* protected by spl only */
	cp->cpu_dispatch_pri = pri;
	ASSERT(pri == DISP_PRIO(tp));

	DTRACE_PROBE3(steal, kthread_t *, tp, cpu_t *, tcp, cpu_t *, cp);

	thread_onproc(tp, cp);			/* set t_state to TS_ONPROC */

	/*
	 * Return with spl high so that swtch() won't need to raise it.
	 * The disp_lock was dropped by dispdeq().
	 */

	return (tp);
}

/*
 * disp_bound_common() - common routine for higher level functions
 *	that check for bound threads under certain conditions.
 *	If 'threadlistsafe' is set then there is no need to acquire
 *	pidlock to stop the thread list from changing (eg, if
 *	disp_bound_* is called with cpus paused).
 */
static int
disp_bound_common(cpu_t *cp, int threadlistsafe, int flag)
{
	int		found = 0;
	kthread_t	*tp;

	ASSERT(flag);

	if (!threadlistsafe)
		mutex_enter(&pidlock);
	tp = curthread;		/* faster than allthreads */
	do {
		if (tp->t_state != TS_FREE) {
			/*
			 * If an interrupt thread is busy, but the
			 * caller doesn't care (i.e. BOUND_INTR is off),
			 * then just ignore it and continue through.
			 */
			if ((tp->t_flag & T_INTR_THREAD) &&
			    !(flag & BOUND_INTR))
				continue;

			/*
			 * Skip the idle thread for the CPU
			 * we're about to set offline.
			 */
			if (tp == cp->cpu_idle_thread)
				continue;

			/*
			 * Skip the pause thread for the CPU
			 * we're about to set offline.
			 */
			if (tp == cp->cpu_pause_thread)
				continue;

			if ((flag & BOUND_CPU) &&
			    (tp->t_bound_cpu == cp ||
			    tp->t_bind_cpu == cp->cpu_id ||
			    tp->t_weakbound_cpu == cp)) {
				found = 1;
				break;
			}

			if ((flag & BOUND_PARTITION) &&
			    (tp->t_cpupart == cp->cpu_part)) {
				found = 1;
				break;
			}
		}
	} while ((tp = tp->t_next) != curthread && found == 0);
	if (!threadlistsafe)
		mutex_exit(&pidlock);
	return (found);
}

/*
 * disp_bound_threads - return nonzero if threads are bound to the processor.
 *	Called infrequently.  Keep this simple.
 *	Includes threads that are asleep or stopped but not onproc.
 */
int
disp_bound_threads(cpu_t *cp, int threadlistsafe)
{
	return (disp_bound_common(cp, threadlistsafe, BOUND_CPU));
}

/*
 * disp_bound_anythreads - return nonzero if _any_ threads are bound
 * to the given processor, including interrupt threads.
 */
int
disp_bound_anythreads(cpu_t *cp, int threadlistsafe)
{
	return (disp_bound_common(cp, threadlistsafe, BOUND_CPU | BOUND_INTR));
}

/*
 * disp_bound_partition - return nonzero if threads are bound to the same
 * partition as the processor.
 *	Called infrequently.  Keep this simple.
 *	Includes threads that are asleep or stopped but not onproc.
 */
int
disp_bound_partition(cpu_t *cp, int threadlistsafe)
{
	return (disp_bound_common(cp, threadlistsafe, BOUND_PARTITION));
}

/*
 * disp_cpu_inactive - make a CPU inactive by moving all of its unbound
 * threads to other CPUs.
 */
void
disp_cpu_inactive(cpu_t *cp)
{
	kthread_t	*tp;
	disp_t		*dp = cp->cpu_disp;
	dispq_t		*dq;
	pri_t		pri;
	int		wasonq;

	disp_lock_enter(&dp->disp_lock);
	while ((pri = dp->disp_max_unbound_pri) != -1) {
		dq = &dp->disp_q[pri];
		tp = dq->dq_first;

		/*
		 * Skip over bound threads.
		 */
		while (tp != NULL && tp->t_bound_cpu != NULL) {
			tp = tp->t_link;
		}

		if (tp == NULL) {
			/* disp_max_unbound_pri must be inaccurate, so fix it */
			disp_fix_unbound_pri(dp, pri);
			continue;
		}

		wasonq = dispdeq(tp);		/* drops disp_lock */
		ASSERT(wasonq);
		ASSERT(tp->t_weakbound_cpu == NULL);

		setbackdq(tp);
		/*
		 * Called from cpu_offline:
		 *
		 * cp has already been removed from the list of active cpus
		 * and tp->t_cpu has been changed so there is no risk of
		 * tp ending up back on cp.
		 *
		 * Called from cpupart_move_cpu:
		 *
		 * The cpu has moved to a new cpupart.  Any threads that
		 * were on it's dispatch queues before the move remain
		 * in the old partition and can't run in the new partition.
		 */
		ASSERT(tp->t_cpu != cp);
		thread_unlock(tp);

		disp_lock_enter(&dp->disp_lock);
	}
	disp_lock_exit(&dp->disp_lock);
}

/*
 * disp_lowpri_cpu - find CPU running the lowest priority thread.
 *	The hint passed in is used as a starting point so we don't favor
 *	CPU 0 or any other CPU.  The caller should pass in the most recently
 *	used CPU for the thread.
 *
 *	The lgroup and priority are used to determine the best CPU to run on
 *	in a NUMA machine.  The lgroup specifies which CPUs are closest while
 *	the thread priority will indicate whether the thread will actually run
 *	there.  To pick the best CPU, the CPUs inside and outside of the given
 *	lgroup which are running the lowest priority threads are found.  The
 *	remote CPU is chosen only if the thread will not run locally on a CPU
 *	within the lgroup, but will run on the remote CPU. If the thread
 *	cannot immediately run on any CPU, the best local CPU will be chosen.
 *
 *	The lpl specified also identifies the cpu partition from which
 *	disp_lowpri_cpu should select a CPU.
 *
 *	curcpu is used to indicate that disp_lowpri_cpu is being called on
 *      behalf of the current thread. (curthread is looking for a new cpu)
 *      In this case, cpu_dispatch_pri for this thread's cpu should be
 *      ignored.
 *
 *      If a cpu is the target of an offline request then try to avoid it.
 *
 *	This function must be called at either high SPL, or with preemption
 *	disabled, so that the "hint" CPU cannot be removed from the online
 *	CPU list while we are traversing it.
 */
cpu_t *
disp_lowpri_cpu(cpu_t *hint, lpl_t *lpl, pri_t tpri, cpu_t *curcpu)
{
	cpu_t	*bestcpu;
	cpu_t	*besthomecpu;
	cpu_t   *cp, *cpstart;

	pri_t   bestpri;
	pri_t   cpupri;

	klgrpset_t	done;
	klgrpset_t	cur_set;

	lpl_t		*lpl_iter, *lpl_leaf;
	int		i;

	/*
	 * Scan for a CPU currently running the lowest priority thread.
	 * Cannot get cpu_lock here because it is adaptive.
	 * We do not require lock on CPU list.
	 */
	ASSERT(hint != NULL);
	ASSERT(lpl != NULL);
	ASSERT(lpl->lpl_ncpu > 0);

	/*
	 * First examine local CPUs. Note that it's possible the hint CPU
	 * passed in in remote to the specified home lgroup. If our priority
	 * isn't sufficient enough such that we can run immediately at home,
	 * then examine CPUs remote to our home lgroup.
	 * We would like to give preference to CPUs closest to "home".
	 * If we can't find a CPU where we'll run at a given level
	 * of locality, we expand our search to include the next level.
	 */
	bestcpu = besthomecpu = NULL;
	klgrpset_clear(done);
	/* start with lpl we were passed */

	lpl_iter = lpl;

	do {

		bestpri = SHRT_MAX;
		klgrpset_clear(cur_set);

		for (i = 0; i < lpl_iter->lpl_nrset; i++) {
			lpl_leaf = lpl_iter->lpl_rset[i];
			if (klgrpset_ismember(done, lpl_leaf->lpl_lgrpid))
				continue;

			klgrpset_add(cur_set, lpl_leaf->lpl_lgrpid);

			if (hint->cpu_lpl == lpl_leaf)
				cp = cpstart = hint;
			else
				cp = cpstart = lpl_leaf->lpl_cpus;

			do {
				if (cp == curcpu)
					cpupri = -1;
				else if (cp == cpu_inmotion)
					cpupri = SHRT_MAX;
				else
					cpupri = cp->cpu_dispatch_pri;
				if (cp->cpu_disp->disp_maxrunpri > cpupri)
					cpupri = cp->cpu_disp->disp_maxrunpri;
				if (cp->cpu_chosen_level > cpupri)
					cpupri = cp->cpu_chosen_level;
				if (cpupri < bestpri) {
					if (CPU_IDLING(cpupri)) {
						ASSERT((cp->cpu_flags &
						    CPU_QUIESCED) == 0);
						return (cp);
					}
					bestcpu = cp;
					bestpri = cpupri;
				}
			} while ((cp = cp->cpu_next_lpl) != cpstart);
		}

		if (bestcpu && (tpri > bestpri)) {
			ASSERT((bestcpu->cpu_flags & CPU_QUIESCED) == 0);
			return (bestcpu);
		}
		if (besthomecpu == NULL)
			besthomecpu = bestcpu;
		/*
		 * Add the lgrps we just considered to the "done" set
		 */
		klgrpset_or(done, cur_set);

	} while ((lpl_iter = lpl_iter->lpl_parent) != NULL);

	/*
	 * The specified priority isn't high enough to run immediately
	 * anywhere, so just return the best CPU from the home lgroup.
	 */
	ASSERT((besthomecpu->cpu_flags & CPU_QUIESCED) == 0);
	return (besthomecpu);
}

/*
 * This routine provides the generic idle cpu function for all processors.
 * If a processor has some specific code to execute when idle (say, to stop
 * the pipeline and save power) then that routine should be defined in the
 * processors specific code (module_xx.c) and the global variable idle_cpu
 * set to that function.
 */
static void
generic_idle_cpu(void)
{
}

/*ARGSUSED*/
static void
generic_enq_thread(cpu_t *cpu, int bound)
{
}

/*
 * Select a CPU for this thread to run on.  Choose t->t_cpu unless:
 *	- t->t_cpu is not in this thread's assigned lgrp
 *	- the time since the thread last came off t->t_cpu exceeds the
 *	  rechoose time for this cpu (ignore this if t is curthread in
 *	  which case it's on CPU and t->t_disp_time is inaccurate)
 *	- t->t_cpu is presently the target of an offline or partition move
 *	  request
 */
static cpu_t *
cpu_choose(kthread_t *t, pri_t tpri)
{
	ASSERT(tpri < kpqpri);

	if ((((lbolt - t->t_disp_time) > t->t_cpu->cpu_rechoose) &&
	    t != curthread) || t->t_cpu == cpu_inmotion) {
		return (disp_lowpri_cpu(t->t_cpu, t->t_lpl, tpri, NULL));
	}

	/*
	 * Take a trip through disp_lowpri_cpu() if the thread was
	 * running outside it's home lgroup
	 */
	if (!klgrpset_ismember(t->t_lpl->lpl_lgrp->lgrp_set[LGRP_RSRC_CPU],
	    t->t_cpu->cpu_lpl->lpl_lgrpid)) {
		return (disp_lowpri_cpu(t->t_cpu, t->t_lpl, tpri,
		    (t == curthread) ? t->t_cpu : NULL));
	}
	return (t->t_cpu);
}
