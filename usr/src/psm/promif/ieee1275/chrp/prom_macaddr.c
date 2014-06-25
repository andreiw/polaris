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
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prom_macaddr.c	1.6	96/03/14 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

/*
 * This function gets ihandle for network device and buffer as
 * arguments.
 */
#define	MAXPROPLEN	128
int
prom_getmacaddr(ihandle_t hd, caddr_t ea)
{
	phandle_t phndl;
	register int i, len;
	char buff[MAXPROPLEN];

	/* convert network device ihandle to phandle */
	phndl = prom_getphandle(hd);

	if ((len = prom_getproplen(phndl, "mac-address")) != -1) {
		/*
		 * try the mac-address property
		 */
		if (len <= MAXPROPLEN) {
			(void) prom_getprop(phndl, "mac-address", buff);
		} else {
			prom_printf(
			    "error retrieving mac-address property\n");
			return (-1);
		}
	} else if ((len = prom_getproplen(phndl, "local-mac-address")) != -1) {
		/*
		 * if the mac-address property is not present then try the
		 * local-mac-address.  It is debatable that this should ever
		 * happen.
		 */
		if (len <= MAXPROPLEN) {
			(void) prom_getprop(phndl, "local-mac-address", buff);
		} else {
			prom_printf(
			    "error retrieving local-mac-address property\n");
			return (-1);
		}
	} else {
		/* neither local-mac-address or mac-address exist */
		prom_printf(
		    "error retrieving mac-address\n");
		return (-1);
	}

	for (i = 0; i < len; i++) {
		ea[i] = buff[i];
	}
	return (0);
}
