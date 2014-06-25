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
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)waddnstr.c	1.4	05/06/08 SMI"

/*
 * waddnstr.c
 * 
 * XCurses Library
 *
 * Copyright 1990, 1995 by Mortice Kern Systems Inc.  All rights reserved.
 *
 */

#if M_RCSID
#ifndef lint
static char rcsID[] = "$Header: /rd/src/libc/xcurses/rcs/waddnstr.c 1.3 1995/06/21 20:30:55 ant Exp $";
#endif
#endif

#include <private.h>
#include <limits.h>

int
waddnstr(w, str, n)
WINDOW *w;
const char *str;
int n;
{
	int i;
	cchar_t cc;
	short oflags;

#ifdef M_CURSES_TRACE
	__m_trace("waddnstr(%p, %p = \"%s\", %d)", w, str, str, n);
#endif

	if (n < 0)
		n = INT_MAX;

	/* Disable window flushing until the entire string has 
	 * been written into the window.
	 */
	oflags = w->_flags & (W_FLUSH | W_SYNC_UP);
	w->_flags &= ~(W_FLUSH | W_SYNC_UP);

	for ( ; *str != '\0' && 0 < n; n -= i, str += i) {
                if ((i = __m_mbs_cc(str, w->_bg._at, w->_bg._co, &cc)) < 0
		|| wadd_wch(w, &cc) == ERR)
			return __m_return_code("waddnstr", ERR);
        }

	w->_flags |= oflags;

	WSYNC(w);

	return __m_return_code("waddnstr", WFLUSH(w));
}
