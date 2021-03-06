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
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef	_SYS_POOL_H
#define	_SYS_POOL_H

#pragma ident	"@(#)pool.h	1.5	05/06/08 SMI"

#include <sys/types.h>
#include <sys/time.h>
#include <sys/nvpair.h>
#include <sys/procset.h>
#include <sys/list.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	POOL_DEFAULT		0		/* default pool's ID */
#define	POOL_MAXID		999999		/* maximum possible pool ID */

/* pools states */
#define	POOL_DISABLED		0		/* pools enabled */
#define	POOL_ENABLED		1		/* pools disabled */

#ifdef	_KERNEL

struct pool_pset;

typedef struct pool {
	poolid_t		pool_id;	/* pool ID */
	uint32_t		pool_ref;	/* # of procs in this pool */
	list_node_t		pool_link;	/* links to next/prev pools */
	nvlist_t		*pool_props;	/* pool properties */
	struct pool_pset	*pool_pset;	/* pool's pset */
} pool_t;

/*
 * Flags for pool_do_bind
 */
#define	POOL_BIND_PSET	0x00000001
#define	POOL_BIND_ALL	POOL_BIND_PSET

/*
 * Result codes for pool_get_class()
 */
#define	POOL_CLASS_UNSET	-1		/* no scheduling class set */
#define	POOL_CLASS_INVAL	-2		/* class is invalid */

extern int	pool_count;	/* current number of pools */
extern pool_t	*pool_default;	/* default pool pointer */
extern int	pool_state;	/* pools state -- enabled/disabled */
extern void	*pool_buf;	/* last state snapshot */
extern size_t	pool_bufsz;	/* size of pool_buf */

/*
 * Lookup routines
 */
extern pool_t	*pool_lookup_pool_by_id(poolid_t);
extern pool_t	*pool_lookup_pool_by_name(char *);

/*
 * Configuration routines
 */
extern void 	pool_init(void);
extern int	pool_status(int);
extern int	pool_create(int, int, id_t *);
extern int	pool_destroy(int, int, id_t);
extern int	pool_transfer(int, id_t, id_t, uint64_t);
extern int	pool_assoc(poolid_t, int, id_t);
extern int	pool_dissoc(poolid_t, int);
extern int	pool_bind(poolid_t, idtype_t, id_t);
extern id_t	pool_get_class(pool_t *);
extern int	pool_do_bind(pool_t *, idtype_t, id_t, int);
extern int	pool_query_binding(idtype_t, id_t, id_t *);
extern int	pool_xtransfer(int, id_t, id_t, uint_t, id_t *);
extern int	pool_pack_conf(void *, size_t, size_t *);
extern int	pool_propput(int, int, id_t, nvpair_t *);
extern int	pool_proprm(int, int, id_t, char *);
extern int	pool_propget(char *, int, int, id_t, nvlist_t **);
extern int	pool_commit(int);

/*
 * Synchronization routines
 */
extern void	pool_lock(void);
extern int	pool_lock_intr(void);
extern int	pool_lock_held(void);
extern void	pool_unlock(void);
extern void	pool_barrier_enter(void);
extern void	pool_barrier_exit(void);

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_POOL_H */
