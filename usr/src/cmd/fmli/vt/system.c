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
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/


/*
 * Copyright  (c) 1986 AT&T
 *	All Rights Reserved
 */
#ident	"@(#)system.c	1.6	05/06/08 SMI"       /* SVr4.0 1.2 */

#include	<curses.h>
#include	"wish.h"

int
vt_system(string)
register char	*string;
{
	register int	retval;
	char	buf;

	(void) vt_before_fork();
	putchar('\n');
	fflush(stdout);
	retval = system(string);
	printf("Please hit ENTER to continue: ");
	fflush(stdout);
	read(0, &buf, 1);
	(void) vt_after_fork();
	return retval;
}
