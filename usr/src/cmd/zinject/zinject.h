/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef	_ZINJECT_H
#define	_ZINJECT_H

#pragma ident	"@(#)zinject.h	1.2	06/05/24 SMI"

#include <sys/zfs_ioctl.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef enum {
	TYPE_DATA,		/* plain file contents		*/
	TYPE_DNODE,		/* metadnode contents		*/
	TYPE_MOS,		/* all MOS data			*/
	TYPE_MOSDIR,		/* MOS object directory		*/
	TYPE_METASLAB,		/* metaslab objects		*/
	TYPE_CONFIG,		/* MOS config			*/
	TYPE_BPLIST,		/* block pointer list		*/
	TYPE_SPACEMAP,		/* space map objects		*/
	TYPE_ERRLOG,		/* persistent error log		*/
	TYPE_INVAL
} err_type_t;

#define	MOS_TYPE(t)	\
	((t) >= TYPE_MOS && (t) < TYPE_INVAL)

int translate_record(err_type_t type, const char *object, const char *range,
    int level, zinject_record_t *record, char *poolname, char *dataset);
int translate_raw(const char *raw, zinject_record_t *record);
int translate_device(const char *pool, const char *device,
    zinject_record_t *record);
void usage(void);

extern libzfs_handle_t *g_zfs;

#ifdef	__cplusplus
}
#endif

#endif	/* _ZINJECT_H */
