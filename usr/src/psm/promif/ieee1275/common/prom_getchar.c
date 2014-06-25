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
 * Copyright (c) 1991-1994, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prom_getchar.c	1.12	05/06/08 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

u_char
prom_getchar(void)
{
	int c;

	while ((c = prom_mayget()) == -1)
		;

	return ((u_char)c);
}

int
prom_mayget(void)
{
	ssize_t rv;
	char c;

	rv = prom_read(prom_stdin_ihandle(), &c, 1, 0, BYTE);
	return (rv == 1 ? (int)c : -1);
}
