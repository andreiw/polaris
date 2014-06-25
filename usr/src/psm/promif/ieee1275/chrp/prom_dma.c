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
 * Copyright (c) 2006 by Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 *
 * Copyright (c) 2006 Noah Yan <noah.yan@gmail.com>
 */

#pragma ident	"@(#)prom_dma.c	1.6	05/06/08 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

/*
 */
int
prom_dma_alloc(ihandle_t ifd, size_t size, unsigned long *virtaddr)
{
	cell_t ci[10];
	int rv;

	if ((ifd == (ihandle_t)-1))
		return (-1);

	ci[0] = p1275_ptr2cell("call-method");	/* Service name */
	ci[1] = (cell_t)3;			/* #argument cells */
	ci[2] = (cell_t)2;			/* #result cells */
	ci[3] = p1275_ptr2cell("dma-alloc");	/* Arg1: Method name */
	ci[4] = p1275_ihandle2cell(ifd);	/* Arg2: memory ihandle */
	ci[5] = p1275_size2cell(size);		/* Arg3: SA2: size */

	promif_preprom();
	rv = p1275_cif_handler(&ci);
	promif_postprom();

	if (rv != 0)
		return (rv);
	if (p1275_cell2int(ci[6]) != 0)		/* Res1: Catch result */
		return (-1);

	*virtaddr = p1275_cells2ull(ci[7], ci[7]);
				/* Res2: SR1: phys.hi ... Res3: SR2: phys.lo */
	return (0);
}

/*
 */
void
prom_dma_free(ihandle_t ifd, size_t size, unsigned long virtaddr)
{
	cell_t ci[8];

	if ((ifd == (ihandle_t)-1))
		return;

	ci[0] = p1275_ptr2cell("call-method");	/* Service name */
	ci[1] = (cell_t)4;			/* #argument cells */
	ci[2] = (cell_t)0;			/* #return cells */
	ci[3] = p1275_ptr2cell("dma-free");	/* Arg1: Method name */
	ci[4] = p1275_ihandle2cell(ifd);	/* Arg2: memory ihandle */
	ci[5] = p1275_size2cell(size);		/* Arg3: SA1: size */
	ci[6] = p1275_ull2cell_low(virtaddr);	/* Arg4: SA3: phys.lo */

	promif_preprom();
	(void) p1275_cif_handler(&ci);
	promif_postprom();
}

int
prom_dma_map_in(ihandle_t ifd, u_int cacheable, size_t size, unsigned long virtaddr, unsigned long *devaddr) 
{
	cell_t ci[8];

	if ((ifd == (ihandle_t)-1))
		return (-1);
	



	return (-1);
}

int
prom_dma_map_out(ihandle_t ifd, size_t size, 
		unsigned long devaddr, unsigned long virtaddr) 
{
	return (-1);
}

void
prom_dma_sync(ihandle_t ifd, size_t size, 
		unsigned long devaddr, unsigned long virtaddr)
{
	return;
}


