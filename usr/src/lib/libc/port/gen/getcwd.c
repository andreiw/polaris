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

#pragma ident	"@(#)getcwd.c	1.41	05/06/08 SMI"

/*
 * getcwd() returns the pathname of the current working directory.
 * On error, a NULL pointer is returned and errno is set.
 */

#pragma weak getcwd = _getcwd

#include "synonyms.h"
#include <sys/syscall.h>
#include <sys/types.h>
#include <errno.h>
#include <stdlib.h>

char *
getcwd(char *pathname, size_t size)
{
	int alloc = 0;

	if (size == 0) {
		errno = EINVAL;
		return (NULL);
	}

	if (pathname == NULL)  {
		if ((pathname = malloc(size)) == NULL) {
			errno = ENOMEM;
			return (NULL);
		}
		alloc = 1;
	}

	if (syscall(SYS_getcwd, pathname, size) == 0)
		return (pathname);

	if (alloc)
		free(pathname);

	return (NULL);
}
