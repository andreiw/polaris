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
 * Copyright 2003 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef	_PRTABLE_H
#define	_PRTABLE_H

#pragma ident	"@(#)prtable.h	1.4	05/06/08 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <limits.h>
#include <zone.h>
#include "prstat.h"

#define	PLWP_TBL_SZ	4096	/* hash table of plwp_t structures */
#define	LWP_ACTIVE	1

typedef struct {
	size_t		t_size;
	size_t		t_nent;
	long		*t_list;
} table_t;

typedef struct {
	uid_t		u_id;
	char		u_name[LOGNAME_MAX+1];
} name_t;

typedef struct {
	size_t		n_size;
	size_t		n_nent;
	name_t		*n_list;
} nametbl_t;

typedef struct {
	zoneid_t	z_id;
	char		z_name[ZONENAME_MAX];
} zonename_t;

typedef struct {
	size_t		z_size;
	size_t		z_nent;
	zonename_t	*z_list;
} zonetbl_t;

typedef struct plwp {		/* linked list of pointers to lwps */
	pid_t		l_pid;
	id_t		l_lwpid;
	int		l_active;
	lwp_info_t	*l_lwp;
	struct plwp	*l_next;
} plwp_t;

extern void pwd_getname(int, char *, int);
extern void add_uid(nametbl_t *, char *);
extern int has_uid(nametbl_t *, uid_t);
extern void add_element(table_t *, long);
extern int has_element(table_t *, long);
extern void add_zone(zonetbl_t *, char *);
extern int has_zone(zonetbl_t *, zoneid_t);
extern void convert_zone(zonetbl_t *);
extern int foreach_element(table_t *, void *, void (*)(long, void *));
extern void lwpid_init();
extern void lwpid_add(lwp_info_t *, pid_t, id_t);
extern lwp_info_t *lwpid_get(pid_t, id_t);
extern int lwpid_pidcheck(pid_t);
extern void lwpid_del(pid_t, id_t);
extern void lwpid_set_active(pid_t, id_t);
extern int lwpid_is_active(pid_t, id_t);

#ifdef	__cplusplus
}
#endif

#endif	/* _PRTABLE_H */
