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
/*      Copyright (c) 1984 AT&T */
/*        All Rights Reserved   */

#pragma ident	"@(#)cuserid.c	1.8	05/06/08 SMI"  /* from S5R2 1.3 */

/*LINTLIBRARY*/
#include <stdio.h>
#include <pwd.h>

extern char *strcpy(), *getlogin();
extern int getuid();
extern struct passwd *getpwuid();
static char res[L_cuserid];

char *
cuserid(s)
char	*s;
{
	register struct passwd *pw;
	register char *p;

	if (s == NULL)
		s = res;
	p = getlogin();
	if (p != NULL)
		return (strcpy(s, p));
	pw = getpwuid(getuid());
	endpwent();
	if (pw != NULL)
		return (strcpy(s, pw->pw_name));
	*s = '\0';
	return (NULL);
}
