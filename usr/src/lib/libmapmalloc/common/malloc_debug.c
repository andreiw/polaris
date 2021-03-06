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
 * Copyright (c) 1991, Sun Microsytems, Inc.
 */

#pragma ident	"@(#)malloc_debug.c	1.3	05/06/08 SMI"

/*LINTLIBRARY*/
#include <sys/types.h>


/*
 * malloc_debug(level) - empty routine
 */

/*ARGSUSED*/
int
malloc_debug(int level)
{
	return (1);
}


/*
 * malloc_verify() - empty routine
 */

int
malloc_verify(void)
{
	return (1);
}


/*
 * mallocmap() - empty routine
 */

void
mallocmap(void)
{
	;
}
