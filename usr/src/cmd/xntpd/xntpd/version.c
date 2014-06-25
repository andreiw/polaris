/*
 * Copyright 1996, 2003 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"@(#)version.c	1.4	03/08/29 SMI"

/*
 * version file for xntpd
 */

#include <config.h>

#define	PATCH   ""

const char *Version = "xntpd "
    PROTOCOL_VER "-" VERSION "+" VENDOR PATCH " 03/08/29 16:23:05 (1.4)";
