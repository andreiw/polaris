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

#ifndef _SYS_PCB_H
#define	_SYS_PCB_H

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#include <sys/reg.h>
#include <sys/debugreg.h>

#ifdef	__cplusplus
extern "C" {
#endif

#if !defined(_ASM)

typedef struct pcb {
	struct fpu	pcb_fpu;	/* fpu state */
	int		pcb_flags;	/* state flags; cleared on fork */
	dbregset_t 	pcb_dregs;	/* debug registers (HID regs 1,2,5) */
	long		pcb_instr;	/* /proc: instruction at stop */
	long		pcb_reserved;	/* padding for 8byte alignement */
} pcb_t;

#endif /* !defined(_ASM) */

/*
 * Bit definitions for pcb_flags
 */
#define	PCB_DEBUG_EN		0x01  /* debug registers are in use */
#define	PCB_DEBUG_MODIFIED	0x02  /* debug registers are modified (/proc) */
#define	PCB_FPU_INITIALIZED	0x04  /* flag signifying fpu in use */
#define	PCB_INSTR_VALID		0x08  /* value in pcb_instr is valid */
#define	PCB_FPU_STATE_MODIFIED  0x10  /* fpu state is modified (/proc) */
#define	CPC_OVERFLOW		0x40  /* performance counters overflowed */

#define	NORMAL_STEP	0x100	/* normal debugger-requested single-step */
#define	WATCH_STEP	0x200	/* single-stepping in watchpoint emulation */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PCB_H */
