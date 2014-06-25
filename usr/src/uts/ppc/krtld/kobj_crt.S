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

/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"%Z%%M%	%I%	%E% SMI"

/*
 * exit routine from linker/loader to kernel
 */

#include <sys/asm_linkage.h>

/*
 *	exitto is called from kobj_init and transfers control to
 *	_start, its argument.  The 3 kernel arguments are loaded
 *	from where they were saved in kobj_init, namely, "romp",
 *	"dbvec", and "bopp".
 */

#if defined(lint)

/* ARGSUSED */
void
exitto(caddr_t entrypoint)
{}

#else	/* lint */

	ENTRY(exitto)
	mtlr	%r3
	lis	%r3,romp@ha
	lwz	%r3,romp@l(%r3)
	lis	%r4,dbvec@ha
	lwz	%r4,dbvec@l(%r4)
	lis	%r5,bopp@ha
	lwz	%r5,bopp@l(%r5)
	blrl			! onward to the next program
	.long	0		! PANIC if return to here.
	SET_SIZE(exitto)
#endif
