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
 *
 * Copyright 2006 Sun Microsystems Laboratories.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * VM - Hardware Address Translation management.
 *
 * This file describes the contents of the PowerPC MMU (ppcmmu)
 * specific hat data structures and the ppcmmu specific hat procedures.
 * The machine independent interface is described in <vm/hat.h>.
 *
 * The definitions assume 32bit implementation of PowerPC.
 */

#ifndef	_VM_LINT_H
#define	_VM_LINT_H

#pragma ident	"%Z%%M%	%I%	%E% SMI"

/*
 * Lint helper functions and macros
 * --------------------------------
 * LINT_USE() informs lint that a variable is used.
 * It controls usage hints more precisely than the ARGSUSED comment.
 *
 * LINT_VARIABLE() is used to inform lint that an expression is to be
 * treated as a variable, even if lint might otherwise emit warnings about
 * a constant expression.  For example, a macro might ASSERT something
 * about one of its arguments.  That argument could be a variable
 * expression or a compile-time constant expression.  If the argument is a
 * constant expression, then lint may whine about a constant in a
 * conditional.  We don't care.
 */

#ifdef __cplusplus
extern "C" {
#endif

#if defined(lint)

#if !defined(__GNUC__)
#define	__asm__(text)
#define	__inline__
#define	__volatile__

#endif /* !defined(__GNUC__) */

extern void _lint_use(int dummy, ...);
extern int _lint_vary;
#define	LINT_USE(var) var = 0; _lint_use(0, var);
#if defined(DEBUG)
#define	LINT_DEBUG_USE(var) var = 0; _lint_use(0, var);
#define	LINT_NDEBUG_USE(var)
#else
#define	LINT_DEBUG_USE(var)
#define	LINT_NDEBUG_USE(var) var = 0; _lint_use(0, var);
#endif
#define	LINT_SET(var, type) var = (type)_lint_vary
#define	LINT_VARIABLE(expr) ((expr) + _lint_vary)

#else /* ! defined(lint) */

#define	LINT_USE(var)
#define	LINT_DEBUG_USE(var)
#define	LINT_NDEBUG_USE(var)
#define	LINT_SET(var)
#define	LINT_VARIABLE(expr) (expr)

#endif /* ! defined(lint) */

#ifdef	__cplusplus
}
#endif

#endif	/* _VM_LINT_H */
