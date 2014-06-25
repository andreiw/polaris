/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#pragma ident	"@(#)inttoa.c	1.1	96/11/01 SMI"

/*
 * inttoa - return an asciized signed integer
 */
#include <stdio.h>

#include "lib_strbuf.h"
#include "ntp_stdlib.h"

char *
inttoa(ival)
	long ival;
{
	register char *buf;

	LIB_GETBUF(buf);

	(void) sprintf(buf, "%ld", (long)ival);
	return buf;
}
