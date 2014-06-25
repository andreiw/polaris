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

#ifndef _SYS_UCONTEXT_H
#define	_SYS_UCONTEXT_H

#pragma ident	"%Z%%M%	%I%	%E% CP"

/*
 * User context
 * TODO Add mixed 32 and 64 bit definition
 */

#include <sys/feature_tests.h>

#include <sys/types.h>
#include <sys/regset.h>
#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
#include <sys/signal.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
typedef struct ucontext ucontext_t;
#else
typedef struct __ucontext ucontext_t;
#endif /* !defined(_XPG4_2) || defined(__EXTENSIONS__) */

#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
struct  ucontext {
#else
struct  __ucontext {
#endif
	ulong_t		uc_flags;
	struct ucontext	*uc_link;
	sigset_t	uc_sigmask;
	stack_t		uc_stack;
	mcontext_t	uc_mcontext;
	long		uc_filler[6];
};

#define GETCONTEXT 0
#define SETCONTEXT 1
#define UC_SIGMASK 001
#define UC_STACK 002
#define UC_CPU 004
#define UC_MAU 010
#define UC_FPU UC_MAU
#define UC_INTR 020
#define UC_MCONTEXT (UC_CPU|UC_FPU)
#define UC_ALL (UC_SIGMASK|UC_STACK|UC_MCONTEXT)

#ifdef __cplusplus
}
#endif

#endif /* _SYS_UCONTEXT_H */
