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
 * Copyright 2005 Cyril Plisko.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef _SYS_MACHLOCK_H
#define	_SYS_MACHLOCK_H

#pragma ident	"%Z%%M%	%I%	%E% CP"

/*
 * Machine dependent lock description
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _ASM

#include <sys/types.h>

#ifdef _KERNEL

extern void	lock_set(lock_t *lp);
extern int	lock_try(lock_t *lp);
extern int	lock_spin_try(lock_t *lp);
extern int	ulock_try(lock_t *lp);
extern void	ulock_clear(lock_t *lp);
extern void	lock_clear(lock_t *lp);
extern void	lock_set_spl(lock_t *lp, int new_pil, ushort_t *old_pil);
extern void	lock_clear_splx(lock_t *lp, int s);

#endif /* _KERNEL */

#define	LOCK_HELD_VALUE		0xff
#define	LOCK_INIT_CLEAR(lp)	(*(lp) = 0)
#define	LOCK_INIT_HELD(lp)	(*(lp) = LOCK_HELD_VALUE)
#define	LOCK_HELD(lp)		(*(volatile lock_t *)(lp) != 0)

typedef	lock_t	disp_lock_t;	/* dispather lock type */

extern	volatile int	hres_lock;

#endif /* _ASM */

/*
 * The next section shamelessly copied from intel/sys/machlock.h
 * SPARC version is very similar (mutexes are still listed as dependent)
 * and we may need to revisit this file later.
 */
/*
 * The definitions of the symbolic interrupt levels:
 *
 *   CLOCK_LEVEL =>  The level at which one must be to block the clock.
 *
 *   LOCK_LEVEL  =>  The highest level at which one may block (and thus the
 *                   highest level at which one may acquire adaptive locks)
 *                   Also the highest level at which one may be preempted.
 *
 *   DISP_LEVEL  =>  The level at which one must be to perform dispatcher
 *                   operations.
 *
 * The constraints on the platform:
 *
 *  - CLOCK_LEVEL must be less than or equal to LOCK_LEVEL
 *  - LOCK_LEVEL must be less than DISP_LEVEL
 *  - DISP_LEVEL should be as close to LOCK_LEVEL as possible
 *
 * Note that LOCK_LEVEL and CLOCK_LEVEL have historically always been equal;
 * changing this relationship is probably possible but not advised.
 *
 */

#define	PIL_MAX		15

#define	CLOCK_LEVEL	10
#define	LOCK_LEVEL	10
#define	DISP_LEVEL	(LOCK_LEVEL + 1)

#define	HIGH_LEVELS	(PIL_MAX - LOCK_LEVEL)

/*
 * The following mask is for the cpu_intr_actv bits corresponding to
 * high-level PILs. It should equal:
 * ((((1 << PIL_MAX + 1) - 1) >> LOCK_LEVEL + 1) << LOCK_LEVEL + 1)
 */
#define	CPU_INTR_ACTV_HIGH_LEVEL_MASK	0xF800

/*
 * The semaphore code depends on being able to represent a lock plus
 * owner in a single 32-bit word.  (Mutexes used to have a similar
 * dependency, but no longer.)  Thus the owner must contain at most
 * 24 significant bits.  At present only threads and semaphores
 * must be aware of this vile constraint.  Different ISAs may handle this
 * differently depending on their capabilities (e.g. compare-and-swap)
 * and limitations (e.g. constraints on alignment and/or KERNELBASE).
 */
#define	PTR24_LSB	5			/* lower bits all zero */
#define	PTR24_MSB	(PTR24_LSB + 24)	/* upper bits all one */
#define	PTR24_ALIGN	32		/* minimum alignment (1 << lsb) */
#define	PTR24_BASE	0xe0000000	/* minimum ptr value (-1 >> (32-msb)) */


#ifdef __cplusplus
}
#endif

#endif /* _SYS_MACHLOCK_H */
