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

#ifndef _SYS_KDI_MACHIMPL_H
#define	_SYS_KDI_MACHIMPL_H

#pragma ident	"%Z%%M%	%I%	%E% CP"

/*
 * The Kernel/Debugger interface.  The operations provided by the kdi_t,
 * defined below, comprise the Debugger -> Kernel portion of the interface,
 * and are to be used only when the system has been stopped.
 */


#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct kdi_mach {
	/* Place holders mostly */
	void (*mkdi_tickwait)(clock_t);

	int (*mkdi_get_stick)(uint64_t *);

	void (*mkdi_kernpanic)(struct regs *, uint_t);

	void (*mkdi_cpu_init)(int, int, int, int);
} kdi_mach_t;

#ifdef __cplusplus
}
#endif

#endif /* _SYS_KDI_MACHIMPL_H */
