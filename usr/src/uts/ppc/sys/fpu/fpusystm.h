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

#ifndef _SYS_FPU_FPUSYSTM_H
#define	_SYS_FPU_FPUSYSTM_H

#pragma ident	"@(#)fpusystm.h	1.7	94/11/18 SMI"
#pragma ident	"%Z%%M%	%I%	%E% SMI"

/*
 * ISA-dependent FPU interfaces
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _KERNEL

struct fpu;
struct regs;
struct core;

extern int fpu_exists;
extern int fpu_version;

void fpu_probe(void);
void fpu_hw_init(void);
void fp_save(struct fpu *);
void fp_restore(struct fpu *);
void fp_free(struct fpu *);
void fp_new_lwp(struct fpu *);
void fp_exit(struct fpu *);
int fp_assist_fault(struct regs *);
int no_fpu_fault(struct regs *);
int fpu_en_fault(struct regs *);
void fpu_save(struct fpu *);
void fpu_restore(struct fpu *);
void fp_fork(kthread_id_t, kthread_id_t);
uint_t fperr_reset(void);

#endif	/* _KERNEL */

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_FPU_FPUSYSTM_H */
