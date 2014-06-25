/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)sunw_irs_nis_acc.c	1.1	00/06/27 SMI"

#include <port_before.h>

#include <irs.h>

#include <port_after.h>

#ifdef	WANT_IRS_NIS
static int	__deliberately_empty;
#else
/*
 * Never called; defined here so that the mapfile doesn't have to change
 * depending on WANT_IRS_NIS.
 */
struct irs_acc *
irs_nis_acc(const char *options) {
	return (0);
}
#endif	/* WANT_IRS_NIS */
