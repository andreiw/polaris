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

#pragma ident	"@(#)forkall.s	1.14	05/06/08 SMI"

	.file	"forkall.s"

#include "SYS.h"

/ pid = forkall();

/ From the syscall:
/ %edx == 0 in parent process, %edx = 1 in child process.
/ %eax == pid of child in parent, %eax == pid of parent in child.
/
/ The child gets a zero return value.
/ The parent gets the pid of the child.

	ENTRY(__forkall)
	SYSTRAP_2RVALS(forkall)
	SYSCERROR
	testl	%edx, %edx
	jz	1f		/ jump if parent
	xorl	%eax, %eax	/ child, return (0)
1:
	RET
	SET_SIZE(__forkall)
