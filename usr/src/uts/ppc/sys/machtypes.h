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

#ifndef _SYS_MACHTYPES_H
#define	_SYS_MACHTYPES_H

#pragma ident	"%Z%%M%	%I%	%E% CP"

/*
 * PowerPC Machine dependent types
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char lock_t;

/*
 * On PowerPC, a label_t holds 22 registers:
 *   r1 (stack pointer), pc, cr, and r13..r31
 * See usr/src/uts/ppc/os/ppc_subr.c
 * for implementation of setjmp() and longjmp().
 */

typedef struct label_t {
	long val[22];
} label_t;

#ifdef __cplusplus
}
#endif

#endif /* _SYS_MACHTYPES_H */
