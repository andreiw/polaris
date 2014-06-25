/* $Id: inet_ntoa.h,v 1.2 2001/02/09 01:55:36 djm Exp $ */

#ifndef	_INET_NTOA_H
#define	_INET_NTOA_H

#pragma ident	"@(#)inet_ntoa.h	1.1	03/09/04 SMI"

#ifdef __cplusplus
extern "C" {
#endif

#include "config.h"

#if defined(BROKEN_INET_NTOA) || !defined(HAVE_INET_NTOA)
char *inet_ntoa(struct in_addr in);
#endif /* defined(BROKEN_INET_NTOA) || !defined(HAVE_INET_NTOA) */

#ifdef __cplusplus
}
#endif

#endif /* _INET_NTOA_H */
