/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#pragma ident	"@(#)fptoa.c	1.1	96/11/01 SMI"

/*
 * fptoa - return an asciized representation of an s_fp number
 */
#include "ntp_fp.h"
#include "ntp_stdlib.h"

char *
fptoa(fpv, ndec)
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

	return dofptoa(plusfp, neg, ndec, 0);
}
