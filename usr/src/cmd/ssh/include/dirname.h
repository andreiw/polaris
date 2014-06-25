/*
 * XXX - Add OpenSSH copyright...
 */

#ifndef	_DIRNAME_H
#define	_DIRNAME_H

#pragma ident	"@(#)dirname.h	1.1	03/09/04 SMI"

#ifdef __cplusplus
extern "C" {
#endif


#ifndef HAVE_DIRNAME

char *dirname(const char *path);

#endif

#ifdef __cplusplus
}
#endif

#endif /* _DIRNAME_H */
