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
#pragma ident	"@(#)index.c	1.4	05/06/08 SMI"
	  /* from UCB 4.1 80/12/21 */

/*
 * Return the ptr in sp at which the character c appears;
 * NULL if not found
 */

#define	NULL	0

char *
index(sp, c)
	register char *sp, c;
{

	do {
		if (*sp == c)
			return (sp);
	} while (*sp++);
	return (NULL);
}
