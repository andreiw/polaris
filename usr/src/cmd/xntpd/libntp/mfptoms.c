/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#pragma ident	"@(#)mfptoms.c	1.1	96/11/01 SMI"

/*
 * mfptoms - Return an asciized signed long fp number in milliseconds
 */
#include "ntp_fp.h"
#include "ntp_stdlib.h"

char *
mfptoms(fpi, fpf, ndec)
	u_long fpi;
	u_long fpf;
	int ndec;
{
	int isneg;

	if (M_ISNEG(fpi, fpf)) {
		isneg = 1;
		M_NEG(fpi, fpf);
	} else
		isneg = 0;

	return dolfptoa(fpi, fpf, isneg, ndec, 1);
}
