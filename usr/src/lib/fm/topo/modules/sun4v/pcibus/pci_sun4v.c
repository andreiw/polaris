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

#pragma ident	"@(#)pci_sun4v.c	1.2	06/05/09 SMI"

#include <fm/topo_mod.h>

/*
 * Including the following file gives us definitions of the three
 * global arrays used to adjust labels, Slot_Rewrites, Physlot_Names,
 * and Missing_Names.  With those defined we can use the common labeling
 * routines for pci.
 */
#include "pci_sun4v.h"

int
platform_pci_label(tnode_t *node, nvlist_t *in, nvlist_t **out, topo_mod_t *mod)
{
	return (pci_label_cmn(node, in, out, mod));
}