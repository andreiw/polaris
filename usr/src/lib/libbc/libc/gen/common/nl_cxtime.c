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
 * Copyright 1993 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"@(#)nl_cxtime.c	1.6	05/06/08 SMI"


#include <stdio.h>
#include <time.h>

#define TBUFSIZE 128
char _tbuf[TBUFSIZE];

char *
nl_cxtime(clk, fmt)
	struct tm *clk;
	char *fmt;
{
	char *nl_ascxtime();
	return (nl_ascxtime(localtime(clk), fmt));
}

char *
nl_ascxtime(tmptr, fmt) 
	struct tm *tmptr;
	char *fmt;
{
	return (strftime (_tbuf, TBUFSIZE, fmt ? fmt : "%H:%M:%S", tmptr) ?
			 _tbuf : asctime(tmptr));
}
