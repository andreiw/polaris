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

#ifndef _SYS_DEBUGREG_H
#define	_SYS_DEBUGREG_H

#pragma ident	"@(#)debugreg.h	1.2	94/02/10 SMI"
#pragma ident	"%Z%%M%	%I%	%E% SMI"

/*
 * This file is a place holder for future implementation
 * of hw debugregs support.
 */

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Maximum number of debugregs in any implementation of PowerPC.
 *	CPU_601		3
 *	CPU_604		?
 */
#define	NDEBUGREG	3

typedef struct dbregset {
	unsigned int	debugreg[NDEBUGREG];
} dbregset_t;

#define	hid_cpu601_1	debugreg[0]
#define	hid_cpu601_2	debugreg[1]
#define	hid_cpu601_5	debugreg[2]

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_DEBUGREG_H */
