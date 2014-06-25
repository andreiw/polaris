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
 * Copyright (c) 1995-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)getn_ws.c	1.4	05/06/08 SMI"

/* LINTLIBRARY */

/*
 * getn_ws.c
 *
 * XCurses Library
 *
 * Copyright 1990, 1995 by Mortice Kern Systems Inc.  All rights reserved.
 *
 */

#if M_RCSID
#ifndef lint
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/getn_ws.c 1.1 "
"1995/07/06 14:01:35 ant Exp $";
#endif
#endif

#include <private.h>

#undef getn_wstr

int
getn_wstr(wint_t *wis, int n)
{
	int code;

	code = wgetn_wstr(stdscr, wis, n);

	return (code);
}

#undef mvgetn_wstr

int
mvgetn_wstr(int y, int x, wint_t *wis, int n)
{
	int code;

	if ((code = wmove(stdscr, y, x)) == OK)
		code = wgetn_wstr(stdscr, wis, n);

	return (code);
}

#undef mvwgetn_wstr

int
mvwgetn_wstr(WINDOW *w, int y, int x, wint_t *wis, int n)
{
	int code;

	if ((code = wmove(w, y, x)) == OK)
		code = wgetn_wstr(w, wis, n);

	return (code);
}

#undef get_wstr

int
get_wstr(wint_t *wis)
{
	int code;

	code = wgetn_wstr(stdscr, wis, -1);

	return (code);
}

#undef mvget_wstr

int
mvget_wstr(int y, int x, wint_t *wis)
{
	int code;

	if ((code = wmove(stdscr, y, x)) == OK)
		code = wgetn_wstr(stdscr, wis, -1);

	return (code);
}

#undef mvwget_wstr

int
mvwget_wstr(WINDOW *w, int y, int x, wint_t *wis)
{
	int code;

	if ((code = wmove(w, y, x)) == OK)
		code = wgetn_wstr(w, wis, -1);

	return (code);
}


#undef wget_wstr

int
wget_wstr(WINDOW *w, wint_t *wis)
{
	int code;

	code = wgetn_wstr(w, wis, -1);

	return (code);
}
