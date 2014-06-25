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

/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"%Z%%M%	%I%	%E% SMI"

/*
 * This file contains platform-dependent MMU support routines,
 * Use of these routines makes the caller platform-dependent,
 * since the caller assumes knowledge of the physical layout of
 * the machines address space.  Generic programs should use the
 * standard client interface memory allocators.
 * All the routines in this file deal with only physical memory
 * allocation, no mapping is setup by these functions.
 */

#include <sys/promif.h>
#include <sys/promimpl.h>

/*
 * If /chosen node has memory property implemented we will use this.
 */
ihandle_t
prom_memory_ihandle(void)
{
	static ihandle_t imemory;

	if (imemory != (ihandle_t)0)
		return (imemory);

	if (prom_getproplen(prom_chosennode(), "memory") != sizeof (ihandle_t))
		return (imemory = (ihandle_t)-1);

	(void) prom_getprop(prom_chosennode(), "memory", (caddr_t)(&imemory));
	imemory = (ihandle_t)prom_decode_int(imemory);
	return (imemory);
}

/*
 * Allocate physical memory, unmapped and possibly aligned.
 * Returns 0: Success; Non-zero: failure.
 * Returns *addr only if successful.
 *
 * This routine is suitable for platforms with 1-cell physical addresses
 * and a single size cell in the "memory" node.
 */
int
prom_allocate_phys(size_t size, uint_t align, uint_t *addr)
{
	cell_t ci[10];
	int rv;
	ihandle_t imemory = prom_memory_ihandle();

	if ((imemory == (ihandle_t)-1))
		return (-1);

	if (align == 0)
		align = (u_int)PROMIF_MIN_ALIGN;

	ci[0] = p1275_ptr2cell("call-method");	/* Service name */
	ci[1] = (cell_t)4;			/* #argument cells */
	ci[2] = (cell_t)2;			/* #result cells */
	ci[3] = p1275_ptr2cell("claim");	/* Arg1: Method name */
	ci[4] = p1275_ihandle2cell(imemory);	/* Arg2: mmu ihandle */
	ci[5] = p1275_uint2cell(align);		/* Arg3: align */
	ci[6] = p1275_uint2cell(size);		/* Arg4: size */

	promif_preprom();
	rv = p1275_cif_handler(&ci);
	promif_postprom();

	if (rv != 0)
		return (rv);
	if (p1275_cell2int(ci[7]) != 0)		/* Res1: Catch result */
		return (-1);

	*addr = p1275_cell2uint(ci[8]);	/* Res2: address allocated */
	return (0);
}

/*
 * Claim a region of physical memory, unmapped.
 * Returns 0: Success; Non-zero: failure.
 *
 * This routine is suitable for platforms with 1-cell physical addresses
 * and a single size cell in the "memory" node.
 */
int
prom_claim_phys(size_t size, uint_t addr)
{
	cell_t ci[10];
	int rv;
	ihandle_t imemory = prom_memory_ihandle();

	if ((imemory == (ihandle_t)-1))
		return (-1);

	ci[0] = p1275_ptr2cell("call-method");	/* Service name */
	ci[1] = (cell_t)5;			/* #argument cells */
	ci[2] = (cell_t)1;			/* #result cells */
	ci[3] = p1275_ptr2cell("claim");	/* Arg1: Method name */
	ci[4] = p1275_ihandle2cell(imemory);	/* Arg2: mmu ihandle */
	ci[5] = 0;				/* Arg3: align */
	ci[6] = p1275_uint2cell(size);		/* Arg4: len */
	ci[7] = p1275_uint2cell(addr);	/* Arg5: addr */

	promif_preprom();
	rv = p1275_cif_handler(&ci);
	promif_postprom();

	if (rv != 0)
		return (rv);
	if (p1275_cell2int(ci[8]) != 0)		/* Res1: Catch result */
		return (-1);

	return (0);
}

/*
 * Free physical memory (no unmapping is done).
 * This routine is suitable for platforms with 1-cell physical addresses
 * with a single size cell.
 */
void
prom_free_phys(size_t size, uint_t addr)
{
	cell_t ci[8];
	ihandle_t imemory = prom_memory_ihandle();

	if ((imemory == (ihandle_t)-1))
		return;

	ci[0] = p1275_ptr2cell("call-method");	/* Service name */
	ci[1] = (cell_t)4;			/* #argument cells */
	ci[2] = (cell_t)0;			/* #return cells */
	ci[3] = p1275_ptr2cell("release");	/* Arg1: Method name */
	ci[4] = p1275_ihandle2cell(imemory);	/* Arg2: mmu ihandle */
	ci[5] = p1275_uint2cell(size);		/* Arg3: size */
	ci[6] = p1275_uint2cell(addr);		/* Arg4: addr */

	promif_preprom();
	(void) p1275_cif_handler(&ci);
	promif_postprom();
}
