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
 * Copyright 2006 Cyril Plisko.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef _SYS_PROCFS_ISA_H
#define	_SYS_PROCFS_ISA_H

#pragma ident	"%Z%%M%	%I%	%E% CP"

/*
 * PowerPC ISA specific component of <sys/procfs.h>
 */

#include <sys/regset.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Possible values of pr_dmodel.
 * This isn't isa-specific, but needs to be defined here for other reasons.
 */
#define PR_MODEL_UNKNOWN	0
#define PR_MODEL_ILP32		1
#define	PR_MODEL_LP64		2

/*
 * To determine whether application is running native.
 */
#if defined(_LP64)
#define	PR_MODEL_NATIVE	PR_MODEL_LP64
#elif defined(_ILP32)
#define	PR_MODEL_NATIVE	PR_MODEL_ILP32
#else
#error "No DATAMODEL_NATIVE specified"
#endif	/* _LP64 || _ILP32 */

#if defined(__powerpc)
/*
 * __powerpc should be defined on both ppc and ppc64 and
 * thus it covers both cases - one instruction container
 */
typedef uint32_t	instr_t;
#endif

#define	NPRGREG		_NGREG
#define	prgreg_t	greg_t
#define	prgregset_t	gregset_t
#define	prfpregset	fpu
#define	prfpregset_t	fpregset_t

#if defined(_SYSCALL32)
/*
 * kernel view of the ppc32 register set
 */
typedef	uint32_t	instr32_t;
#if defined(__powerpc64)
#define	NPRGREG32	_NGREG32
#define	prgreg32_t	greg32_t
#define	prgregset32_t	gregset32_t
#define	prfpregset32	fpu32
#define	prfpregset32_t	fpregset32_t
#else
#define	NPRGREG32	_NGREG
#define	prgreg32_t	greg_t
#define	prgregset32_t	gregset_t
#define	prfpregset32	fpu
#define	prfpregset32_t	fpregset_t
#endif
#endif	/* _SYSCALL32 */

#ifdef __cplusplus
}
#endif

#endif /* _SYS_PROCFS_ISA_H */
