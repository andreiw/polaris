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
 *
 * Copyright 2006 Sun Microsystems Laboratories.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * sysconfig_impl.h:
 *
 * platform-specific variables for the SUN private sysconfig syscall
 *
 */

#ifndef _SYS_SYSCONFIG_IMPL_H
#define	_SYS_SYSCONFIG_IMPL_H

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#include <sys/mmu.h>		/* for dcache size etc */

#ifdef	__cplusplus
extern "C" {
#endif

extern int dcache_sets;		/* associativity  1,2,3 etc */
extern int icache_sets;		/* associativity  1,2,3 etc */
extern int timebase_frequency;
extern int lwarx_size;		/* bytes */
extern int coherency_size;	/* bytes */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SYSCONFIG_IMPL_H */
