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
 * Copyright 1987 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */


#pragma ident	"@(#)pwdnm.h	1.7	05/06/08 SMI"

#ifndef _rpcsvc_pwdnm_h
#define _rpcsvc_pwdnm_h

struct pwdnm {
	char *name;
	char *password;
};
typedef struct pwdnm pwdnm;


#define PWDAUTH_PROG 100036
#define PWDAUTH_VERS 1
#define PWDAUTHSRV 1
#define GRPAUTHSRV 2

bool_t xdr_pwdnm();

#endif /*!_rpcsvc_pwdnm_h*/
