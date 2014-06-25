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
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/


/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)menu.c	1.3	05/06/08 SMI"

/*
 *	cscope - interactive C symbol cross-reference
 *
 *	mouse menu functions
 */

#include "global.h"	/* changing */

static	MOUSEMENU mainmenu[] = {
	"Send",		"##\033s##\r",
	"Repeat",	"\01",
	"Rebuild",	"\022",
	"Help",		"?",
	"Exit",		"\04",
	0,		0
};

static	MOUSEMENU changemenu[] = {
	"Mark Screen",	"*",
	"Mark All",	"a",
	"Change",	"\04",
	"Continue",	"\b",	/* key that will be ignored at Select prompt */
	"Help",		"?",
	"Exit",		"\r",
	0,		0
};

/* initialize the mouse menu */

void
initmenu(void)
{
	if (changing == YES) {
		downloadmenu(changemenu);
	} else {
		downloadmenu(mainmenu);
	}
}