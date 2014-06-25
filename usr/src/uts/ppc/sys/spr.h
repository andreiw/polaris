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


#ifndef _SPR_H
#define	_SPR_H

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#include <sys/spr_bat.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * PPC Special Purpose Register (SPR) number encoding.
 *
 * Documents
 * ---------
 * MPCFPE32B:
 *   Programming Environments Manual for 32-Bit Implementations
 *   of the PowerPC(TM) Architecture
 *   Rev. 3
 *   2005/09
 *
 * MPC7450UM:
 *   MPC7450 RISC Microprocessor Reference Manual
 *   Rev. 5
 *   2005/01
 *
 * PVR, PIR:
 *   MPCFPE32B  page 2-2
 *   MPC7450UM  page 1-37
 * SDR1:
 *   MPCFPE32B  page 2-2, page 7-39 .. 7-40
 *   MPC7450UM  page 5-49
 * HID0, HID1:
 *   MPC7450UM  page 1-37
 *
 */

#define	SPR_DSISR	  18
#define	SPR_DAR		  19
#define	SPR_SRR0	  26
#define	SPR_SRR1	  27
#define	SPR_DMISS	 976
#define	SPR_DCMP	 977
#define	SPR_IMISS	 980
#define	SPR_ICMP	 981
#define	SPR_G0		 272
#define	SPR_G1		 273
#define	SPR_G2		 274
#define	SPR_G3		 275

#define	SPR_PVR   287	/* Processor Version Register */
#define	SPR_PIR  1023	/* Processor ID Register */
#define	SPR_HID0 1008	/* Hardware Implementation-Dependent Register 0 */
#define	SPR_HID1 1009	/* Hardware Implementation-Dependent Register 1 */

#define	SPR_SDR1   25


#ifdef __cplusplus
}
#endif

#endif /* _SPR_H */
