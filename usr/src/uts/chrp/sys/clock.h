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

#ifndef _SYS_CLOCK_H
#define	_SYS_CLOCK_H

#pragma ident	"%Z%%M%	%I%	%E% CP"

/*
 * Numerous hw architecture specific clock definitions
 * XXX PPC Needs massive work !
 */

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_KERNEL) && !defined(_ASM)

#include <sys/time.h>

extern int     timebase_period;        /* nanoseconds * 2^NSEC_SHIFT / tb */
extern int     tbticks_per_10usec;     /* timebase ticks for a 10 us delay */
extern int     dec_incr_per_tick;      /* decrementer increments per HZ */

#endif	/* _KERNEL && !_ASM*/

#define NANOSEC 1000000000
#define ADJ_SHIFT 4             /* used in get_hrestime and clock_intr() */

/*
 * The following shifts are used to scale the timebase in order to reduce
 * the error introduced by integer math.  For example, if the timebase_freq
 * is 16.5 MHz, this corresponds to an update interval (1/f) of 60.6060...
 * nanoseconds/time-base-incr.  If the timebase is simply multiplied by
 * 60, we get a 1% error (43 minutes slow after 3 days).  So, instead
 * scale the multiplier by (2^11) and then divide by (2^11) afterwards,
 * reducing the error to less than 1 ppm for reasonable frequencies.
 */
#define NSEC_SHIFT      11      /* multiplier scale */
#define NSEC_SHIFT1     4       /* pre-multiply scale to avoid overflow */
#define NSEC_SHIFT2     (NSEC_SHIFT-NSEC_SHIFT1) /* post multiply shift */

#define YRBASE          00      /* 1900 - what year 0 in chip represents */


#define ADJ_SHIFT	4	/* used in get_hrestime */

#define NSEC_SHIFT      11

#ifdef __cplusplus
}
#endif

#endif /* _SYS_CLOCK_H */
