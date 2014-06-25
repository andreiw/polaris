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

#pragma ident	"@(#)tsol_getrhent.c	1.1	06/03/24 SMI"

#include "files_common.h"
#include <string.h>
#include <libtsnet.h>

/*
 *	files/tsol_getrhent.c --
 *           "files" backend for nsswitch "tnrhdb" database
 */
static int
check_addr(nss_XbyY_args_t *args)
{
	tsol_rhstr_t *rhstrp = (tsol_rhstr_t *)args->returnval;

	if ((args->key.hostaddr.type == rhstrp->family) &&
	    (strcmp(args->key.hostaddr.addr, rhstrp->address) == 0))
		return (1);

	return (0);
}

static nss_status_t
getbyaddr(files_backend_ptr_t be, void *a)
{
	nss_XbyY_args_t *argp = a;

	return (_nss_files_XY_all(be, argp, 1,
		argp->key.hostaddr.addr, check_addr));
}

static files_backend_op_t tsol_rh_ops[] = {
	_nss_files_destr,
	_nss_files_endent,
	_nss_files_setent,
	_nss_files_getent_netdb,
	getbyaddr
};

/* ARGSUSED */
nss_backend_t *
_nss_files_tnrhdb_constr(const char *dummy1, const char *dummy2,
    const char *dummy3)
{
	return (_nss_files_constr(tsol_rh_ops,
	    sizeof (tsol_rh_ops) / sizeof (tsol_rh_ops[0]), TNRHDB_PATH,
	    NSS_LINELEN_TSOL_RH, NULL));
}
