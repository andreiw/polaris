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

#pragma ident	"@(#)cmdgen_mount.c	1.2	05/06/08 SMI"

#include "cmdgen_include.h"

/*
 * Public methods
 */

/*
 * Method: cmdgen_mount
 *
 * Description: Routes the calls to the mount command generator to the
 * appropriate code for the different mount types.
 *
 * Parameters:
 *	- int fstype - The file system type being mounted.
 *	- CCIMInstance *inst - The instance containing the properties of the
 *	file system to be mounted.
 *	- CCIMObjectPath *objPath - The object containing the properties of the
 *	file system to be mounted.
 *
 * Returns:
 *	- char * - The command generated.
 */
char *
cmdgen_mount(int fstype, CCIMInstance *inst, CCIMObjectPath *objPath,
	int *errp) {

	char *cmd = NULL;

	*errp = 0;
	switch (fstype) {
		case CMDGEN_NFS:
			cmd = cmdgen_mount_nfs(inst, objPath, errp);
			break;
		default:
			*errp = EINVAL;
	}
	return (cmd);
} /* cmdgen_mount */
