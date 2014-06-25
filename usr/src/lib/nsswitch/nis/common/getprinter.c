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

#pragma ident	"@(#)getprinter.c	1.3	05/06/08 SMI"

#pragma weak _nss_nis__printers_constr = _nss_nis_printers_constr

#include <nss_dbdefs.h>
#include "nis_common.h"

static nss_status_t
getbyname(be, a)
	nis_backend_ptr_t	be;
	void			*a;
{
	nss_XbyY_args_t		*argp = (nss_XbyY_args_t *)a;
	nss_status_t		res;

	return (_nss_nis_lookup(be, argp, 0, "printers.conf.byname",
				argp->key.name, 0));
}

static nis_backend_op_t printers_ops[] = {
	_nss_nis_destr,
	_nss_nis_endent,
	_nss_nis_setent,
	_nss_nis_getent_rigid,
	getbyname
};

nss_backend_t *
_nss_nis_printers_constr(dummy1, dummy2, dummy3)
	const char	*dummy1, *dummy2, *dummy3;
{
	return (_nss_nis_constr(printers_ops,
		sizeof (printers_ops) / sizeof (printers_ops[0]),
		"printers.conf.byname"));
}
