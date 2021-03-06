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
 * Copyright 1989-2002 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"@(#)sad_conf.c	1.11	05/06/08 SMI"

/*
 * Config dependent data structures for the Streams Administrative Driver
 * (or "Ballad of the SAD Cafe").
 */
#include <sys/types.h>
#include <sys/conf.h>
#include <sys/stream.h>
#include <sys/sad.h>
#include <sys/kmem.h>

#ifndef	NSAD
#define	NSAD	16
#endif

struct saddev *saddev;			/* sad device array */
int	sadcnt = NSAD;			/* number of sad devices */

/*
 * Auto-push structures.
 */
#ifndef NAUTOPUSH
#define	NAUTOPUSH	32
#endif
struct autopush *autopush;
int	nautopush = NAUTOPUSH;

void
sad_initspace(void)
{
	saddev = kmem_zalloc(sadcnt * sizeof (struct saddev), KM_SLEEP);
	autopush = kmem_zalloc(nautopush * sizeof (struct autopush), KM_SLEEP);
	strpcache = kmem_zalloc((strpmask + 1) * sizeof (struct autopush),
	    KM_SLEEP);
}
