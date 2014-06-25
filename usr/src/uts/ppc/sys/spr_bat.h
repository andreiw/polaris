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


#ifndef _SPR_BAT_H
#define	_SPR_BAT_H

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * PPC Special Purpose Register (SPR) number encoding for BAT registers.
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
 * SPR numbers for BAT registers
 *   MPCFPE32B  page E-23
 *   MPC7450UM  page C-2 .. C-3
 *
 *
 * IBAT[0-3]U = SPR 528 + (2 * n)
 * IBAT[0-3]L = SPR 529 + (2 * n)
 * DBAT[0-3]U = SPR 536 + (2 * n)
 * DBAT[0-3]L = SPR 537 + (2 * n)
 *
 * IBAT[4-7]U = SPR 560 + (2 * n)
 * IBAT[4-7]L = SPR 561 + (2 * n)
 * DBAT[4-7]U = SPR 568 + (2 * n)
 * DBAT[4-7]L = SPR 569 + (2 * n)
 *
 */

/*
 * These values are the maximum potentially supported by any known processor,
 * without taking into account the fact that to some models that may support
 * fewer BAT registers, or that some models support a varying number of BAT
 * registers, depending on machine state, such as HID0/HIGH_BAT_EN on MPC7450,
 * for example.
 */

#define	MAX_IBATS 8
#define	MAX_DBATS 8
#define	MAX_BATS (MAX_IBATS + MAX_DBATS)

#define	IBAT_T 0
#define	DBAT_T 1

#define	IBAT0U	528
#define	IBAT0L	529
#define	IBAT1U	530
#define	IBAT1L	531
#define	IBAT2U	532
#define	IBAT2L	533
#define	IBAT3U	534
#define	IBAT3L	535

#define	DBAT0U	536
#define	DBAT0L	537
#define	DBAT1U	538
#define	DBAT1L	539
#define	DBAT2U	540
#define	DBAT2L	541
#define	DBAT3U	542
#define	DBAT3L	543

#define	IBAT4U	560
#define	IBAT4L	561
#define	IBAT5U	562
#define	IBAT5L	563
#define	IBAT6U	564
#define	IBAT6L	565
#define	IBAT7U	566
#define	IBAT7L	567

#define	DBAT4U	568
#define	DBAT4L	569
#define	DBAT5U	570
#define	DBAT5L	571
#define	DBAT6U	572
#define	DBAT6L	573
#define	DBAT7U	574
#define	DBAT7L	575

#ifdef __cplusplus
}
#endif

#endif /* _SPR_BAT_H */
