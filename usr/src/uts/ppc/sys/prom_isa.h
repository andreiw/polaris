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
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright 2006 Sun Microsystems Laboratories.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef	_SYS_PROM_ISA_H
#define	_SYS_PROM_ISA_H

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#include <sys/obpdefs.h>

/*
 * This file contains external ISA-specific promif interface definitions.
 * There may be none.  This file is included by reference in <sys/promif.h>
 *
 * This version of the file is for 32 bit PowerPC implementations.
 */

#ifdef	__cplusplus
extern "C" {
#endif

typedef	unsigned long	cell_t;

#define	p1275_ptr2cell(p)	((cell_t)((void *)(p)))
#define	p1275_int2cell(i)	((cell_t)((int)(i)))
#define	p1275_uint2cell(u)	((cell_t)((unsigned int)(u)))
#define p1275_size2cell(u)      ((cell_t)((size_t)(u)))     /* taken from sparc v9 */
#define	p1275_phandle2cell(ph)	((cell_t)((phandle_t)(ph)))
#define	p1275_dnode2cell(d)	((cell_t)((pnode_t)(d)))
#define	p1275_ihandle2cell(ih)	((cell_t)((ihandle_t)(ih)))
#define	p1275_ull2cell_high(ll)	(0LL)
#define	p1275_ull2cell_low(ll)	((cell_t)(ll))
#define p1275_uintptr2cell(i)   ((cell_t)((uintptr_t)(i)))  /* taken from sparc v9 */

#define	p1275_cell2ptr(p)	((void *)((cell_t)(p)))
#define	p1275_cell2int(i)	((int)((cell_t)(i)))
#define	p1275_cell2uint(u)	((unsigned int)((cell_t)(u)))
#define p1275_cell2size(u)      ((size_t)((cell_t)(u)))     /* taken from sparc v9 */
#define	p1275_cell2phandle(ph)	((phandle_t)((cell_t)(ph)))
#define	p1275_cell2dnode(d)	((pnode_t)((cell_t)(d)))
#define	p1275_cell2ihandle(ih)	((ihandle_t)((cell_t)(ih)))
#define	p1275_cells2ull(h, l)	((unsigned long long)(cell_t)(l))
#define p1275_cell2uintptr(i)   ((uintptr_t)((cell_t)(i)))

#define p1275_cif_init                  p1275_ppc_cif_init
#define p1275_cif_handler               p1275_ppc_cif_handler

extern void     *p1275_ppc_cif_init(void *);
extern int	p1275_cif_handler(void *);

/* XXX - 2.6 may not need structure used for initializing callback handlers */

struct callbacks {
	char *name;		/* callback service name */
	int (*fn)();		/* handler for the service */
};

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_PROM_ISA_H */
