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
 * Copyright 1997 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*
 * University Copyright- Copyright (c) 1982, 1986, 1988
 * The Regents of the University of California
 * All Rights Reserved
 *
 * University Acknowledgment- Portions of this document are derived from
 * software developed by the University of California, Berkeley, and its
 * contributors.
 */

#pragma ident	"@(#)ungetch.c	1.11	05/06/08 SMI"

/*LINTLIBRARY*/

#include <sys/types.h>
#include "curses_inc.h"

/* Place a char onto the beginning of the input queue. */

int
ungetch(int ch)
{
	int	i = cur_term->_chars_on_queue, j = i - 1;
	chtype	*inputQ = cur_term->_input_queue;

	/* Place the character at the beg of the Q */
	/*
	 * Here is commented out because 'ch' should deal with a single byte
	 * character only. So ISCBIT(ch) is 0 every time.
	 *
	 * register chtype	r;
	 *
	 * if (ISCBIT(ch)) {
	 *	r = RBYTE(ch);
	 *	ch = LBYTE(ch);
	 *	-* do the right half first to maintain the byte order *-
	 *	if (r != MBIT && ungetch(r) == ERR)
	 *		return (ERR);
	 * }
	 */

	while (i > 0)
		inputQ[i--] = inputQ[j--];
	cur_term->_ungotten++;
	inputQ[0] = -ch;
	cur_term->_chars_on_queue++;
	return (0);
}
