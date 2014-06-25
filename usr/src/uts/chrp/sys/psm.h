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


#ifndef	_SYS_PSM_H
#define	_SYS_PSM_H


#pragma ident	"%Z%%M%	%I%	%E% SML"

/*
 * Platform Specific Module (PSM)
 */

/*
 * Include the loadable module wrapper.
 */
#include <sys/types.h>
#include <sys/conf.h>
#include <sys/modctl.h>
#include <sys/sunddi.h>
#include <sys/kmem.h>
#include <sys/psm_defs.h>
#include <sys/psm_types.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * PSM External Interfaces
 */
extern int psm_mod_init(void **, struct psm_info *);
extern int psm_mod_fini(void **, struct psm_info *);
extern int psm_mod_info(void **, struct psm_info *, struct modinfo *);

extern int psm_add_intr(int, avfunc, char *, int, caddr_t);
extern int psm_add_nmintr(int, avfunc, char *, caddr_t);
extern processorid_t psm_get_cpu_id(void);

/* map physical address							*/
extern caddr_t psm_map(uint32_t addr, size_t len, int prot);
/* unmap the physical address return from psm_map_phys()		*/
extern void psm_unmap(caddr_t addr, size_t len, ulong prot);

#define	PSM_PROT_READ		0x0000
#define	PSM_PROT_WRITE		0x0001
#define	PSM_PROT_SAMEADDR 	0x0002
#define	PSM_PROT_CACHE		0x0004

/* handle memory error */
extern void psm_handle_memerror(uint32_t);

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PSM_H */
