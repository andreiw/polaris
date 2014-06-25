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
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"@(#)can_use_af.c	1.5	06/01/04 SMI"

#include "mt.h"
#include "rpc_mt.h"
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <net/if.h>
#include <sys/sockio.h>

/*
 * Determine if we have a configured interface for the specified address
 * family.
 */
int
__can_use_af(sa_family_t af)
{
	struct lifnum	lifn;
	int		fd;

	if ((fd =  open("/dev/udp", O_RDONLY)) < 0)
		return (0);
	lifn.lifn_family = af;
	/* XXX Remove this after fixing 6204567 */
	/* CONSTCOND */
	lifn.lifn_flags = IFF_UP & !(IFF_NOXMIT | IFF_DEPRECATED);
	if (ioctl(fd, SIOCGLIFNUM, &lifn, sizeof (lifn)) < 0)
		lifn.lifn_count = 0;

	(void) close(fd);
	return (lifn.lifn_count);
}
