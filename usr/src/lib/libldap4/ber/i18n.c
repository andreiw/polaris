/*
 *
 * Portions Copyright 07/17/00 Sun Microsystems, Inc. 
 * All Rights Reserved
 *
 */

#pragma ident	"@(#)i18n.c	1.3	00/07/17 SMI"

#include <nl_types.h>
/* #include <lthread.h> */
#include <pthread.h>
#include <thread.h>

nl_catd slapdcat = 0;
int     notdone = 1;
static pthread_mutex_t log_mutex;
pthread_mutex_t systime_mutex;

void i18n_catopen(char * name)
{
	if ( notdone ) {
		notdone = 0;
		slapdcat = catopen(name, NL_CAT_LOCALE);
	} /* end if */
}

