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
 * Copyright 2003 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"@(#)cmdgen_unshare.c	1.2	05/06/08 SMI"

#include "cmdgen_include.h"

/*
 * Private data type declaration
 */

/*
 * Public methods
 */
char *
cmdgen_unshare(int fstype, CCIMInstance *inst, CCIMObjectPath *objPath,
	    int *errp) {
	char *cmd = NULL;

	switch (fstype) {
		case CMDGEN_NFS:
			cmd = cmdgen_unshare_nfs(inst, objPath, errp);
			break;
	}
	return (cmd);
} /* cmdgen_unshare */
