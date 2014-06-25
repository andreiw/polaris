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


#ifndef	_SYS_PROM_PLAT_H
#define	_SYS_PROM_PLAT_H

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * This file contains external platform-specific promif interface definitions
 * for the 32 bit PowerPC platform with IEEE 1275-1994 compliant prom.
 */

/*
 * "reg"-format for 32 bit cell-size, 1-cell physical addresses,
 * with a single 'size' cell:
 */

struct prom_reg {
	unsigned int addr, size;
};


extern	caddr_t		prom_malloc(caddr_t virt, size_t size, uint_t align);

extern	caddr_t		prom_allocate_virt(uint_t align, size_t size);
extern	caddr_t		prom_claim_virt(size_t size, caddr_t virt);
extern	void		prom_free_virt(size_t size, caddr_t virt);

extern	int		prom_map_phys(int mode, size_t size, caddr_t virt,
				uint_t pa_hi, uint_t pa_lo);
extern	int		prom_allocate_phys(size_t size, uint_t align,
				uint_t *addr);
extern	int		prom_claim_phys(size_t size, uint_t addr);
extern	void		prom_free_phys(size_t size, uint_t addr);

extern	int		prom_getmacaddr(ihandle_t hd, caddr_t ea);

extern	void		prom_unmap_phys(size_t size, caddr_t virt);
extern	void		prom_unmap_virt(size_t size, caddr_t virt);

/*
 * prom_translate_virt returns the physical address and virtualized "mode"
 * for the given virtual address. After the call, if *valid is non-zero,
 * a mapping to 'virt' exists and the physical address and virtualized
 * "mode" were returned to the caller.
 */
extern	int		prom_translate_virt(caddr_t virt, int *valid,
				pfn_t *physaddr, int *mode);
#ifdef	__cplusplus
}
#endif

#endif /* _SYS_PROM_PLAT_H */
