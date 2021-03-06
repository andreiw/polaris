/* $Id: setproctitle.h,v 1.2 2001/02/09 01:55:36 djm Exp $ */

#ifndef	_SETPROCTITLE_H
#define	_SETPROCTITLE_H

#pragma ident	"@(#)setproctitle.h	1.4	03/11/19 SMI"

#ifdef __cplusplus
extern "C" {
#endif

#include "config.h"

#ifndef HAVE_SETPROCTITLE
void setproctitle(const char *fmt, ...);
#endif

#ifdef __cplusplus
}
#endif

#endif /* _SETPROCTITLE_H */
