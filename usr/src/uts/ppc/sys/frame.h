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

#ifndef _SYS_FRAME_H
#define	_SYS_FRAME_H

#pragma ident	"@(#)frame.h	1.2	94/02/10 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Definition of the PowerPC stack frame (when it is pushed on the stack).
 * This is only used in kadb's _setjmp/_longjmp.
 */

struct frame {
	struct frame	*fr_savfp;	/* saved frame pointer */
	int	fr_savpc;		/* saved program counter */
	int	fr_cr;			/* saved condition reg. */
	int	fr_nonvols[20];		/* r2,r13-r31 */
};

/*
 * Structure definition for minimum stack frame.
 */
struct minframe {
	struct minframe	*fr_savfp;	/* saved frame pointer */
	int	fr_savpc;		/* saved program counter */
};

typedef struct minframe minframe_t;

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_FRAME_H */
