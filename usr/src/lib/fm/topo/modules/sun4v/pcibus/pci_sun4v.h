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
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef _PCI_SUN4V_H
#define	_PCI_SUN4V_H

#pragma ident	"@(#)pci_sun4v.h	1.1	06/02/11 SMI"

#include "pcibus_labels.h"

#ifdef __cplusplus
extern "C" {
#endif

physnm_t t200_pnms[] = {
	/* Slot #, Label */
	{ 224, "PCIE0" },
	{ 225, "PCIE1" },
	{ 226, "PCIE2" }
};

pphysnm_t plat_pnames[] = {
	{ "SUNW,Sun-Fire-T200",
	    sizeof (t200_pnms) / sizeof (physnm_t),
	    t200_pnms }
};

physlot_names_t PhyslotNMs = {
	1,
	plat_pnames
};

devlab_t t200_missing[] = {
	/* board, bridge, root-complex, bus, dev, label */
	{ 0, 0, 1 - TO_PCI, 6, 1, "PCIX1" },
	{ 0, 0, 1 - TO_PCI, 6, 2, "PCIX0" }
};

pdevlabs_t plats_missing[] = {
	{ "SUNW,Sun-Fire-T200",
	    sizeof (t200_missing) / sizeof (devlab_t),
	    t200_missing }
};

missing_names_t Missing = {
	1,
	plats_missing
};

slotnm_rewrite_t *Slot_Rewrites = NULL;
physlot_names_t *Physlot_Names = &PhyslotNMs;
missing_names_t *Missing_Names = &Missing;

#ifdef __cplusplus
}
#endif

#endif /* _PCI_SUN4V_H */
