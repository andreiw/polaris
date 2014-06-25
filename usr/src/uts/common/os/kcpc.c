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
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"@(#)kcpc.c	1.14	06/02/11 SMI"

#include <sys/param.h>
#include <sys/thread.h>
#include <sys/cpuvar.h>
#include <sys/inttypes.h>
#include <sys/cmn_err.h>
#include <sys/time.h>
#include <sys/mutex.h>
#include <sys/systm.h>
#include <sys/kcpc.h>
#include <sys/cpc_impl.h>
#include <sys/cpc_pcbe.h>
#include <sys/atomic.h>
#include <sys/sunddi.h>
#include <sys/modctl.h>
#include <sys/sdt.h>
#if defined(__x86)
#include <asm/clock.h>
#endif

kmutex_t	kcpc_ctx_llock[CPC_HASH_BUCKETS];	/* protects ctx_list */
kcpc_ctx_t	*kcpc_ctx_list[CPC_HASH_BUCKETS];	/* head of list */


krwlock_t	kcpc_cpuctx_lock;	/* lock for 'kcpc_cpuctx' below */
int		kcpc_cpuctx;		/* number of cpu-specific contexts */

int kcpc_counts_include_idle = 1; /* Project Private /etc/system variable */

/*
 * These are set when a PCBE module is loaded.
 */
uint_t		cpc_ncounters = 0;
pcbe_ops_t	*pcbe_ops = NULL;

/*
 * Statistics on (mis)behavior
 */
static uint32_t kcpc_intrctx_count;    /* # overflows in an interrupt handler */
static uint32_t kcpc_nullctx_count;    /* # overflows in a thread with no ctx */

/*
 * Is misbehaviour (overflow in a thread with no context) fatal?
 */
#ifdef DEBUG
static int kcpc_nullctx_panic = 1;
#else
static int kcpc_nullctx_panic = 0;
#endif

static void kcpc_lwp_create(kthread_t *t, kthread_t *ct);
static void kcpc_restore(kcpc_ctx_t *ctx);
static void kcpc_save(kcpc_ctx_t *ctx);
static void kcpc_free(kcpc_ctx_t *ctx, int isexec);
static int kcpc_configure_reqs(kcpc_ctx_t *ctx, kcpc_set_t *set, int *subcode);
static void kcpc_free_configs(kcpc_set_t *set);
static kcpc_ctx_t *kcpc_ctx_alloc(void);
static void kcpc_ctx_clone(kcpc_ctx_t *ctx, kcpc_ctx_t *cctx);
static void kcpc_ctx_free(kcpc_ctx_t *ctx);
static int kcpc_assign_reqs(kcpc_set_t *set, kcpc_ctx_t *ctx);
static int kcpc_tryassign(kcpc_set_t *set, int starting_req, int *scratch);
static kcpc_set_t *kcpc_dup_set(kcpc_set_t *set);

void
kcpc_register_pcbe(pcbe_ops_t *ops)
{
	pcbe_ops = ops;
	cpc_ncounters = pcbe_ops->pcbe_ncounters();
}

int
kcpc_bind_cpu(kcpc_set_t *set, processorid_t cpuid, int *subcode)
{
	cpu_t		*cp;
	kcpc_ctx_t	*ctx;
	int		error;

	ctx = kcpc_ctx_alloc();

	if (kcpc_assign_reqs(set, ctx) != 0) {
		kcpc_ctx_free(ctx);
		*subcode = CPC_RESOURCE_UNAVAIL;
		return (EINVAL);
	}

	ctx->kc_cpuid = cpuid;
	ctx->kc_thread = curthread;

	set->ks_data = kmem_zalloc(set->ks_nreqs * sizeof (uint64_t), KM_SLEEP);

	if ((error = kcpc_configure_reqs(ctx, set, subcode)) != 0) {
		kmem_free(set->ks_data, set->ks_nreqs * sizeof (uint64_t));
		kcpc_ctx_free(ctx);
		return (error);
	}

	set->ks_ctx = ctx;
	ctx->kc_set = set;

	/*
	 * We must hold cpu_lock to prevent DR, offlining, or unbinding while
	 * we are manipulating the cpu_t and programming the hardware, else the
	 * the cpu_t could go away while we're looking at it.
	 */
	mutex_enter(&cpu_lock);
	cp = cpu_get(cpuid);

	if (cp == NULL)
		/*
		 * The CPU could have been DRd out while we were getting set up.
		 */
		goto unbound;

	mutex_enter(&cp->cpu_cpc_ctxlock);

	if (cp->cpu_cpc_ctx != NULL) {
		/*
		 * If this CPU already has a bound set, return an error.
		 */
		mutex_exit(&cp->cpu_cpc_ctxlock);
		goto unbound;
	}

	if (curthread->t_bind_cpu != cpuid) {
		mutex_exit(&cp->cpu_cpc_ctxlock);
		goto unbound;
	}
	cp->cpu_cpc_ctx = ctx;

	/*
	 * Kernel preemption must be disabled while fiddling with the hardware
	 * registers to prevent partial updates.
	 */
	kpreempt_disable();
	ctx->kc_rawtick = KCPC_GET_TICK();
	pcbe_ops->pcbe_program(ctx);
	kpreempt_enable();

	mutex_exit(&cp->cpu_cpc_ctxlock);
	mutex_exit(&cpu_lock);

	return (0);

unbound:
	mutex_exit(&cpu_lock);
	set->ks_ctx = NULL;
	kmem_free(set->ks_data, set->ks_nreqs * sizeof (uint64_t));
	kcpc_ctx_free(ctx);
	return (EAGAIN);
}

int
kcpc_bind_thread(kcpc_set_t *set, kthread_t *t, int *subcode)
{
	kcpc_ctx_t	*ctx;
	int		error;

	/*
	 * Only one set is allowed per context, so ensure there is no
	 * existing context.
	 */

	if (t->t_cpc_ctx != NULL)
		return (EEXIST);

	ctx = kcpc_ctx_alloc();

	/*
	 * The context must begin life frozen until it has been properly
	 * programmed onto the hardware. This prevents the context ops from
	 * worrying about it until we're ready.
	 */
	ctx->kc_flags |= KCPC_CTX_FREEZE;
	ctx->kc_hrtime = gethrtime();

	if (kcpc_assign_reqs(set, ctx) != 0) {
		kcpc_ctx_free(ctx);
		*subcode = CPC_RESOURCE_UNAVAIL;
		return (EINVAL);
	}

	ctx->kc_cpuid = -1;
	if (set->ks_flags & CPC_BIND_LWP_INHERIT)
		ctx->kc_flags |= KCPC_CTX_LWPINHERIT;
	ctx->kc_thread = t;
	t->t_cpc_ctx = ctx;
	/*
	 * Permit threads to look at their own hardware counters from userland.
	 */
	ctx->kc_flags |= KCPC_CTX_NONPRIV;

	/*
	 * Create the data store for this set.
	 */
	set->ks_data = kmem_alloc(set->ks_nreqs * sizeof (uint64_t), KM_SLEEP);

	if ((error = kcpc_configure_reqs(ctx, set, subcode)) != 0) {
		kmem_free(set->ks_data, set->ks_nreqs * sizeof (uint64_t));
		kcpc_ctx_free(ctx);
		t->t_cpc_ctx = NULL;
		return (error);
	}

	set->ks_ctx = ctx;
	ctx->kc_set = set;

	/*
	 * Add a device context to the subject thread.
	 */
	installctx(t, ctx, kcpc_save, kcpc_restore, NULL,
	    kcpc_lwp_create, NULL, kcpc_free);

	/*
	 * Ask the backend to program the hardware.
	 */
	if (t == curthread) {
		kpreempt_disable();
		ctx->kc_rawtick = KCPC_GET_TICK();
		atomic_and_uint(&ctx->kc_flags, ~KCPC_CTX_FREEZE);
		pcbe_ops->pcbe_program(ctx);
		kpreempt_enable();
	} else
		/*
		 * Since we are the agent LWP, we know the victim LWP is stopped
		 * until we're done here; no need to worry about preemption or
		 * migration here. We still use an atomic op to clear the flag
		 * to ensure the flags are always self-consistent; they can
		 * still be accessed from, for instance, another CPU doing a
		 * kcpc_invalidate_all().
		 */
		atomic_and_uint(&ctx->kc_flags, ~KCPC_CTX_FREEZE);


	return (0);
}

/*
 * Walk through each request in the set and ask the PCBE to configure a
 * corresponding counter.
 */
static int
kcpc_configure_reqs(kcpc_ctx_t *ctx, kcpc_set_t *set, int *subcode)
{
	int		i;
	int		ret;
	kcpc_request_t	*rp;

	for (i = 0; i < set->ks_nreqs; i++) {
		int n;
		rp = &set->ks_req[i];

		n = rp->kr_picnum;

		ASSERT(n >= 0 && n < cpc_ncounters);

		ASSERT(ctx->kc_pics[n].kp_req == NULL);

		if (rp->kr_flags & CPC_OVF_NOTIFY_EMT) {
			if ((pcbe_ops->pcbe_caps & CPC_CAP_OVERFLOW_INTERRUPT)
			    == 0) {
				*subcode = -1;
				return (ENOTSUP);
			}
			/*
			 * If any of the counters have requested overflow
			 * notification, we flag the context as being one that
			 * cares about overflow.
			 */
			ctx->kc_flags |= KCPC_CTX_SIGOVF;
		}

		rp->kr_config = NULL;
		if ((ret = pcbe_ops->pcbe_configure(n, rp->kr_event,
		    rp->kr_preset, rp->kr_flags, rp->kr_nattrs, rp->kr_attr,
		    &(rp->kr_config), (void *)ctx)) != 0) {
			kcpc_free_configs(set);
			*subcode = ret;
			if (ret == CPC_ATTR_REQUIRES_PRIVILEGE)
				return (EACCES);
			return (EINVAL);
		}

		ctx->kc_pics[n].kp_req = rp;
		rp->kr_picp = &ctx->kc_pics[n];
		rp->kr_data = set->ks_data + rp->kr_index;
		*rp->kr_data = rp->kr_preset;
	}

	return (0);
}

static void
kcpc_free_configs(kcpc_set_t *set)
{
	int i;

	for (i = 0; i < set->ks_nreqs; i++)
		if (set->ks_req[i].kr_config != NULL)
			pcbe_ops->pcbe_free(set->ks_req[i].kr_config);
}

/*
 * buf points to a user address and the data should be copied out to that
 * address in the current process.
 */
int
kcpc_sample(kcpc_set_t *set, uint64_t *buf, hrtime_t *hrtime, uint64_t *tick)
{
	kcpc_ctx_t	*ctx = set->ks_ctx;
	uint64_t	curtick = KCPC_GET_TICK();

	if (ctx == NULL)
		return (EINVAL);
	else if (ctx->kc_flags & KCPC_CTX_INVALID)
		return (EAGAIN);

	if ((ctx->kc_flags & KCPC_CTX_FREEZE) == 0) {
		/*
		 * Kernel preemption must be disabled while reading the
		 * hardware regs, and if this is a CPU-bound context, while
		 * checking the CPU binding of the current thread.
		 */
		kpreempt_disable();

		if (ctx->kc_cpuid != -1) {
			if (curthread->t_bind_cpu != ctx->kc_cpuid) {
				kpreempt_enable();
				return (EAGAIN);
			}
		}

		if (ctx->kc_thread == curthread) {
			ctx->kc_hrtime = gethrtime();
			pcbe_ops->pcbe_sample(ctx);
			ctx->kc_vtick += curtick - ctx->kc_rawtick;
			ctx->kc_rawtick = curtick;
		}

		kpreempt_enable();
	}

	if (copyout(set->ks_data, buf,
	    set->ks_nreqs * sizeof (uint64_t)) == -1)
		return (EFAULT);
	if (copyout(&ctx->kc_hrtime, hrtime, sizeof (uint64_t)) == -1)
		return (EFAULT);
	if (copyout(&ctx->kc_vtick, tick, sizeof (uint64_t)) == -1)
		return (EFAULT);

	return (0);
}

/*
 * Stop the counters on the CPU this context is bound to.
 */
static void
kcpc_stop_hw(kcpc_ctx_t *ctx)
{
	cpu_t *cp;

	ASSERT((ctx->kc_flags & (KCPC_CTX_INVALID | KCPC_CTX_INVALID_STOPPED))
	    == KCPC_CTX_INVALID);

	kpreempt_disable();

	cp = cpu_get(ctx->kc_cpuid);
	ASSERT(cp != NULL);

	if (cp == CPU) {
		pcbe_ops->pcbe_allstop();
		atomic_or_uint(&ctx->kc_flags,
		    KCPC_CTX_INVALID_STOPPED);
	} else
		kcpc_remote_stop(cp);
	kpreempt_enable();
}

int
kcpc_unbind(kcpc_set_t *set)
{
	kcpc_ctx_t	*ctx = set->ks_ctx;
	kthread_t	*t;

	if (ctx == NULL)
		return (EINVAL);

	atomic_or_uint(&ctx->kc_flags, KCPC_CTX_INVALID);

	if (ctx->kc_cpuid == -1) {
		t = ctx->kc_thread;
		/*
		 * The context is thread-bound and therefore has a device
		 * context.  It will be freed via removectx() calling
		 * freectx() calling kcpc_free().
		 */
		if (t == curthread &&
			(ctx->kc_flags & KCPC_CTX_INVALID_STOPPED) == 0) {
			kpreempt_disable();
			pcbe_ops->pcbe_allstop();
			atomic_or_uint(&ctx->kc_flags,
			    KCPC_CTX_INVALID_STOPPED);
			kpreempt_enable();
		}
#ifdef DEBUG
		if (removectx(t, ctx, kcpc_save, kcpc_restore, NULL,
		    kcpc_lwp_create, NULL, kcpc_free) == 0)
			panic("kcpc_unbind: context %p not preset on thread %p",
			    ctx, t);
#else
		(void) removectx(t, ctx, kcpc_save, kcpc_restore, NULL,
		    kcpc_lwp_create, NULL, kcpc_free);
#endif /* DEBUG */
		t->t_cpc_set = NULL;
		t->t_cpc_ctx = NULL;
	} else {
		/*
		 * If we are unbinding a CPU-bound set from a remote CPU, the
		 * native CPU's idle thread could be in the midst of programming
		 * this context onto the CPU. We grab the context's lock here to
		 * ensure that the idle thread is done with it. When we release
		 * the lock, the CPU no longer has a context and the idle thread
		 * will move on.
		 *
		 * cpu_lock must be held to prevent the CPU from being DR'd out
		 * while we disassociate the context from the cpu_t.
		 */
		cpu_t *cp;
		mutex_enter(&cpu_lock);
		cp = cpu_get(ctx->kc_cpuid);
		if (cp != NULL) {
			/*
			 * The CPU may have been DR'd out of the system.
			 */
			mutex_enter(&cp->cpu_cpc_ctxlock);
			if ((ctx->kc_flags & KCPC_CTX_INVALID_STOPPED) == 0)
				kcpc_stop_hw(ctx);
			ASSERT(ctx->kc_flags & KCPC_CTX_INVALID_STOPPED);
			cp->cpu_cpc_ctx = NULL;
			mutex_exit(&cp->cpu_cpc_ctxlock);
		}
		mutex_exit(&cpu_lock);
		if (ctx->kc_thread == curthread) {
			kcpc_free(ctx, 0);
			curthread->t_cpc_set = NULL;
		}
	}

	return (0);
}

int
kcpc_preset(kcpc_set_t *set, int index, uint64_t preset)
{
	int i;

	ASSERT(set != NULL);
	ASSERT(set->ks_ctx != NULL);
	ASSERT(set->ks_ctx->kc_thread == curthread);
	ASSERT(set->ks_ctx->kc_cpuid == -1);

	if (index < 0 || index >= set->ks_nreqs)
		return (EINVAL);

	for (i = 0; i < set->ks_nreqs; i++)
		if (set->ks_req[i].kr_index == index)
			break;
	ASSERT(i != set->ks_nreqs);

	set->ks_req[i].kr_preset = preset;
	return (0);
}

int
kcpc_restart(kcpc_set_t *set)
{
	kcpc_ctx_t	*ctx = set->ks_ctx;
	int		i;

	ASSERT(ctx != NULL);
	ASSERT(ctx->kc_thread == curthread);
	ASSERT(ctx->kc_cpuid == -1);

	kpreempt_disable();

	/*
	 * If the user is doing this on a running set, make sure the counters
	 * are stopped first.
	 */
	if ((ctx->kc_flags & KCPC_CTX_FREEZE) == 0)
		pcbe_ops->pcbe_allstop();

	for (i = 0; i < set->ks_nreqs; i++) {
		*(set->ks_req[i].kr_data) = set->ks_req[i].kr_preset;
		pcbe_ops->pcbe_configure(0, NULL, set->ks_req[i].kr_preset,
		    0, 0, NULL, &set->ks_req[i].kr_config, NULL);
	}

	/*
	 * Ask the backend to program the hardware.
	 */
	ctx->kc_rawtick = KCPC_GET_TICK();
	atomic_and_uint(&ctx->kc_flags, ~KCPC_CTX_FREEZE);
	pcbe_ops->pcbe_program(ctx);
	kpreempt_enable();

	return (0);
}

/*
 * Caller must hold kcpc_cpuctx_lock.
 */
int
kcpc_enable(kthread_t *t, int cmd, int enable)
{
	kcpc_ctx_t	*ctx = t->t_cpc_ctx;
	kcpc_set_t	*set = t->t_cpc_set;
	kcpc_set_t	*newset;
	int		i;
	int		flag;
	int		err;

	ASSERT(RW_READ_HELD(&kcpc_cpuctx_lock));

	if (ctx == NULL) {
		/*
		 * This thread has a set but no context; it must be a
		 * CPU-bound set.
		 */
		ASSERT(t->t_cpc_set != NULL);
		ASSERT(t->t_cpc_set->ks_ctx->kc_cpuid != -1);
		return (EINVAL);
	} else if (ctx->kc_flags & KCPC_CTX_INVALID)
		return (EAGAIN);

	if (cmd == CPC_ENABLE) {
		if ((ctx->kc_flags & KCPC_CTX_FREEZE) == 0)
			return (EINVAL);
		kpreempt_disable();
		atomic_and_uint(&ctx->kc_flags, ~KCPC_CTX_FREEZE);
		kcpc_restore(ctx);
		kpreempt_enable();
	} else if (cmd == CPC_DISABLE) {
		if (ctx->kc_flags & KCPC_CTX_FREEZE)
			return (EINVAL);
		kpreempt_disable();
		kcpc_save(ctx);
		atomic_or_uint(&ctx->kc_flags, KCPC_CTX_FREEZE);
		kpreempt_enable();
	} else if (cmd == CPC_USR_EVENTS || cmd == CPC_SYS_EVENTS) {
		/*
		 * Strategy for usr/sys: stop counters and update set's presets
		 * with current counter values, unbind, update requests with
		 * new config, then re-bind.
		 */
		flag = (cmd == CPC_USR_EVENTS) ?
		    CPC_COUNT_USER: CPC_COUNT_SYSTEM;

		kpreempt_disable();
		atomic_or_uint(&ctx->kc_flags,
		    KCPC_CTX_INVALID | KCPC_CTX_INVALID_STOPPED);
		pcbe_ops->pcbe_allstop();
		kpreempt_enable();
		for (i = 0; i < set->ks_nreqs; i++) {
			set->ks_req[i].kr_preset = *(set->ks_req[i].kr_data);
			if (enable)
				set->ks_req[i].kr_flags |= flag;
			else
				set->ks_req[i].kr_flags &= ~flag;
		}
		newset = kcpc_dup_set(set);
		if (kcpc_unbind(set) != 0)
			return (EINVAL);
		t->t_cpc_set = newset;
		if (kcpc_bind_thread(newset, t, &err) != 0) {
			t->t_cpc_set = NULL;
			kcpc_free_set(newset);
			return (EINVAL);
		}
	} else
		return (EINVAL);

	return (0);
}

/*
 * Provide PCBEs with a way of obtaining the configs of every counter which will
 * be programmed together.
 *
 * If current is NULL, provide the first config.
 *
 * If data != NULL, caller wants to know where the data store associated with
 * the config we return is located.
 */
void *
kcpc_next_config(void *token, void *current, uint64_t **data)
{
	int		i;
	kcpc_pic_t	*pic;
	kcpc_ctx_t *ctx = (kcpc_ctx_t *)token;

	if (current == NULL) {
		/*
		 * Client would like the first config, which may not be in
		 * counter 0; we need to search through the counters for the
		 * first config.
		 */
		for (i = 0; i < cpc_ncounters; i++)
			if (ctx->kc_pics[i].kp_req != NULL)
				break;
		/*
		 * There are no counters configured for the given context.
		 */
		if (i == cpc_ncounters)
			return (NULL);
	} else {
		/*
		 * There surely is a faster way to do this.
		 */
		for (i = 0; i < cpc_ncounters; i++) {
			pic = &ctx->kc_pics[i];

			if (pic->kp_req != NULL &&
			    current == pic->kp_req->kr_config)
				break;
		}

		/*
		 * We found the current config at picnum i. Now search for the
		 * next configured PIC.
		 */
		for (i++; i < cpc_ncounters; i++) {
			pic = &ctx->kc_pics[i];
			if (pic->kp_req != NULL)
				break;
		}

		if (i == cpc_ncounters)
			return (NULL);
	}

	if (data != NULL) {
		*data = ctx->kc_pics[i].kp_req->kr_data;
	}

	return (ctx->kc_pics[i].kp_req->kr_config);
}


static kcpc_ctx_t *
kcpc_ctx_alloc(void)
{
	kcpc_ctx_t	*ctx;
	long		hash;

	ctx = (kcpc_ctx_t *)kmem_alloc(sizeof (kcpc_ctx_t), KM_SLEEP);

	hash = CPC_HASH_CTX(ctx);
	mutex_enter(&kcpc_ctx_llock[hash]);
	ctx->kc_next = kcpc_ctx_list[hash];
	kcpc_ctx_list[hash] = ctx;
	mutex_exit(&kcpc_ctx_llock[hash]);

	ctx->kc_pics = (kcpc_pic_t *)kmem_zalloc(sizeof (kcpc_pic_t) *
	    cpc_ncounters, KM_SLEEP);

	ctx->kc_flags = 0;
	ctx->kc_vtick = 0;
	ctx->kc_rawtick = 0;
	ctx->kc_cpuid = -1;

	return (ctx);
}

/*
 * Copy set from ctx to the child context, cctx, if it has CPC_BIND_LWP_INHERIT
 * in the flags.
 */
static void
kcpc_ctx_clone(kcpc_ctx_t *ctx, kcpc_ctx_t *cctx)
{
	kcpc_set_t	*ks = ctx->kc_set, *cks;
	int		i, j;
	int		code;

	ASSERT(ks != NULL);

	if ((ks->ks_flags & CPC_BIND_LWP_INHERIT) == 0)
		return;

	cks = kmem_alloc(sizeof (*cks), KM_SLEEP);
	cctx->kc_set = cks;
	cks->ks_flags = ks->ks_flags;
	cks->ks_nreqs = ks->ks_nreqs;
	cks->ks_req = kmem_alloc(cks->ks_nreqs *
	    sizeof (kcpc_request_t), KM_SLEEP);
	cks->ks_data = kmem_alloc(cks->ks_nreqs * sizeof (uint64_t),
	    KM_SLEEP);
	cks->ks_ctx = cctx;

	for (i = 0; i < cks->ks_nreqs; i++) {
		cks->ks_req[i].kr_index = ks->ks_req[i].kr_index;
		cks->ks_req[i].kr_picnum = ks->ks_req[i].kr_picnum;
		(void) strncpy(cks->ks_req[i].kr_event,
		    ks->ks_req[i].kr_event, CPC_MAX_EVENT_LEN);
		cks->ks_req[i].kr_preset = ks->ks_req[i].kr_preset;
		cks->ks_req[i].kr_flags = ks->ks_req[i].kr_flags;
		cks->ks_req[i].kr_nattrs = ks->ks_req[i].kr_nattrs;
		if (ks->ks_req[i].kr_nattrs > 0) {
			cks->ks_req[i].kr_attr =
			    kmem_alloc(ks->ks_req[i].kr_nattrs *
				sizeof (kcpc_attr_t), KM_SLEEP);
		}
		for (j = 0; j < ks->ks_req[i].kr_nattrs; j++) {
			(void) strncpy(cks->ks_req[i].kr_attr[j].ka_name,
			    ks->ks_req[i].kr_attr[j].ka_name,
			    CPC_MAX_ATTR_LEN);
			cks->ks_req[i].kr_attr[j].ka_val =
			    ks->ks_req[i].kr_attr[j].ka_val;
		}
	}
	if (kcpc_configure_reqs(cctx, cks, &code) != 0)
		panic("kcpc_ctx_clone: configure of context %p with "
		    "set %p failed with subcode %d", cctx, cks, code);
}


static void
kcpc_ctx_free(kcpc_ctx_t *ctx)
{
	kcpc_ctx_t	**loc;
	long		hash = CPC_HASH_CTX(ctx);

	mutex_enter(&kcpc_ctx_llock[hash]);
	loc = &kcpc_ctx_list[hash];
	ASSERT(*loc != NULL);
	while (*loc != ctx)
		loc = &(*loc)->kc_next;
	*loc = ctx->kc_next;
	mutex_exit(&kcpc_ctx_llock[hash]);

	kmem_free(ctx->kc_pics, cpc_ncounters * sizeof (kcpc_pic_t));
	kmem_free(ctx, sizeof (*ctx));
}

/*
 * Generic interrupt handler used on hardware that generates
 * overflow interrupts.
 *
 * Note: executed at high-level interrupt context!
 */
/*ARGSUSED*/
kcpc_ctx_t *
kcpc_overflow_intr(caddr_t arg, uint64_t bitmap)
{
	kcpc_ctx_t	*ctx;
	kthread_t	*t = curthread;
	int		i;

	/*
	 * On both x86 and UltraSPARC, we may deliver the high-level
	 * interrupt in kernel mode, just after we've started to run an
	 * interrupt thread.  (That's because the hardware helpfully
	 * delivers the overflow interrupt some random number of cycles
	 * after the instruction that caused the overflow by which time
	 * we're in some part of the kernel, not necessarily running on
	 * the right thread).
	 *
	 * Check for this case here -- find the pinned thread
	 * that was running when the interrupt went off.
	 */
	if (t->t_flag & T_INTR_THREAD) {
		klwp_t *lwp;

		atomic_add_32(&kcpc_intrctx_count, 1);

		/*
		 * Note that t_lwp is always set to point at the underlying
		 * thread, thus this will work in the presence of nested
		 * interrupts.
		 */
		ctx = NULL;
		if ((lwp = t->t_lwp) != NULL) {
			t = lwptot(lwp);
			ctx = t->t_cpc_ctx;
		}
	} else
		ctx = t->t_cpc_ctx;

	if (ctx == NULL) {
		/*
		 * This can easily happen if we're using the counters in
		 * "shared" mode, for example, and an overflow interrupt
		 * occurs while we are running cpustat.  In that case, the
		 * bound thread that has the context that belongs to this
		 * CPU is almost certainly sleeping (if it was running on
		 * the CPU we'd have found it above), and the actual
		 * interrupted thread has no knowledge of performance counters!
		 */
		ctx = curthread->t_cpu->cpu_cpc_ctx;
		if (ctx != NULL) {
			/*
			 * Return the bound context for this CPU to
			 * the interrupt handler so that it can synchronously
			 * sample the hardware counters and restart them.
			 */
			return (ctx);
		}

		/*
		 * As long as the overflow interrupt really is delivered early
		 * enough after trapping into the kernel to avoid switching
		 * threads, we must always be able to find the cpc context,
		 * or something went terribly wrong i.e. we ended up
		 * running a passivated interrupt thread, a kernel
		 * thread or we interrupted idle, all of which are Very Bad.
		 */
		if (kcpc_nullctx_panic)
			panic("null cpc context, thread %p", (void *)t);
		atomic_add_32(&kcpc_nullctx_count, 1);
	} else if ((ctx->kc_flags & KCPC_CTX_INVALID) == 0) {
		/*
		 * Schedule an ast to sample the counters, which will
		 * propagate any overflow into the virtualized performance
		 * counter(s), and may deliver a signal.
		 */
		ttolwp(t)->lwp_pcb.pcb_flags |= CPC_OVERFLOW;
		/*
		 * If a counter has overflowed which was counting on behalf of
		 * a request which specified CPC_OVF_NOTIFY_EMT, send the
		 * process a signal.
		 */
		for (i = 0; i < cpc_ncounters; i++) {
			if (ctx->kc_pics[i].kp_req != NULL &&
			    bitmap & (1 << i) &&
			    ctx->kc_pics[i].kp_req->kr_flags &
			    CPC_OVF_NOTIFY_EMT) {
				/*
				 * A signal has been requested for this PIC, so
				 * so freeze the context. The interrupt handler
				 * has already stopped the counter hardware.
				 */
				atomic_or_uint(&ctx->kc_flags, KCPC_CTX_FREEZE);
				atomic_or_uint(&ctx->kc_pics[i].kp_flags,
				    KCPC_PIC_OVERFLOWED);
			}
		}
		aston(t);
	}
	return (NULL);
}

/*
 * The current thread context had an overflow interrupt; we're
 * executing here in high-level interrupt context.
 */
/*ARGSUSED*/
uint_t
kcpc_hw_overflow_intr(caddr_t arg1, caddr_t arg2)
{
	kcpc_ctx_t	*ctx;
	uint64_t	bitmap;

	if (pcbe_ops == NULL ||
	    (bitmap = pcbe_ops->pcbe_overflow_bitmap()) == 0)
		return (DDI_INTR_UNCLAIMED);

	/*
	 * Prevent any further interrupts.
	 */
	pcbe_ops->pcbe_allstop();

	/*
	 * Invoke the "generic" handler.
	 *
	 * If the interrupt has occurred in the context of an lwp owning
	 * the counters, then the handler posts an AST to the lwp to
	 * trigger the actual sampling, and optionally deliver a signal or
	 * restart the counters, on the way out of the kernel using
	 * kcpc_hw_overflow_ast() (see below).
	 *
	 * On the other hand, if the handler returns the context to us
	 * directly, then it means that there are no other threads in
	 * the middle of updating it, no AST has been posted, and so we
	 * should sample the counters here, and restart them with no
	 * further fuss.
	 */
	if ((ctx = kcpc_overflow_intr(arg1, bitmap)) != NULL) {
		uint64_t curtick = KCPC_GET_TICK();

		ctx->kc_hrtime = gethrtime_waitfree();
		ctx->kc_vtick += curtick - ctx->kc_rawtick;
		ctx->kc_rawtick = curtick;
		pcbe_ops->pcbe_sample(ctx);
		pcbe_ops->pcbe_program(ctx);
	}

	return (DDI_INTR_CLAIMED);
}

/*
 * Called from trap() when processing the ast posted by the high-level
 * interrupt handler.
 */
int
kcpc_overflow_ast()
{
	kcpc_ctx_t	*ctx = curthread->t_cpc_ctx;
	int		i;
	int		found = 0;
	uint64_t	curtick = KCPC_GET_TICK();

	ASSERT(ctx != NULL);	/* Beware of interrupt skid. */

	/*
	 * An overflow happened: sample the context to ensure that
	 * the overflow is propagated into the upper bits of the
	 * virtualized 64-bit counter(s).
	 */
	kpreempt_disable();
	ctx->kc_hrtime = gethrtime_waitfree();
	pcbe_ops->pcbe_sample(ctx);
	kpreempt_enable();

	ctx->kc_vtick += curtick - ctx->kc_rawtick;

	/*
	 * The interrupt handler has marked any pics with KCPC_PIC_OVERFLOWED
	 * if that pic generated an overflow and if the request it was counting
	 * on behalf of had CPC_OVERFLOW_REQUEST specified. We go through all
	 * pics in the context and clear the KCPC_PIC_OVERFLOWED flags. If we
	 * found any overflowed pics, keep the context frozen and return true
	 * (thus causing a signal to be sent).
	 */
	for (i = 0; i < cpc_ncounters; i++) {
		if (ctx->kc_pics[i].kp_flags & KCPC_PIC_OVERFLOWED) {
			atomic_and_uint(&ctx->kc_pics[i].kp_flags,
			    ~KCPC_PIC_OVERFLOWED);
			found = 1;
		}
	}
	if (found)
		return (1);

	/*
	 * Otherwise, re-enable the counters and continue life as before.
	 */
	kpreempt_disable();
	atomic_and_uint(&ctx->kc_flags, ~KCPC_CTX_FREEZE);
	pcbe_ops->pcbe_program(ctx);
	kpreempt_enable();
	return (0);
}

/*
 * Called when switching away from current thread.
 */
static void
kcpc_save(kcpc_ctx_t *ctx)
{
	if (ctx->kc_flags & KCPC_CTX_INVALID) {
		if (ctx->kc_flags & KCPC_CTX_INVALID_STOPPED)
			return;
		/*
		 * This context has been invalidated but the counters have not
		 * been stopped. Stop them here and mark the context stopped.
		 */
		pcbe_ops->pcbe_allstop();
		atomic_or_uint(&ctx->kc_flags, KCPC_CTX_INVALID_STOPPED);
		return;
	}

	pcbe_ops->pcbe_allstop();
	if (ctx->kc_flags & KCPC_CTX_FREEZE)
		return;

	/*
	 * Need to sample for all reqs into each req's current mpic.
	 */
	ctx->kc_hrtime = gethrtime();
	ctx->kc_vtick += KCPC_GET_TICK() - ctx->kc_rawtick;
	pcbe_ops->pcbe_sample(ctx);
}

static void
kcpc_restore(kcpc_ctx_t *ctx)
{
	if ((ctx->kc_flags & (KCPC_CTX_INVALID | KCPC_CTX_INVALID_STOPPED)) ==
	    KCPC_CTX_INVALID)
		/*
		 * The context is invalidated but has not been marked stopped.
		 * We mark it as such here because we will not start the
		 * counters during this context switch.
		 */
		atomic_or_uint(&ctx->kc_flags, KCPC_CTX_INVALID_STOPPED);


	if (ctx->kc_flags & (KCPC_CTX_INVALID | KCPC_CTX_FREEZE))
		return;

	/*
	 * While programming the hardware, the counters should be stopped. We
	 * don't do an explicit pcbe_allstop() here because they should have
	 * been stopped already by the last consumer.
	 */
	ctx->kc_rawtick = KCPC_GET_TICK();
	pcbe_ops->pcbe_program(ctx);
}

/*
 * If kcpc_counts_include_idle is set to 0 by the sys admin, we add the the
 * following context operators to the idle thread on each CPU. They stop the
 * counters when the idle thread is switched on, and they start them again when
 * it is switched off.
 */

/*ARGSUSED*/
void
kcpc_idle_save(struct cpu *cp)
{
	/*
	 * The idle thread shouldn't be run anywhere else.
	 */
	ASSERT(CPU == cp);

	/*
	 * We must hold the CPU's context lock to ensure the context isn't freed
	 * while we're looking at it.
	 */
	mutex_enter(&cp->cpu_cpc_ctxlock);

	if ((cp->cpu_cpc_ctx == NULL) ||
	    (cp->cpu_cpc_ctx->kc_flags & KCPC_CTX_INVALID)) {
		mutex_exit(&cp->cpu_cpc_ctxlock);
		return;
	}

	pcbe_ops->pcbe_program(cp->cpu_cpc_ctx);
	mutex_exit(&cp->cpu_cpc_ctxlock);
}

void
kcpc_idle_restore(struct cpu *cp)
{
	/*
	 * The idle thread shouldn't be run anywhere else.
	 */
	ASSERT(CPU == cp);

	/*
	 * We must hold the CPU's context lock to ensure the context isn't freed
	 * while we're looking at it.
	 */
	mutex_enter(&cp->cpu_cpc_ctxlock);

	if ((cp->cpu_cpc_ctx == NULL) ||
	    (cp->cpu_cpc_ctx->kc_flags & KCPC_CTX_INVALID)) {
		mutex_exit(&cp->cpu_cpc_ctxlock);
		return;
	}

	pcbe_ops->pcbe_allstop();
	mutex_exit(&cp->cpu_cpc_ctxlock);
}

/*ARGSUSED*/
static void
kcpc_lwp_create(kthread_t *t, kthread_t *ct)
{
	kcpc_ctx_t	*ctx = t->t_cpc_ctx, *cctx;
	int		i;

	if (ctx == NULL || (ctx->kc_flags & KCPC_CTX_LWPINHERIT) == 0)
		return;

	rw_enter(&kcpc_cpuctx_lock, RW_READER);
	if (ctx->kc_flags & KCPC_CTX_INVALID) {
		rw_exit(&kcpc_cpuctx_lock);
		return;
	}
	cctx = kcpc_ctx_alloc();
	kcpc_ctx_clone(ctx, cctx);
	rw_exit(&kcpc_cpuctx_lock);

	cctx->kc_flags = ctx->kc_flags;
	cctx->kc_thread = ct;
	cctx->kc_cpuid = -1;
	ct->t_cpc_set = cctx->kc_set;
	ct->t_cpc_ctx = cctx;

	if (cctx->kc_flags & KCPC_CTX_SIGOVF) {
		kcpc_set_t *ks = cctx->kc_set;
		/*
		 * Our contract with the user requires us to immediately send an
		 * overflow signal to all children if we have the LWPINHERIT
		 * and SIGOVF flags set. In addition, all counters should be
		 * set to UINT64_MAX, and their pic's overflow flag turned on
		 * so that our trap() processing knows to send a signal.
		 */
		atomic_or_uint(&cctx->kc_flags, KCPC_CTX_FREEZE);
		for (i = 0; i < ks->ks_nreqs; i++) {
			kcpc_request_t *kr = &ks->ks_req[i];

			if (kr->kr_flags & CPC_OVF_NOTIFY_EMT) {
				*(kr->kr_data) = UINT64_MAX;
				kr->kr_picp->kp_flags |= KCPC_PIC_OVERFLOWED;
			}
		}
		ttolwp(ct)->lwp_pcb.pcb_flags |= CPC_OVERFLOW;
		aston(ct);
	}

	installctx(ct, cctx, kcpc_save, kcpc_restore,
	    NULL, kcpc_lwp_create, NULL, kcpc_free);
}

/*
 * Counter Stoppage Theory
 *
 * The counters may need to be stopped properly at the following occasions:
 *
 * 1) An LWP exits.
 * 2) A thread exits.
 * 3) An LWP performs an exec().
 * 4) A bound set is unbound.
 *
 * In addition to stopping the counters, the CPC context (a kcpc_ctx_t) may need
 * to be freed as well.
 *
 * Case 1: kcpc_passivate(), called via lwp_exit(), stops the counters. Later on
 * when the thread is freed, kcpc_free(), called by freectx(), frees the
 * context.
 *
 * Case 2: same as case 1 except kcpc_passivate is called from thread_exit().
 *
 * Case 3: kcpc_free(), called via freectx() via exec(), recognizes that it has
 * been called from exec. It stops the counters _and_ frees the context.
 *
 * Case 4: kcpc_unbind() stops the hardware _and_ frees the context.
 *
 * CPU-bound counters are always stopped via kcpc_unbind().
 */

/*
 * We're being called to delete the context; we ensure that all associated data
 * structures are freed, and that the hardware is passivated if this is an exec.
 */

/*ARGSUSED*/
static void
kcpc_free(kcpc_ctx_t *ctx, int isexec)
{
	int		i;
	kcpc_set_t	*set = ctx->kc_set;

	ASSERT(set != NULL);

	atomic_or_uint(&ctx->kc_flags, KCPC_CTX_INVALID);

	if (isexec) {
		/*
		 * This thread is execing, and after the exec it should not have
		 * any performance counter context. Stop the counters properly
		 * here so the system isn't surprised by an overflow interrupt
		 * later.
		 */
		if (ctx->kc_cpuid != -1) {
			cpu_t *cp;
			/*
			 * CPU-bound context; stop the appropriate CPU's ctrs.
			 * Hold cpu_lock while examining the CPU to ensure it
			 * doesn't go away.
			 */
			mutex_enter(&cpu_lock);
			cp = cpu_get(ctx->kc_cpuid);
			/*
			 * The CPU could have been DR'd out, so only stop the
			 * CPU and clear its context pointer if the CPU still
			 * exists.
			 */
			if (cp != NULL) {
				mutex_enter(&cp->cpu_cpc_ctxlock);
				kcpc_stop_hw(ctx);
				cp->cpu_cpc_ctx = NULL;
				mutex_exit(&cp->cpu_cpc_ctxlock);
			}
			mutex_exit(&cpu_lock);
			ASSERT(curthread->t_cpc_ctx == NULL);
		} else {
			/*
			 * Thread-bound context; stop _this_ CPU's counters.
			 */
			kpreempt_disable();
			pcbe_ops->pcbe_allstop();
			atomic_or_uint(&ctx->kc_flags,
			    KCPC_CTX_INVALID_STOPPED);
			kpreempt_enable();
			curthread->t_cpc_ctx = NULL;
		}

		/*
		 * Since we are being called from an exec and we know that
		 * exec is not permitted via the agent thread, we should clean
		 * up this thread's CPC state completely, and not leave dangling
		 * CPC pointers behind.
		 */
		ASSERT(ctx->kc_thread == curthread);
		curthread->t_cpc_set = NULL;
	}

	/*
	 * Walk through each request in this context's set and free the PCBE's
	 * configuration if it exists.
	 */
	for (i = 0; i < set->ks_nreqs; i++) {
		if (set->ks_req[i].kr_config != NULL)
			pcbe_ops->pcbe_free(set->ks_req[i].kr_config);
	}

	kmem_free(set->ks_data, set->ks_nreqs * sizeof (uint64_t));
	kcpc_ctx_free(ctx);
	kcpc_free_set(set);
}

/*
 * Free the memory associated with a request set.
 */
void
kcpc_free_set(kcpc_set_t *set)
{
	int		i;
	kcpc_request_t	*req;

	ASSERT(set->ks_req != NULL);

	for (i = 0; i < set->ks_nreqs; i++) {
		req = &set->ks_req[i];

		if (req->kr_nattrs != 0) {
			kmem_free(req->kr_attr,
			    req->kr_nattrs * sizeof (kcpc_attr_t));
		}
	}

	kmem_free(set->ks_req, sizeof (kcpc_request_t) * set->ks_nreqs);
	kmem_free(set, sizeof (kcpc_set_t));
}

/*
 * Grab every existing context and mark it as invalid.
 */
void
kcpc_invalidate_all(void)
{
	kcpc_ctx_t *ctx;
	long hash;

	for (hash = 0; hash < CPC_HASH_BUCKETS; hash++) {
		mutex_enter(&kcpc_ctx_llock[hash]);
		for (ctx = kcpc_ctx_list[hash]; ctx; ctx = ctx->kc_next)
			atomic_or_uint(&ctx->kc_flags, KCPC_CTX_INVALID);
		mutex_exit(&kcpc_ctx_llock[hash]);
	}
}

/*
 * Called from lwp_exit() and thread_exit()
 */
void
kcpc_passivate(void)
{
	kcpc_ctx_t *ctx = curthread->t_cpc_ctx;
	kcpc_set_t *set = curthread->t_cpc_set;

	if (set == NULL)
		return;

	/*
	 * We're cleaning up after this thread; ensure there are no dangling
	 * CPC pointers left behind. The context and set will be freed by
	 * freectx() in the case of an LWP-bound set, and by kcpc_unbind() in
	 * the case of a CPU-bound set.
	 */
	curthread->t_cpc_ctx = NULL;

	if (ctx == NULL) {
		/*
		 * This thread has a set but no context; it must be a CPU-bound
		 * set. The hardware will be stopped via kcpc_unbind() when the
		 * process exits and closes its file descriptors with
		 * kcpc_close(). Our only job here is to clean up this thread's
		 * state; the set will be freed with the unbind().
		 */
		(void) kcpc_unbind(set);
		/*
		 * Unbinding a set belonging to the current thread should clear
		 * its set pointer.
		 */
		ASSERT(curthread->t_cpc_set == NULL);
		return;
	}

	curthread->t_cpc_set = NULL;

	/*
	 * This thread/LWP is exiting but context switches will continue to
	 * happen for a bit as the exit proceeds.  Kernel preemption must be
	 * disabled here to prevent a race between checking or setting the
	 * INVALID_STOPPED flag here and kcpc_restore() setting the flag during
	 * a context switch.
	 */

	kpreempt_disable();
	if ((ctx->kc_flags & KCPC_CTX_INVALID_STOPPED) == 0) {
		pcbe_ops->pcbe_allstop();
		atomic_or_uint(&ctx->kc_flags,
		    KCPC_CTX_INVALID | KCPC_CTX_INVALID_STOPPED);
	}
	kpreempt_enable();
}

/*
 * Assign the requests in the given set to the PICs in the context.
 * Returns 0 if successful, -1 on failure.
 */
/*ARGSUSED*/
static int
kcpc_assign_reqs(kcpc_set_t *set, kcpc_ctx_t *ctx)
{
	int i;
	int *picnum_save;

	ASSERT(set->ks_nreqs <= cpc_ncounters);

	/*
	 * Provide kcpc_tryassign() with scratch space to avoid doing an
	 * alloc/free with every invocation.
	 */
	picnum_save = kmem_alloc(set->ks_nreqs * sizeof (int), KM_SLEEP);
	/*
	 * kcpc_tryassign() blindly walks through each request in the set,
	 * seeing if a counter can count its event. If yes, it assigns that
	 * counter. However, that counter may have been the only capable counter
	 * for _another_ request's event. The solution is to try every possible
	 * request first. Note that this does not cover all solutions, as
	 * that would require all unique orderings of requests, an n^n operation
	 * which would be unacceptable for architectures with many counters.
	 */
	for (i = 0; i < set->ks_nreqs; i++)
		if (kcpc_tryassign(set, i, picnum_save) == 0)
			break;

	kmem_free(picnum_save, set->ks_nreqs * sizeof (int));
	if (i == set->ks_nreqs)
		return (-1);
	return (0);
}

static int
kcpc_tryassign(kcpc_set_t *set, int starting_req, int *scratch)
{
	int		i;
	int		j;
	uint64_t	bitmap = 0, resmap = 0;
	uint64_t	ctrmap;

	/*
	 * We are attempting to assign the reqs to pics, but we may fail. If we
	 * fail, we need to restore the state of the requests to what it was
	 * when we found it, as some reqs may have been explicitly assigned to
	 * a specific PIC beforehand. We do this by snapshotting the assignments
	 * now and restoring from it later if we fail.
	 *
	 * Also we note here which counters have already been claimed by
	 * requests with explicit counter assignments.
	 */
	for (i = 0; i < set->ks_nreqs; i++) {
		scratch[i] = set->ks_req[i].kr_picnum;
		if (set->ks_req[i].kr_picnum != -1)
			resmap |= (1 << set->ks_req[i].kr_picnum);
	}

	/*
	 * Walk through requests assigning them to the first PIC that is
	 * capable.
	 */
	i = starting_req;
	do {
		if (set->ks_req[i].kr_picnum != -1) {
			ASSERT((bitmap & (1 << set->ks_req[i].kr_picnum)) == 0);
			bitmap |= (1 << set->ks_req[i].kr_picnum);
			if (++i == set->ks_nreqs)
				i = 0;
			continue;
		}

		ctrmap = pcbe_ops->pcbe_event_coverage(set->ks_req[i].kr_event);
		for (j = 0; j < cpc_ncounters; j++) {
			if (ctrmap & (1 << j) && (bitmap & (1 << j)) == 0 &&
			    (resmap & (1 << j)) == 0) {
				/*
				 * We can assign this counter because:
				 *
				 * 1. It can count the event (ctrmap)
				 * 2. It hasn't been assigned yet (bitmap)
				 * 3. It wasn't reserved by a request (resmap)
				 */
				bitmap |= (1 << j);
				break;
			}
		}
		if (j == cpc_ncounters) {
			for (i = 0; i < set->ks_nreqs; i++)
				set->ks_req[i].kr_picnum = scratch[i];
			return (-1);
		}
		set->ks_req[i].kr_picnum = j;

		if (++i == set->ks_nreqs)
			i = 0;
	} while (i != starting_req);

	return (0);
}

kcpc_set_t *
kcpc_dup_set(kcpc_set_t *set)
{
	kcpc_set_t	*new;
	int		i;
	int		j;

	new = kmem_alloc(sizeof (*new), KM_SLEEP);
	new->ks_flags = set->ks_flags;
	new->ks_nreqs = set->ks_nreqs;
	new->ks_req = kmem_alloc(set->ks_nreqs * sizeof (kcpc_request_t),
	    KM_SLEEP);
	new->ks_data = NULL;
	new->ks_ctx = NULL;

	for (i = 0; i < new->ks_nreqs; i++) {
		new->ks_req[i].kr_config = NULL;
		new->ks_req[i].kr_index = set->ks_req[i].kr_index;
		new->ks_req[i].kr_picnum = set->ks_req[i].kr_picnum;
		new->ks_req[i].kr_picp = NULL;
		new->ks_req[i].kr_data = NULL;
		(void) strncpy(new->ks_req[i].kr_event, set->ks_req[i].kr_event,
		    CPC_MAX_EVENT_LEN);
		new->ks_req[i].kr_preset = set->ks_req[i].kr_preset;
		new->ks_req[i].kr_flags = set->ks_req[i].kr_flags;
		new->ks_req[i].kr_nattrs = set->ks_req[i].kr_nattrs;
		new->ks_req[i].kr_attr = kmem_alloc(new->ks_req[i].kr_nattrs *
		    sizeof (kcpc_attr_t), KM_SLEEP);
		for (j = 0; j < new->ks_req[i].kr_nattrs; j++) {
			new->ks_req[i].kr_attr[j].ka_val =
			    set->ks_req[i].kr_attr[j].ka_val;
			(void) strncpy(new->ks_req[i].kr_attr[j].ka_name,
			    set->ks_req[i].kr_attr[j].ka_name,
			    CPC_MAX_ATTR_LEN);
		}
	}

	return (new);
}

int
kcpc_allow_nonpriv(void *token)
{
	return (((kcpc_ctx_t *)token)->kc_flags & KCPC_CTX_NONPRIV);
}

void
kcpc_invalidate(kthread_t *t)
{
	kcpc_ctx_t *ctx = t->t_cpc_ctx;

	if (ctx != NULL)
		atomic_or_uint(&ctx->kc_flags, KCPC_CTX_INVALID);
}

/*
 * Given a PCBE ID, attempt to load a matching PCBE module. The strings given
 * are used to construct PCBE names, starting with the most specific,
 * "pcbe.first.second.third.fourth" and ending with the least specific,
 * "pcbe.first".
 *
 * Returns 0 if a PCBE was successfully loaded and -1 upon error.
 */
int
kcpc_pcbe_tryload(const char *prefix, uint_t first, uint_t second, uint_t third)
{
	uint_t s[3];

	s[0] = first;
	s[1] = second;
	s[2] = third;

	return (modload_qualified("pcbe",
	    "pcbe", prefix, ".", s, 3) < 0 ? -1 : 0);
}