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
#ifndef _SYS_SMP_IMPLDEFS_H
#define	_SYS_SMP_IMPLDEFS_H

#pragma ident	"%Z%%M%	%I%	%E% SML"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <sys/sunddi.h>
#include <sys/cpuvar.h>
#include <sys/avintr.h>
#include <sys/pic.h>
#include <sys/xc_levels.h>

#define	WARM_RESET_VECTOR	0x467	/* the ROM/BIOS vector for 	*/
					/* starting up secondary cpu's	*/

/*
 *	External Reference Functions
 */
extern void (*psminitf)();	/* psm init entry point			*/
extern void (*picinitf)();	/* pic init entry point			*/
extern void (*clkinitf)();	/* clock init entry point		*/
extern int (*ap_mlsetup)(); 	/* completes init of starting cpu	*/
extern void (*send_dirintf)();	/* send interprocessor intr		*/
extern void (*cpu_startf)();	/* start running a given processor	*/
extern hrtime_t (*gethrtimef)(); /* get high resolution timer value	*/
extern void (*psm_shutdownf)();	/* machine dependent shutdown		*/
extern void (*psm_set_idle_cpuf)(processorid_t); /* cpu changed to idle */
extern void (*psm_unset_idle_cpuf)(processorid_t); /* cpu out of idle 	*/
extern int (*psm_disable_intr)(processorid_t); /* disable intr to cpu	*/
extern void (*psm_enable_intr)(processorid_t); /* enable intr to cpu	*/

extern int (*slvltovect)(int);	/* ipl interrupt priority level		*/
extern int (*setlvl)(int, int *); /* set intr pri represented by vect	*/
extern void (*setlvlx)(int, int); /* set intr pri to specified level	*/
extern void (*setspl)(int);	/* mask intr below or equal given ipl	*/
extern int (*addspl)(int, int, int, int); /* add intr mask of vector 	*/
extern int (*delspl)(int, int, int, int); /* delete intr mask of vector */

extern void (*setsoftint)(int, struct av_softinfo *);	/* trigger a software intr		*/

extern u_int xc_serv(caddr_t);	/* cross call service routine		*/
extern void set_pending();	/* set software interrupt pending	*/
extern void clksetup(void);	/* timer initialization 		*/
extern void microfind(void);

/* trigger a software intr */
extern void av_set_softint_pending();	/* set software interrupt pending */

/* map physical address							*/
extern caddr_t psm_map_phys(paddr_t, long, u_long);
/* unmap the physical address given in psm_map_phys() from the addr	*/
extern void psm_unmap_phys(caddr_t, long);
extern void psm_modloadonly(void);
extern void psm_install(void);
extern void psm_modload(void);

/*
 *	External Reference Data
 */
extern struct av_head autovect[]; /* array of auto intr vectors		*/
extern int clock_vector;	/* clock interrupt vector		*/
extern paddr_t rm_platter_pa;	/* phy addr realmode startup storage	*/
extern caddr_t rm_platter_va;	/* virt addr realmode startup storage	*/
extern int mp_cpus;		/* bit map of possible cpus found	*/

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SMP_IMPLDEFS_H */
