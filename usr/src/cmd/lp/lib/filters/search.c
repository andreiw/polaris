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


#ident	"@(#)search.c	1.5	05/06/08 SMI"	/* SVr4.0 1.4	*/
/* EMACS_MODES: !fill, lnumb, !overwrite, !nodelete, !picture */

#include "string.h"

#include "lp.h"
#include "filters.h"

/**
 ** search_filter() - SEARCH INTERNAL FILTER TABLE FOR FILTER BY NAME
 **/

_FILTER *
#if	defined(__STDC__)
search_filter (
	char *			name
)
#else
search_filter (name)
	register char		*name;
#endif
{
	register _FILTER	*pf;

	for (pf = filters; pf->name; pf++)
		if (STREQU(pf->name, name))
			break;
	return (pf->name? pf : 0);
}
