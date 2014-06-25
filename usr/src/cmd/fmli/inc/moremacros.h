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


/****************************************************************************
*
*	MACRO's replacing 1 line functions
*
****************************************************************************/

#ident	"@(#)moremacros.h	1.6	05/06/08 SMI"       /* SVr4.0 1.3 */

extern	char	*strnsave();
#define strsave(s)	((s) ? strnsave(s, strlen(s)) : NULL )

extern	struct actrec		*AR_cur;
#define	ar_get_current()	AR_cur

extern	char	**Altenv;
extern	char	*getaltenv();
extern	void	copyaltenv();
extern	int	delaltenv();
extern	int	putaltenv();
#define	getAltenv(name)		getaltenv(Altenv, name)
#define	copyAltenv(an_env)	copyaltenv(an_env, &Altenv)
#define	delAltenv(name)		delaltenv(&Altenv, name)
#define	putAltenv(str)		putaltenv(&Altenv, str)
