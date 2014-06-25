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

#pragma ident	"@(#)kmdb_shell.c	1.2	05/06/08 SMI"

#include <mdb/mdb_shell.h>
#include <mdb/mdb_lex.h>

/*ARGSUSED*/
void
mdb_shell_exec(char *cmd)
{
	yyperror("shell escape facility not available in kmdb\n");
}

/*ARGSUSED*/
void
mdb_shell_pipe(char *cmd)
{
	yyperror("shell pipe facility not available in kmdb\n");
}
