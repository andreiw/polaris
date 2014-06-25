/*
 * Copyright 2002 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"@(#)netgroup.h	1.1	03/01/14 SMI"

#ifdef ORIGINAL_ISC_CODE
#ifndef netgroup_h
#define netgroup_h

int getnetgrent(const char **machinep, const char **userp,
		const char **domainp);

int getnetgrent_r(char **machinep, char **userp, char **domainp,
		  char *buffer, int buflen);

void setnetgrent(const char *netgroup);

void endnetgrent(void);

int innetgr(const char *netgroup, const char *machine,
	    const char *user, const char *domain);

#endif
#endif /* ORIGINAL_ISC_CODE */
