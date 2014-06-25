/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
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
 * Copyright 2006 Sun Microsystems Laboratories.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"%Z%%M%	%I%	%E% CP"

/* #pragma	ident	"@(#)prom_ppc.c	1.2	95/05/07 SMI" */

#include <sys/promif.h>
#include <sys/promimpl.h>

/*
 * P1275 Client Interface Functions defined for PowerPC.
 * We have this interface to closely follow sparc implementation.
 * This file belongs in a platform dependent area.
 */

/*
 * This function returns NULL or a verified client interface structure
 * pointer to the caller.  We need to verify the cookie somehow.
 */

static int (*cif_handler)(void *);

void *
p1275_ppc_cif_init(void *cookie)
{
	cif_handler = (int (*)(void *))cookie;
	return ((void *)cookie);
}


int
p1275_ppc_cif_handler(void *p)
{
	cell_t rv;

	if (cif_handler == NULL)
		return (-1);

	rv = (*cif_handler)(p);
	return (p1275_cell2int(rv));
}
