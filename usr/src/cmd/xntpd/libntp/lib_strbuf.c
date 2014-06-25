/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#pragma ident	"@(#)lib_strbuf.c	1.2	99/08/11 SMI"

/*
 * lib_strbuf - library string storage
 */

#include "lib_strbuf.h"

/*
 * Storage declarations
 */
char lib_stringbuf[LIB_NUMBUFS][LIB_BUFLENGTH];
int lib_nextbuf;
int lib_inited = 0;

/*
 * initialization routine.  Might be needed if the code is ROMized.
 */
void
init_lib()
{
	lib_nextbuf = 0;
	lib_inited = 1;
}