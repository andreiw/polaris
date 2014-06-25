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

#ifndef _SYS_CPU_FEATURE_H
#define	_SYS_CPU_FEATURE_H

#pragma ident	"%Z%%M%	%I%	%E% SMI"

/*
 * PowerPC CPU features for some known models.
 */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * CPU_FEATURE_HBEN:
 *   This model is capable of using more BAT registers, and
 *   HID0[HID0_HBEN] enables them.
 *
 * CPU_FEATURE_XBSEN:
 *   This model is capable of using extended BAT lengths, and
 *   HID0[HID0_XBSEN] enables that feature.
 */

#define	CPU_FEATURE_HBEN	0x00000001
#define	CPU_FEATURE_XBSEN	0x00000002

#ifdef __cplusplus
}
#endif

#endif /* _SYS_CPU_FEATURE_H */
