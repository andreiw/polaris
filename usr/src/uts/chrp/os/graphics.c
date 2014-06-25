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
 */

#pragma ident	"@(#)graphics.c	1.3	06/02/01 SMI"

/*
 * So far, on PPC, we do not do any graphics.
 *
 * Just print out dots or baton twirls or something.
 * Maybe even just do nothing.
 *
 */

static void
progressbar_show(void)
{
	prom_printf(".");
}

void
progressbar_init()
{
	progressbar_show();
}

static void
progressbar_step()
{
	progressbar_show();
}

/*ARGSUSED*/
static void
progressbar_thread(void *arg)
{
}

void
progressbar_start(void)
{
}

void
progressbar_stop(void)
{
}
