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
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"@(#)csetlen.c	1.8	05/06/08 SMI"

#include	"synonyms.h"
#include	<sys/types.h>
#include	<ctype.h>
#include	<euc.h>

int
csetlen(int cset)
{
	switch (cset) {
	case 0:
		return (1);
	case 1:
		return (eucw1);
	case 2:
		return (eucw2);
	case 3:
		return (eucw3);
	default:
		return (0);
	}
}


int
csetcol(int cset)
{
	switch (cset) {
	case 0:
		return (1);
	case 1:
		return (scrw1);
	case 2:
		return (scrw2);
	case 3:
		return (scrw3);
	default:
		return (0);
	}
}