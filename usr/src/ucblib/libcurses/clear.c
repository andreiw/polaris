/*
 * Copyright 2001 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*	Copyright (c) 1983, 1984, 1985, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#pragma ident	"@(#)clear.c	1.6	05/06/02 SMI"

/*LINTLIBRARY*/

#ifndef lint
static char
sccsid[] = "@(#)clear.c 1.6 88/02/08 SMI"; /* from UCB 5.1 85/06/07 */
#endif /* not lint */

#include	"curses.ext"

/*
 *	This routine clears the window.
 */

int
wclear(WINDOW *win)
{
	(void) werase(win);
	win->_clear = TRUE;
	return (OK);
}
