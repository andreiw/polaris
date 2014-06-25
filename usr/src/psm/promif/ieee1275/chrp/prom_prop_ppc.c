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


#pragma	ident	"@(#)prom_prop_ppc.c	1.4	95/07/14 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

int
prom_getintprop(pnode_t nodeid, caddr_t name, int *value)
{
	int	len;

	len = prom_getprop(nodeid, name, (caddr_t)value);
	if (len == sizeof (int))
		*value = prom_decode_int(*value);
	return (len);
}

/*
 * Decode an "encode-phys" property. On PPC OpenFirmware, addresses
 * are always only 32 bit, so "encode-phys" and "encode-int" are really same.
 */
u_int
prom_decode_phys(u_int value)
{
	return (prom_decode_int(value));
}
