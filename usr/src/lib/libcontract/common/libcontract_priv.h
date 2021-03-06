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

#ifndef	_LIBCONTRACT_PRIV_H
#define	_LIBCONTRACT_PRIV_H

#pragma ident	"@(#)libcontract_priv.h	1.2	05/06/08 SMI"

#include <sys/contract.h>
#include <libcontract.h>
#include <stdio.h>

#ifdef	__cplusplus
extern "C" {
#endif

extern int contract_latest(ctid_t *);
extern int contract_open(ctid_t, const char *, const char *, int);
extern int contract_abandon_id(ctid_t);
extern ctid_t getctid(void);
extern void contract_event_dump(FILE *, ct_evthdl_t, int);

#ifdef	__cplusplus
}
#endif

#endif	/* _LIBCONTRACT_PRIV_H */
