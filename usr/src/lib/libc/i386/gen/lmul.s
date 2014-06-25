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
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

	.ident	"@(#)lmul.s	1.5	05/06/08 SMI"

	.file	"lmul.s"

/ Double long multiply routine.

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(lmul,function)

#include "SYS.h"

	.set	lop,8
	.set	rop,16
	.set	ans,0

	ENTRY(lmul)
	popl	%eax
	xchgl	%eax,0(%esp)

	pushl	%eax

	movl	lop+4(%esp),%eax
	mull	rop(%esp)	/ high(lop) * low(rop)
	movl	%eax,%ecx	/ partial high(product)

	movl	rop+4(%esp),%eax
	mull	lop(%esp)	/ high(rop) * low(lop)
	addl	%eax,%ecx	/ partial sum of high(product)

	movl	rop(%esp),%eax
	mull	lop(%esp)	/ low(rop) * low(lop)
	addl	%edx,%ecx	/ final high(product)

	movl	%eax,%edx
	popl	%eax
	movl	%edx,ans(%eax)
	movl	%ecx,ans+4(%eax)

	ret
	SET_SIZE(lmul)