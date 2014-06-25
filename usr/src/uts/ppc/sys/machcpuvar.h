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

#ifndef _SYS_MACHCPUVAR_H
#define	_SYS_MACHCPUVAR_H

#pragma ident	"%Z%%M%	%I%	%E% SMI"

/*
 * PowerPC machine-dependent part of cpu struct
 */

#include <sys/types.h>
#include <sys/regset.h>
#include <sys/avintr.h>
#include <sys/xc_levels.h>


#ifdef __cplusplus
extern "C" {
#endif

typedef void *cpu_pri_lev_t;

struct machcpu {
	/* define all the x_call stuff */
	volatile int	xc_pend[X_CALL_LEVELS];
	volatile int	xc_wait[X_CALL_LEVELS];
	volatile int	xc_ack[X_CALL_LEVELS];
	volatile int	xc_state[X_CALL_LEVELS];
	volatile int	xc_retval[X_CALL_LEVELS];

	int		mcpu_nodeid;	/* node-id */
	int		mcpu_pri;	/* CPU priority */
	cpu_pri_lev_t	mcpu_pri_data;	/* ptr to machine dependent data */
					/* for setting priority level */
	uint_t		mcpu_mask;	/* bitmask for this cpu (1<<cpu_id) */
	greg_t		mcpu_r3;	/* level 0 save area */
	greg_t		mcpu_srr0;	/* level 0 save area */
	greg_t		mcpu_srr1;	/* level 0 save area */
	greg_t		mcpu_dsisr;	/* level 0 save area */
	greg_t		mcpu_dar;	/* level 0 save area */

	caddr_t		mcpu_cmntrap;   /* level 1 entry point */
	caddr_t		mcpu_syscall;   /* level 1 entry point */
	caddr_t		mcpu_cmnint;	/* level 1 entry point */
	caddr_t		mcpu_clockintr; /* level 1 entry point */
	/* level 1 MSRs */
	greg_t		mcpu_msr_enabled;	/* interrupts enabled */
	greg_t		mcpu_msr_disabled;	/* interrupts disabled */

	/*
	 * For ppcopy() and friends.
	 */
	kmutex_t	mcpu_ppaddr_mutex;

	caddr_t		mcpu_caddr1;	/* per cpu CADDR1 */
	caddr_t		mcpu_caddr2;	/* per cpu CADDR2 */
	void		*mcpu_caddr1pte;
	void		*mcpu_caddr2pte;

	struct softint	mcpu_softinfo;
};

typedef struct machcpu	machcpu_t;

#define	NINTR_THREADS	(LOCK_LEVEL-1)	/* number of interrupt threads */

#define	cpu_pri		cpu_m.mcpu_pri
#define	cpu_pri_data	cpu_m.mcpu_pri_data
#define	cpu_mask	cpu_m.mcpu_mask
#define	cpu_r3		cpu_m.mcpu_r3
#define	cpu_srr0	cpu_m.mcpu_srr0
#define	cpu_srr1	cpu_m.mcpu_srr1
#define	cpu_dsisr	cpu_m.mcpu_dsisr
#define	cpu_dar		cpu_m.mcpu_dar

#if defined(XXX_HUH)
#define	cpu_cr		cpu_m.mcpu_cr
#define	cpu_lr		cpu_m.mcpu_lr
#define	cpu_mq		cpu_m.mcpu_mq
#define	cpu_ibatl	cpu_m.mcpu_ibatl
#define	cpu_ibatu	cpu_m.mcpu_ibatu
#define	cpu_trap_kadb	cpu_m.mcpu_trap_kadb
#endif /* defined(XXX_HUH) */

#define	cpu_cmntrap		cpu_m.mcpu_cmntrap
#define	cpu_syscall		cpu_m.mcpu_syscall
#define	cpu_cmnint		cpu_m.mcpu_cmnint
#define	cpu_clockintr		cpu_m.mcpu_clockintr
#define	cpu_msr_enabled		cpu_m.mcpu_msr_enabled
#define	cpu_msr_disabled	cpu_m.mcpu_msr_disabled

#define	cpu_ppaddr_mutex cpu_m.mcpu_ppaddr_mutex
#define	cpu_caddr1 cpu_m.mcpu_caddr1
#define	cpu_caddr2 cpu_m.mcpu_caddr2
#define	cpu_caddr1pte cpu_m.mcpu_caddr1pte
#define	cpu_caddr2pte cpu_m.mcpu_caddr2pte

#define	cpu_softinfo		cpu_m.mcpu_softinfo

#ifdef __cplusplus
}
#endif

#endif /* _SYS_MACHCPUVAR_H */
