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

#ifndef _PCI_TOOLS_EXT_H
#define	_PCI_TOOLS_EXT_H

#pragma ident	"@(#)pci_tools_ext.h	1.1	05/10/27 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/* This file contains pcitool defs exported to other modules of a PCI driver. */

/*
 * Minor numbers for dedicated pcitool nodes.
 * Note that FF and FE minor numbers are used for other minor nodes.
 */
#define	PCI_TOOL_REG_MINOR_NUM	0xFD
#define	PCI_TOOL_INTR_MINOR_NUM	0xFC

/*
 * Functions exported from the pci_tools.c module.
 */
extern int pcitool_dev_reg_ops(dev_info_t *dip, void *arg, int cmd, int mode);
extern int pcitool_bus_reg_ops(dev_info_t *dip, void *arg, int cmd, int mode);
extern int pcitool_intr_admn(dev_info_t *dip, void *arg, int cmd, int mode);
extern int pcitool_init(dev_info_t *dip, boolean_t is_pciex);
extern void pcitool_uninit(dev_info_t *dip);

#ifdef	__cplusplus
}
#endif

#endif	/* _PCI_TOOLS_EXT_H */
