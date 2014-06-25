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
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
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

#pragma ident	"@(#)setcurterm.c	1.10	05/08/10 SMI"

/*LINTLIBRARY*/

#include <sys/types.h>
#include "curses_inc.h"

/*
 * Establish the terminal that the #defines in term.h refer to.
 */

TERMINAL *
setcurterm(TERMINAL *newterminal)
{
	TERMINAL	*oldterminal = cur_term;

	if (newterminal) {
#ifdef	_VR3_COMPAT_CODE
		acs_map = newterminal->_acs32map;
#else	/* _VR3_COMPAT_CODE */
		acs_map = newterminal->_acsmap;
#endif	/* _VR3_COMPAT_CODE */
		cur_bools = newterminal->_bools;
		cur_nums = newterminal->_nums;
		cur_strs = newterminal->_strs;
		cur_term = newterminal;
	}
	return (oldterminal);
}
