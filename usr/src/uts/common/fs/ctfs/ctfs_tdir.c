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
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"@(#)ctfs_tdir.c	1.6	05/10/30 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/cred.h>
#include <sys/vfs.h>
#include <sys/gfs.h>
#include <sys/vnode.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/sysmacros.h>
#include <fs/fs_subr.h>
#include <sys/contract.h>
#include <sys/contract_impl.h>
#include <sys/ctfs.h>
#include <sys/ctfs_impl.h>
#include <sys/file.h>
#include <sys/pathname.h>

/*
 * Entries in a /system/contract/<type> directory.
 */
static gfs_dirent_t ctfs_tdir_dirents[] = {
	{ "bundle", ctfs_create_bundle },
	{ "pbundle", ctfs_create_pbundle },
	{ "template", ctfs_create_tmplnode },
	{ "latest", ctfs_create_latenode, GFS_CACHE_VNODE },
	{ NULL }
};
#define	CTFS_NSPECIALS	((sizeof ctfs_tdir_dirents / sizeof (gfs_dirent_t)) - 1)

static int ctfs_tdir_do_readdir(vnode_t *, struct dirent64 *, int *, offset_t *,
    offset_t *, void *);
static int ctfs_tdir_do_lookup(vnode_t *, const char *, vnode_t **, ino64_t *);
static ino64_t ctfs_tdir_do_inode(vnode_t *, int);

/*
 * ctfs_create_tdirnode
 */
vnode_t *
ctfs_create_tdirnode(vnode_t *pvp)
{
	return (gfs_dir_create(sizeof (ctfs_tdirnode_t), pvp, ctfs_ops_tdir,
	    ctfs_tdir_dirents, ctfs_tdir_do_inode, CTFS_NAME_MAX,
	    ctfs_tdir_do_readdir, ctfs_tdir_do_lookup));
}

/*
 * ctfs_tdir_getattr - VOP_GETATTR entry point
 */
/* ARGSUSED */
static int
ctfs_tdir_getattr(vnode_t *vp, vattr_t *vap, int flags, cred_t *cr)
{
	vap->va_type = VDIR;
	vap->va_mode = 0555;
	vap->va_nlink = 2 + CTFS_NSPECIALS;
	vap->va_size = vap->va_nlink +
	    contract_type_count(ct_types[gfs_file_index(vp)]);
	vap->va_ctime.tv_sec = vp->v_vfsp->vfs_mtime;
	vap->va_ctime.tv_nsec = 0;
	contract_type_time(ct_types[gfs_file_index(vp)], &vap->va_atime);
	vap->va_mtime = vap->va_atime;
	ctfs_common_getattr(vp, vap);

	return (0);
}

static ino64_t
ctfs_tdir_do_inode(vnode_t *vp, int index)
{
	return (CTFS_INO_TYPE_FILE(gfs_file_index(vp), index));
}

/* ARGSUSED */
static int
ctfs_tdir_do_readdir(vnode_t *vp, struct dirent64 *dp, int *eofp,
    offset_t *offp, offset_t *nextp, void *data)
{
	uint64_t zuniqid;
	ctid_t next;
	ct_type_t *ty = ct_types[gfs_file_index(vp)];

	zuniqid = VTOZONE(vp)->zone_uniqid;
	next = contract_type_lookup(ty, zuniqid, *offp);

	if (next == -1) {
		*eofp = 1;
		return (0);
	}

	dp->d_ino = CTFS_INO_CT_DIR(next);
	numtos(next, dp->d_name);
	*offp = next;
	*nextp = next + 1;

	return (0);
}

static int
ctfs_tdir_do_lookup(vnode_t *vp, const char *nm, vnode_t **vpp, ino64_t *inop)
{
	int i;
	contract_t *ct;

	i = stoi((char **)&nm);
	if (*nm != '\0')
		return (ENOENT);

	ct = contract_type_ptr(ct_types[gfs_file_index(vp)], i,
	    VTOZONE(vp)->zone_uniqid);
	if (ct == NULL)
		return (ENOENT);

	*vpp = ctfs_create_cdirnode(vp, ct);
	*inop = gfs_file_inode(*vpp);
	contract_rele(ct);
	return (0);
}

const fs_operation_def_t ctfs_tops_tdir[] = {
	{ VOPNAME_OPEN,		ctfs_open },
	{ VOPNAME_CLOSE,	ctfs_close },
	{ VOPNAME_IOCTL,	fs_inval },
	{ VOPNAME_GETATTR,	ctfs_tdir_getattr },
	{ VOPNAME_ACCESS,	ctfs_access_dir },
	{ VOPNAME_READDIR,	gfs_vop_readdir },
	{ VOPNAME_LOOKUP,	gfs_vop_lookup },
	{ VOPNAME_SEEK,		fs_seek },
	{ VOPNAME_INACTIVE,	(fs_generic_func_p) gfs_vop_inactive },
	{ NULL, NULL }
};
