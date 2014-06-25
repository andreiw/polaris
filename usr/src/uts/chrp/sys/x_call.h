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


#ifndef	_SYS_X_CALL_H
#define	_SYS_X_CALL_H

#pragma ident	"%Z%%M%	%I%	%E% SML"

/*
 * For x86, we only have three cross call levels:
 * a low, med and high. (see xc_levels.h)
 */
#ifdef	MP
#include <sys/xc_levels.h>
#endif

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef	MP
/*
 * States of a cross-call session. (stored in xc_state field of the
 * per-CPU area).
 */
#define	XC_DONE		0	/* x-call session done */
#define	XC_HOLD		1	/* spin doing nothing */
#define	XC_SYNC_OP	2	/* perform a synchronous operation */
#define	XC_CALL_OP	3	/* perform a call operation */
#define	XC_WAIT		4	/* capture/release. callee has seen wait */

#ifndef _ASM

#include <sys/cpuvar.h>

struct	xc_mbox {
	int	(*func)();
	int	arg1;
	int	arg2;
	int	arg3;
	cpuset_t set;
	int	saved_pri;
};

/*
 * Cross-call routines.
 */
#if defined(_KERNEL)

extern void	xc_init(void);
extern u_int	xc_serv(caddr_t);
extern void	xc_call(int, int, int, int, cpuset_t, int (*)());
extern void	xc_sync(int, int, int, int, cpuset_t, int (*)());
extern void	xc_prolog(cpuset_t);
extern void	xc_epilog(void);
extern void	xc_capture_cpus(cpuset_t);
extern void	xc_release_cpus(void);

#endif	/* _KERNEL */

#endif	/* !_ASM */

#endif	/* MP */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_X_CALL_H */
