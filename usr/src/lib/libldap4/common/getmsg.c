/*
 *
 * Copyright 07/17/00 Sun Microsystems, Inc.  
 * All Rights Reserved
 *
 *
 * Comments:   
 *
 */

#pragma ident	"@(#)getmsg.c	1.3	00/07/17 SMI"

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include "lber.h"
#include "ldap.h"
#include "ldap-private.h"

LDAPMessage * ldap_first_message(LDAP *ld, LDAPMessage *res)
{
	return (res == NULLMSG ? NULLMSG : res);
}

LDAPMessage * ldap_next_message(LDAP *ld, LDAPMessage *msg)
{
	if (msg == NULLMSG || msg->lm_chain == NULLMSG) 
		return (NULLMSG);
	return (msg->lm_chain);
}

int ldap_count_messages( LDAP *ld, LDAPMessage *res)
{
	int i;
	
	for ( i =0; res != NULL; res = res->lm_chain)
		i++;

	return (i);
}
