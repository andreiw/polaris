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

#ifndef _SYS_REGSET_H
#define	_SYS_REGSET_H

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#include <sys/feature_tests.h>

#if !defined(_ASM)
#include <sys/types.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/reg_gp.h>

#if !defined(_ASM)

#if defined(_SYSCALL32)
#define	_NGREG32	_NGREG
#define	_NGREG64	_NGREG
typedef uint32_t	greg32_t;
typedef uint64_t 	greg64_t;
typedef greg32_t	gregset32_t[_NGREG32];
typedef greg64_t	gregset64_t[_NGREG64];

#endif /* _SYSCALL32 */

#include <sys/regset_fp.h>
#include <sys/struct_regs.h>

#endif /* !defined(_ASM) */

#ifdef __cplusplus
}
#endif

#endif /* _SYS_REGSET_H */
