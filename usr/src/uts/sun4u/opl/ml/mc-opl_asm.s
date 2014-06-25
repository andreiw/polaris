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
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */
/*
 * All Rights Reserved, Copyright (c) FUJITSU LIMITED 2006
 */

#pragma ident	"@(#)mc-opl_asm.s	1.1	06/08/03 SMI"

#if defined(lint)
#include <sys/types.h>
#else
#include <sys/asm_linkage.h>
#endif	/* lint */

#if defined(lint)

/* ARGSUSED */
void
mc_prefetch(uint64_t va)
{ return; }

#else
	/* issue a strong prefetch to cause MI error */
	ENTRY(mc_prefetch)
	prefetch	[%o0],0x16
	retl
	nop
	SET_SIZE(mc_prefetch)
#endif
