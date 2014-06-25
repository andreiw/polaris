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
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)getuserattr.c	1.2	05/06/08 SMI"

#include <sys/types.h>
#include <user_attr.h>
#include <stdlib.h>
#include <string.h>
#include "nis_common.h"

static nss_status_t
getbynam(nis_backend_ptr_t be, void *a)
{
	nss_XbyY_args_t *argp = (nss_XbyY_args_t *)a;

	return (_nss_nis_lookup(be,
		argp, 1, NIS_MAP_USERATTR, argp->key.name, 0));
}

static nis_backend_op_t userattr_ops[] = {
	_nss_nis_destr,
	_nss_nis_endent,
	_nss_nis_setent,
	_nss_nis_getent_netdb,
	getbynam
};

nss_backend_t  *
_nss_nis_user_attr_constr(const char *dummy1,
    const char *dummy2,
    const char *dummy3,
    const char *dummy4,
    const char *dummy5)
{
	return (_nss_nis_constr(userattr_ops,
		sizeof (userattr_ops)/sizeof (userattr_ops[0]),
		NIS_MAP_USERATTR));
}
