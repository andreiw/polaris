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
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/


/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"@(#)execve.s	1.12	05/06/08 SMI"

	.file	"execve.s"

/* C library -- execve						*/
/* int execve(const char *path, const char *argv[], const char *envp[])
 *
 * where argv is a vector argv[0] ... argv[x], 0
 * last vector element must be 0
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(execve,function)

#include "SYS.h"

	ANSI_PRAGMA_WEAK2(_private_execve,execve,function)
	SYSCALL_RVAL1(execve)
	SET_SIZE(execve)
