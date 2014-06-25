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
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"@(#)_lgrp_home_fast.s	1.5	05/06/08 SMI"

	.file "_lgrp_home_fast.s"

/*
 * C library -- gethomelgroup
 * lgrpid_t gethomelgroup()
 * lgrp_id_t _lgrp_home_fast()
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(gethomelgroup,function)

#include "SYS.h"

/*
 * lgrp_id_t _lgrp_home_fast(void)
 * lgrpid_t gethomelgroup(void)
 *
 * Returns the home lgroup id for caller using fast trap
 * XXX gethomelgroup() being replaced by lgrp_home() XXX
 */

	ENTRY2(_lgrp_home_fast,_gethomelgroup)
	SYSFASTTRAP(GETLGRP)		/* share fast trap with getcpuid */
	RET2				/* return rval2 */
	SET_SIZE(_lgrp_home_fast)
	SET_SIZE(_gethomelgroup)