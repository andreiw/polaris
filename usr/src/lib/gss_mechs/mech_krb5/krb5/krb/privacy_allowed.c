/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)privacy_allowed.c	1.1	99/07/18 SMI"

#include "k5-int.h"
#include "auth_con.h"

krb5_boolean
krb5_privacy_allowed(void)
{
#ifdef	KRB5_NO_PRIVACY
	return (FALSE);
#else
	return (TRUE);
#endif
}
