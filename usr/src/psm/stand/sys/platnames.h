/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
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
 * Copyright (c) 1994-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_PLATNAMES_H
#define	_SYS_PLATNAMES_H

#pragma ident	"@(#)platnames.h	1.8	05/06/08 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * External interfaces
 */
extern char *get_mfg_name(void);
extern int find_platform_dir(int (*)(char *), char *, int);
extern int open_platform_file(char *,
    int (*)(char *, void *), void *, char *, char *);
extern void mod_path_uname_m(char *, char *);

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PLATNAMES_H */
