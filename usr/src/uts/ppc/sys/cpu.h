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
 * Copyright 2006 Cyril Plisko.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright 2006 Sun Microsystems Laboratories.  All rights reserved.
 * Use is subject to license terms.
 */


#ifndef _SYS_CPU_H
#define	_SYS_CPU_H

#pragma ident	"%Z%%M%	%I%	%E% SMI"

/*
 * The uts/intel/sys/cpu.h contains the next interesting text:
 * WARNING:
 *	This header file is Obsolete and may be deleted in a
 *	future release of Solaris.
 *
 * While uts/sparc/sys/cpu.h has nothing on obsoletion.
 * In any case PowerPC version is essentially empty for now.
 */


/*
 * Include generic bustype cookies.
 */
#include <sys/bustypes.h>

#ifdef	__cplusplus
extern "C" {
#endif


#define	CPU_ARCH	0xf0		/* mask for architecture bits */
#define	CPU_MACH	0x0f		/* mask for machine implementation */

#define	CPU_ANY		(CPU_ARCH|CPU_MACH)
#define	CPU_NONE	0

/*
 * PowerPC chip architectures
 */

#define	PPC_601_ARCH	0x10		/* arch value for 601 */
#define	PPC_603_ARCH	0x20		/* arch value for 603 */
#define	PPC_604_ARCH	0x30		/* arch value for 604 */
#define	PPC_620_ARCH	0x40		/* arch value for 620 */
#define	PPC_7447_ARCH	0x50		/* arch value for 7447 */
#define	PPC_7450_ARCH	0x60		/* arch value for MPC7450 */

/*
 * PowerPC platforms
 */

#define	PREP		0x01
#define CHRP            0x02		/* arch value for CHRP */
#define ODW             0x03		/* arch value for ODW */

#define	PREP_601	(PPC_601_ARCH + PREP)

#define	CPU_601		(PPC_601_ARCH + CPU_MACH)
#define	CPU_603		(PPC_603_ARCH + CPU_MACH)
#define	CPU_604		(PPC_604_ARCH + CPU_MACH)
#define	CPU_620		(PPC_620_ARCH + CPU_MACH)


/*
 * Used to insert cpu-dependent instructions into spin loops
 */


#define	SMT_PAUSE()

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_CPU_H */
