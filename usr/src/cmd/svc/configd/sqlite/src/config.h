/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef	SQLITE_CONFIG_H
#define	SQLITE_CONFIG_H

#pragma ident	"@(#)config.h	1.1	04/11/12 SMI"

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef _LP64
#define	SQLITE_PTR_SZ	8
#else
#define	SQLITE_PTR_SZ	4
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* SQLITE_CONFIG_H */
