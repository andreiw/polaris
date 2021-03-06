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
 * Copyright 1996 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"@(#)connect.c	1.8	05/09/30 SMI"

#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>

/* SVR4 stream operation macros */
#define	STR		('S'<<8)
#define	I_SWROPT	(STR|023)
#define	SNDPIPE		0x002

int
connect(int s, struct sockaddr *name, int namelen)
{
	int	a;

	if ((a = _connect(s, name, namelen)) == -1)
		maperror();
	return (a);
}
