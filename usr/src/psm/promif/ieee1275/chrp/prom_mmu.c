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
 * Copyright 2006 Sun Microsystems Laboratories.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"%Z%%M%	%I%	%E% SML"


/*
 * This file contains platform-dependent MMU support routines,
 * suitable for mmu methods with 1-cell physical addresses.
 * Use of these routines makes the caller platform-dependent,
 * since the caller assumes knowledge of the physical layout of
 * the machines address space.  Generic programs should use the
 * standard client interface memory allocators.
 */

#include <sys/promif.h>
#include <sys/promimpl.h>

/*
 * with the assumption that chosen node's mmu property is really
 * cpu node's ihandle. if this is not so, this function needs to be changed.
 */
ihandle_t
prom_mmu_ihandle(void)
{
	static ihandle_t immu;

	if (immu != (ihandle_t)0)
		return (immu);

	if (prom_getproplen(prom_chosennode(), "mmu") != sizeof (ihandle_t))
		return (immu = (ihandle_t)-1);

	(void) prom_getprop(prom_chosennode(), "mmu", (caddr_t)(&immu));
	immu = (ihandle_t)prom_decode_int(immu);
	return (immu);
}

/*
 * prom_map_phys:
 *
 * Create an MMU mapping for a given physical address to a given virtual
 * address. The given resources are assumed to be owned by the caller,
 * and are *not* removed from any free lists.
 *
 * This routine is suitable for mapping a 1-cell physical address.
 * phys_hi not currently used.
 */

/* ARGSUSED4 */
int
prom_map_phys(int mode, size_t size, caddr_t virt,
	uint_t phys_hi, uint_t phys_lo)
{
	cell_t ci[11];
	int rv;
	ihandle_t immu = prom_mmu_ihandle();

	if (phys_hi != (uint_t)0)
		prom_panic("prom_map_phys: non-NULL value in phys_hi\n");

	if ((immu == (ihandle_t)-1))
		return (-1);

	ci[0] = p1275_ptr2cell("call-method");	/* Service name */
	ci[1] = (cell_t)7;			/* #argument cells */
	ci[2] = (cell_t)1;			/* #result cells */
	ci[3] = p1275_ptr2cell("map");		/* Arg1: method name */
	ci[4] = p1275_ihandle2cell(immu);	/* Arg2: mmu ihandle */
	ci[5] = p1275_int2cell(mode);		/* Arg3: mode */
	ci[6] = p1275_uint2cell(size);		/* Arg4: size */
	ci[7] = p1275_ptr2cell(virt);		/* Arg5: virt */
	ci[8] = p1275_uint2cell(phys_lo);	/* Arg6: addr */
	ci[9] = p1275_uint2cell(phys_lo);	/* Arg6: addr */

	promif_preprom();
	rv = p1275_cif_handler(&ci);
	promif_postprom();

	if (rv != 0)
		return (-1);
	if (ci[10] != 0)			/* Res1: Catch result */
		return (-1);
	return (0);
}

void
prom_unmap_phys(size_t size, caddr_t virt)
{
	(void) prom_unmap_virt(size, virt);
}

/*
 * Allocate aligned or unaligned virtual address space, unmapped.
 */
caddr_t
prom_allocate_virt(uint_t align, size_t size)
{
	cell_t ci[9];
	int rv;
	ihandle_t immu = prom_mmu_ihandle();

	if ((immu == (ihandle_t)-1))
		return ((caddr_t)-1);

	if (align == 0)
		align = PROMIF_MIN_ALIGN;

	ci[0] = p1275_ptr2cell("call-method");	/* Service name */
	ci[1] = (cell_t)4;			/* #argument cells */
	ci[2] = (cell_t)2;			/* #result cells */
	ci[3] = p1275_ptr2cell("claim");	/* Arg1: Method name */
	ci[4] = p1275_ihandle2cell(immu);	/* Arg2: mmu ihandle */
	ci[5] = p1275_uint2cell(align);		/* Arg3: align */
	ci[6] = p1275_uint2cell(size);		/* Arg4: size */

	promif_preprom();
	rv = p1275_cif_handler(&ci);
	promif_postprom();

	if (rv != 0)
		return ((caddr_t)-1);
	if (ci[7] != 0)				/* Res1: Catch result */
		return ((caddr_t)-1);
	return (p1275_cell2ptr(ci[8]));		/* Res2: base */
}

/*
 * Claim a region of virtual address space, unmapped.
 */
caddr_t
prom_claim_virt(size_t size, caddr_t virt)
{
	cell_t ci[10];
	int rv;
	ihandle_t immu = prom_mmu_ihandle();

	if ((immu == (ihandle_t)-1))
		return ((caddr_t)-1);

	ci[0] = p1275_ptr2cell("call-method");	/* Service name */
	ci[1] = (cell_t)5;			/* #argument cells */
	ci[2] = (cell_t)2;			/* #result cells */
	ci[3] = p1275_ptr2cell("claim");	/* Arg1: Method name */
	ci[4] = p1275_ihandle2cell(immu);	/* Arg2: mmu ihandle */
	ci[5] = (cell_t)0;			/* Arg3: align */
	ci[6] = p1275_uint2cell(size);		/* Arg4: length */
	ci[7] = p1275_ptr2cell(virt);		/* Arg5: virt */

	promif_preprom();
	rv = p1275_cif_handler(&ci);
	promif_postprom();

	if (rv != 0)
		return ((caddr_t)-1);
	if (ci[8] != 0)				/* Res1: Catch result */
		return ((caddr_t)-1);
	return (p1275_cell2ptr(ci[9]));		/* Res2: base */
}

/*
 * Free virtual address resource (no unmapping is done).
 */
void
prom_free_virt(size_t size, caddr_t virt)
{
	cell_t ci[7];
	ihandle_t immu = prom_mmu_ihandle();

	if ((immu == (ihandle_t)-1))
		return;

	ci[0] = p1275_ptr2cell("call-method");	/* Service name */
	ci[1] = (cell_t)4;			/* #argument cells */
	ci[2] = (cell_t)0;			/* #return cells */
	ci[3] = p1275_ptr2cell("release");	/* Arg1: Method name */
	ci[4] = p1275_ihandle2cell(immu);	/* Arg2: mmu ihandle */
	ci[5] = p1275_uint2cell(size);		/* Arg3: length */
	ci[6] = p1275_ptr2cell(virt);		/* Arg4: virt */

	promif_preprom();
	(void) p1275_cif_handler(&ci);
	promif_postprom();
}

/*
 * Un-map virtual address. Does not free underlying resources.
 */
void
prom_unmap_virt(size_t size, caddr_t virt)
{
	cell_t ci[7];
	ihandle_t immu = prom_mmu_ihandle();

	if ((immu == (ihandle_t)-1))
		return;

	ci[0] = p1275_ptr2cell("call-method");	/* Service name */
	ci[1] = (cell_t)4;			/* #argument cells */
	ci[2] = (cell_t)0;			/* #result cells */
	ci[3] = p1275_ptr2cell("unmap");	/* Arg1: Method name */
	ci[4] = p1275_ihandle2cell(immu);	/* Arg2: mmu ihandle */
	ci[5] = p1275_uint2cell(size);		/* Arg3: size */
	ci[6] = p1275_ptr2cell(virt);		/* Arg4: virt */

	promif_preprom();
	p1275_cif_handler(&ci);
	promif_postprom();
}

int
prom_translate_virt(caddr_t virt, int *valid, pfn_t *phys_addr, int *mode)
{
	cell_t ci[10];
	int rv;
	ihandle_t immu = prom_mmu_ihandle();

	*valid = 0;

	if ((immu == (ihandle_t)-1))
		return (-1);

	ci[0] = p1275_ptr2cell("call-method");	/* Service name */
	ci[1] = (cell_t)3;			/* #argument cells */
	ci[2] = (cell_t)4;			/* #result cells */
	ci[3] = p1275_ptr2cell("translate");	/* Arg1: method name */
	ci[4] = p1275_ihandle2cell(immu);	/* Arg2: mmu ihandle */
	ci[5] = p1275_ptr2cell(virt);		/* Arg3: virt address */
	ci[6] = 0;				/* Res1: catch-result */
	ci[7] = 0;				/* Res2: sr1: valid */

	promif_preprom();
	rv = p1275_cif_handler(&ci);
	promif_postprom();

	if (rv != 0) {
		return (-1);
	}
	if (ci[6] != 0)	{			/* Res1: Catch result */
		return (-1);
	}
	if (p1275_cell2int(ci[7]) == 0) {	/* valid result? */
		return (0);
	}

	*mode = p1275_cell2int(ci[8]);
	*phys_addr = (pfn_t)p1275_cell2uint(ci[9]);
	*valid = -1;

	return (0);
}