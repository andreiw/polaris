/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#pragma ident	"@(#)fptoms.c	1.1	96/11/01 SMI"

/*
 * fptoms - return an asciized s_fp number in milliseconds
 */
#include "ntp_fp.h"

char *
fptoms(fpv, ndec)
	s_fp fpv;
	int ndec;
{
	u_fp plusfp;
	int neg;

	if (fpv < 0) {
		plusfp = (u_fp)(-fpv);
		neg = 1;
	} else {
		plusfp = (u_fp)fpv;
		neg = 0;
	}

	return dofptoa(plusfp, neg, ndec, 1);
}
