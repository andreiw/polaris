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

#pragma ident	"@(#)pc_vnops.c	1.92	05/11/17 SMI"

#include <sys/param.h>
#include <sys/t_lock.h>
#include <sys/systm.h>
#include <sys/sysmacros.h>
#include <sys/user.h>
#include <sys/buf.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <sys/dirent.h>
#include <sys/vnode.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/fcntl.h>
#include <sys/uio.h>
#include <sys/fs/pc_label.h>
#include <sys/fs/pc_fs.h>
#include <sys/fs/pc_dir.h>
#include <sys/fs/pc_node.h>
#include <sys/mman.h>
#include <sys/pathname.h>
#include <sys/vmsystm.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/statvfs.h>
#include <sys/unistd.h>
#include <sys/kmem.h>
#include <sys/conf.h>
#include <sys/flock.h>
#include <sys/policy.h>

#include <vm/seg.h>
#include <vm/page.h>
#include <vm/pvn.h>
#include <vm/seg_map.h>
#include <vm/seg_vn.h>
#include <vm/hat.h>
#include <vm/as.h>
#include <vm/seg_kmem.h>

#include <fs/fs_subr.h>

static int pcfs_open(struct vnode **, int, struct cred *);
static int pcfs_close(struct vnode *, int, int, offset_t, struct cred *);
static int pcfs_read(struct vnode *, struct uio *, int, struct cred *,
	struct caller_context *);
static int pcfs_write(struct vnode *, struct uio *, int, struct cred *,
	struct caller_context *);
static int pcfs_getattr(struct vnode *, struct vattr *, int, struct cred *);
static int pcfs_setattr(struct vnode *, struct vattr *, int, struct cred *,
	caller_context_t *);
static int pcfs_access(struct vnode *, int, int, struct cred *);
static int pcfs_lookup(struct vnode *, char *, struct vnode **,
	struct pathname *, int, struct vnode *, struct cred *);
static int pcfs_create(struct vnode *, char *, struct vattr *,
	enum vcexcl, int mode, struct vnode **, struct cred *, int);
static int pcfs_remove(struct vnode *, char *, struct cred *);
static int pcfs_rename(struct vnode *, char *, struct vnode *, char *,
	struct cred *);
static int pcfs_mkdir(struct vnode *, char *, struct vattr *, struct vnode **,
	struct cred *);
static int pcfs_rmdir(struct vnode *, char *, struct vnode *, struct cred *);
static int pcfs_readdir(struct vnode *, struct uio *, struct cred *, int *);
static int pcfs_fsync(struct vnode *, int, struct cred *);
static void pcfs_inactive(struct vnode *, struct cred *);
static int pcfs_fid(struct vnode *vp, struct fid *fidp);
static int pcfs_space(struct vnode *, int, struct flock64 *, int,
	offset_t, cred_t *, caller_context_t *);
static int pcfs_getpage(struct vnode *, offset_t, size_t, uint_t *, page_t *[],
	size_t, struct seg *, caddr_t, enum seg_rw, struct cred *);
static int pcfs_getapage(struct vnode *, u_offset_t, size_t, uint_t *,
	page_t *[], size_t, struct seg *, caddr_t, enum seg_rw, struct cred *);
static int pcfs_putpage(struct vnode *, offset_t, size_t, int, struct cred *);
static int pcfs_map(struct vnode *, offset_t, struct as *, caddr_t *, size_t,
	uchar_t, uchar_t, uint_t, struct cred *);
static int pcfs_addmap(struct vnode *, offset_t, struct as *, caddr_t,
	size_t, uchar_t, uchar_t, uint_t, struct cred *);
static int pcfs_delmap(struct vnode *, offset_t, struct as *, caddr_t,
	size_t, uint_t, uint_t, uint_t, struct cred *);
static int pcfs_seek(struct vnode *, offset_t, offset_t *);
static int pcfs_pathconf(struct vnode *, int, ulong_t *, struct cred *);

int pcfs_putapage(struct vnode *, page_t *, u_offset_t *, size_t *, int,
	struct cred *);
static int rwpcp(struct pcnode *, struct uio *, enum uio_rw, int);
static int get_long_fn_chunk(struct pcdir_lfn *ep, char *buf, int foldcase);

extern krwlock_t pcnodes_lock;

#define	lround(r)	(((r)+sizeof (long long)-1)&(~(sizeof (long long)-1)))

/*
 * vnode op vectors for files and directories.
 */
struct vnodeops *pcfs_fvnodeops;
struct vnodeops *pcfs_dvnodeops;

const fs_operation_def_t pcfs_fvnodeops_template[] = {
	VOPNAME_OPEN, pcfs_open,
	VOPNAME_CLOSE, pcfs_close,
	VOPNAME_READ, pcfs_read,
	VOPNAME_WRITE, pcfs_write,
	VOPNAME_GETATTR, pcfs_getattr,
	VOPNAME_SETATTR, pcfs_setattr,
	VOPNAME_ACCESS, pcfs_access,
	VOPNAME_FSYNC, pcfs_fsync,
	VOPNAME_INACTIVE, (fs_generic_func_p) pcfs_inactive,
	VOPNAME_FID, pcfs_fid,
	VOPNAME_SEEK, pcfs_seek,
	VOPNAME_SPACE, pcfs_space,
	VOPNAME_GETPAGE, pcfs_getpage,
	VOPNAME_PUTPAGE, pcfs_putpage,
	VOPNAME_MAP, (fs_generic_func_p) pcfs_map,
	VOPNAME_ADDMAP, (fs_generic_func_p) pcfs_addmap,
	VOPNAME_DELMAP, pcfs_delmap,
	VOPNAME_PATHCONF, pcfs_pathconf,
	VOPNAME_VNEVENT, fs_vnevent_support,
	NULL, NULL
};

const fs_operation_def_t pcfs_dvnodeops_template[] = {
	VOPNAME_OPEN, pcfs_open,
	VOPNAME_CLOSE, pcfs_close,
	VOPNAME_GETATTR, pcfs_getattr,
	VOPNAME_SETATTR, pcfs_setattr,
	VOPNAME_ACCESS, pcfs_access,
	VOPNAME_LOOKUP, pcfs_lookup,
	VOPNAME_CREATE, pcfs_create,
	VOPNAME_REMOVE, pcfs_remove,
	VOPNAME_RENAME, pcfs_rename,
	VOPNAME_MKDIR, pcfs_mkdir,
	VOPNAME_RMDIR, pcfs_rmdir,
	VOPNAME_READDIR, pcfs_readdir,
	VOPNAME_FSYNC, pcfs_fsync,
	VOPNAME_INACTIVE, (fs_generic_func_p) pcfs_inactive,
	VOPNAME_FID, pcfs_fid,
	VOPNAME_SEEK, pcfs_seek,
	VOPNAME_PATHCONF, pcfs_pathconf,
	VOPNAME_VNEVENT, fs_vnevent_support,
	NULL, NULL
};


/*ARGSUSED*/
static int
pcfs_open(
	struct vnode **vpp,
	int flag,
	struct cred *cr)
{
	return (0);
}

/*
 * files are sync'ed on close to keep floppy up to date
 */

/*ARGSUSED*/
static int
pcfs_close(
	struct vnode *vp,
	int flag,
	int count,
	offset_t offset,
	struct cred *cr)
{
	return (0);
}

/*ARGSUSED*/
static int
pcfs_read(
	struct vnode *vp,
	struct uio *uiop,
	int ioflag,
	struct cred *cr,
	struct caller_context *ct)
{
	struct pcfs *fsp;
	struct pcnode *pcp;
	int error;

	fsp = VFSTOPCFS(vp->v_vfsp);
	if (error = pc_verify(fsp))
		return (error);
	error = pc_lockfs(fsp, 0, 0);
	if (error)
		return (error);
	if ((pcp = VTOPC(vp)) == NULL) {
		pc_unlockfs(fsp);
		return (EIO);
	}
	error = rwpcp(pcp, uiop, UIO_READ, ioflag);
	if ((fsp->pcfs_vfs->vfs_flag & VFS_RDONLY) == 0) {
		pcp->pc_flags |= PC_ACC;
		pc_mark_acc(pcp);
	}
	pc_unlockfs(fsp);
	if (error) {
		PC_DPRINTF1(1, "pcfs_read: io error = %d\n", error);
	}
	return (error);
}

/*ARGSUSED*/
static int
pcfs_write(
	struct vnode *vp,
	struct uio *uiop,
	int ioflag,
	struct cred *cr,
	struct caller_context *ct)
{
	struct pcfs *fsp;
	struct pcnode *pcp;
	int error;

	fsp = VFSTOPCFS(vp->v_vfsp);
	if (error = pc_verify(fsp))
		return (error);
	error = pc_lockfs(fsp, 0, 0);
	if (error)
		return (error);
	if ((pcp = VTOPC(vp)) == NULL) {
		pc_unlockfs(fsp);
		return (EIO);
	}
	if (ioflag & FAPPEND) {
		/*
		 * in append mode start at end of file.
		 */
		uiop->uio_loffset = pcp->pc_size;
	}
	error = rwpcp(pcp, uiop, UIO_WRITE, ioflag);
	pcp->pc_flags |= PC_MOD;
	pc_mark_mod(pcp);
	if (ioflag & (FSYNC|FDSYNC))
		(void) pc_nodeupdate(pcp);

	pc_unlockfs(fsp);
	if (error) {
		PC_DPRINTF1(1, "pcfs_write: io error = %d\n", error);
	}
	return (error);
}

/*
 * read or write a vnode
 */
static int
rwpcp(
	struct pcnode *pcp,
	struct uio *uio,
	enum uio_rw rw,
	int ioflag)
{
	struct vnode *vp = PCTOV(pcp);
	struct pcfs *fsp;
	daddr_t bn;			/* phys block number */
	int n;
	offset_t off;
	caddr_t base;
	int mapon, pagecreate;
	int newpage;
	int error = 0;
	rlim64_t limit = uio->uio_llimit;
	int oresid = uio->uio_resid;

	PC_DPRINTF4(5, "rwpcp pcp=%p off=%lld resid=%ld size=%u\n", (void *)pcp,
	    uio->uio_loffset, uio->uio_resid, pcp->pc_size);

	ASSERT(rw == UIO_READ || rw == UIO_WRITE);
	ASSERT(vp->v_type == VREG);

	if (uio->uio_loffset >= UINT32_MAX && rw == UIO_READ) {
		return (0);
	}

	if (uio->uio_loffset < 0)
		return (EINVAL);

	if (limit == RLIM64_INFINITY || limit > MAXOFFSET_T)
		limit = MAXOFFSET_T;

	if (uio->uio_loffset >= limit && rw == UIO_WRITE) {
		proc_t *p = ttoproc(curthread);

		mutex_enter(&p->p_lock);
		(void) rctl_action(rctlproc_legacy[RLIMIT_FSIZE], p->p_rctls,
		    p, RCA_UNSAFE_SIGINFO);
		mutex_exit(&p->p_lock);
		return (EFBIG);
	}

	/* the following condition will occur only for write */

	if (uio->uio_loffset >= UINT32_MAX)
		return (EFBIG);

	if (uio->uio_resid == 0)
		return (0);

	if (limit > UINT32_MAX)
		limit = UINT32_MAX;

	fsp = VFSTOPCFS(vp->v_vfsp);
	if (fsp->pcfs_flags & PCFS_IRRECOV)
		return (EIO);

	do {
		/*
		 * Assignments to "n" in this block may appear
		 * to overflow in some cases.  However, after careful
		 * analysis it was determined that all assignments to
		 * "n" serve only to make "n" smaller.  Since "n"
		 * starts out as no larger than MAXBSIZE, "int" is
		 * safe.
		 */
		off = uio->uio_loffset & MAXBMASK;
		mapon = (int)(uio->uio_loffset & MAXBOFFSET);
		n = MIN(MAXBSIZE - mapon, uio->uio_resid);
		if (rw == UIO_READ) {
			offset_t diff;

			diff = pcp->pc_size - uio->uio_loffset;
			if (diff <= 0)
				return (0);
			if (diff < n)
				n = (int)diff;
		}
		/*
		 * Compare limit with the actual offset + n, not the
		 * rounded down offset "off" or we will overflow
		 * the maximum file size after all.
		 */
		if (rw == UIO_WRITE && uio->uio_loffset + n >= limit) {
			if (uio->uio_loffset >= limit) {
				error = EFBIG;
				break;
			}
			n = (int)(limit - uio->uio_loffset);
		}
		base = segmap_getmap(segkmap, vp, (u_offset_t)off);
		pagecreate = 0;
		newpage = 0;
		if (rw == UIO_WRITE) {
			/*
			 * If PAGESIZE < MAXBSIZE, perhaps we ought to deal
			 * with one page at a time, instead of one MAXBSIZE
			 * at a time, so we can fully explore pagecreate
			 * optimization??
			 */
			if (uio->uio_loffset + n > pcp->pc_size) {
				uint_t ncl, lcn;

				ncl = (uint_t)howmany((offset_t)pcp->pc_size,
					fsp->pcfs_clsize);
				if (uio->uio_loffset > pcp->pc_size &&
				    ncl < (uint_t)howmany(uio->uio_loffset,
							fsp->pcfs_clsize)) {
					/*
					 * Allocate and zerofill skipped
					 * clusters. This may not be worth the
					 * effort since a small lseek beyond
					 * eof but still within the cluster
					 * will not be zeroed out.
					 */
					lcn = pc_lblkno(fsp, uio->uio_loffset);
					error = pc_balloc(pcp, (daddr_t)lcn,
					    1, &bn);
					ncl = lcn + 1;
				}
				if (!error &&
				    ncl < (uint_t)howmany(uio->uio_loffset + n,
							fsp->pcfs_clsize))
					/*
					 * allocate clusters w/o zerofill
					 */
					error = pc_balloc(pcp,
					    (daddr_t)pc_lblkno(fsp,
					    uio->uio_loffset + n - 1),
					    0, &bn);

				pcp->pc_flags |= PC_CHG;

				if (error) {
					/* figure out new file size */
					pcp->pc_size = fsp->pcfs_clsize *
					    pc_fileclsize(fsp,
						pcp->pc_scluster);

					if (error == ENOSPC &&
					    (pcp->pc_size - uio->uio_loffset)
						> 0) {
						PC_DPRINTF3(2, "rwpcp ENOSPC "
						    "off=%lld n=%d size=%d\n",
						    uio->uio_loffset,
						    n, pcp->pc_size);
						n = (int)(pcp->pc_size -
							uio->uio_loffset);
					} else {
						PC_DPRINTF1(1,
						    "rwpcp error1=%d\n", error);
						(void) segmap_release(segkmap,
						    base, 0);
						break;
					}
				} else {
					pcp->pc_size =
					    (uint_t)(uio->uio_loffset + n);
				}
				if (mapon == 0) {
					newpage = segmap_pagecreate(segkmap,
						base, (size_t)n, 0);
					pagecreate = 1;
				}
			} else if (n == MAXBSIZE) {
				newpage = segmap_pagecreate(segkmap, base,
						(size_t)n, 0);
				pagecreate = 1;
			}
		}
		error = uiomove(base + mapon, (size_t)n, rw, uio);

		if (pagecreate && uio->uio_loffset <
			roundup(off + mapon + n, PAGESIZE)) {
			offset_t nzero, nmoved;

			nmoved = uio->uio_loffset - (off + mapon);
			nzero = roundup(mapon + n, PAGESIZE) - nmoved;
			(void) kzero(base + mapon + nmoved, (size_t)nzero);
		}

		/*
		 * Unlock the pages which have been allocated by
		 * page_create_va() in segmap_pagecreate().
		 */
		if (newpage)
			segmap_pageunlock(segkmap, base, (size_t)n,
				rw == UIO_WRITE ? S_WRITE : S_READ);

		if (error) {
			PC_DPRINTF1(1, "rwpcp error2=%d\n", error);
			/*
			 * If we failed on a write, we may have already
			 * allocated file blocks as well as pages.  It's hard
			 * to undo the block allocation, but we must be sure
			 * to invalidate any pages that may have been
			 * allocated.
			 */
			if (rw == UIO_WRITE)
				(void) segmap_release(segkmap, base, SM_INVAL);
			else
				(void) segmap_release(segkmap, base, 0);
		} else {
			uint_t flags = 0;

			if (rw == UIO_READ) {
				if (n + mapon == MAXBSIZE ||
				    uio->uio_loffset == pcp->pc_size)
					flags = SM_DONTNEED;
			} else if (ioflag & (FSYNC|FDSYNC)) {
				flags = SM_WRITE;
			} else if (n + mapon == MAXBSIZE) {
				flags = SM_WRITE|SM_ASYNC|SM_DONTNEED;
			}
			error = segmap_release(segkmap, base, flags);
		}

	} while (error == 0 && uio->uio_resid > 0 && n != 0);

	if (oresid != uio->uio_resid)
		error = 0;
	return (error);
}

/*ARGSUSED*/
static int
pcfs_getattr(
	struct vnode *vp,
	struct vattr *vap,
	int flags,
	struct cred *cr)
{
	struct pcnode *pcp;
	struct pcfs *fsp;
	int error;
	char attr;
	struct pctime atime;

	PC_DPRINTF1(8, "pcfs_getattr: vp=%p\n", (void *)vp);

	fsp = VFSTOPCFS(vp->v_vfsp);
	error = pc_lockfs(fsp, 0, 0);
	if (error)
		return (error);
	if ((pcp = VTOPC(vp)) == NULL) {
		pc_unlockfs(fsp);
		return (EIO);
	}
	/*
	 * Copy from pcnode.
	 */
	vap->va_type = vp->v_type;
	attr = pcp->pc_entry.pcd_attr;
	if (PCA_IS_HIDDEN(fsp, attr))
		vap->va_mode = 0;
	else if (attr & PCA_LABEL)
		vap->va_mode = 0444;
	else if (attr & PCA_RDONLY)
		vap->va_mode = 0555;
	else if (fsp->pcfs_flags & PCFS_BOOTPART) {
		vap->va_mode = 0755;
	} else {
		vap->va_mode = 0777;
	}

	if (attr & PCA_DIR)
		vap->va_mode |= S_IFDIR;
	else
		vap->va_mode |= S_IFREG;
	if (fsp->pcfs_flags & PCFS_BOOTPART) {
		vap->va_uid = 0;
		vap->va_gid = 0;
	} else {
		vap->va_uid = crgetuid(cr);
		vap->va_gid = crgetgid(cr);
	}
	vap->va_fsid = vp->v_vfsp->vfs_dev;
	vap->va_nodeid = (ino64_t)pc_makenodeid(pcp->pc_eblkno,
	    pcp->pc_eoffset, pcp->pc_entry.pcd_attr,
	    pc_getstartcluster(fsp, &pcp->pc_entry), fsp->pcfs_entps);
	vap->va_nlink = 1;
	vap->va_size = (u_offset_t)pcp->pc_size;
	pc_pcttotv(&pcp->pc_entry.pcd_mtime, &vap->va_mtime);
	vap->va_ctime = vap->va_mtime;
	atime.pct_time = 0;
	atime.pct_date = pcp->pc_entry.pcd_ladate;
	pc_pcttotv(&atime, &vap->va_atime);
	vap->va_rdev = 0;
	vap->va_nblocks = (fsblkcnt64_t)howmany((offset_t)pcp->pc_size,
				DEV_BSIZE);
	vap->va_blksize = fsp->pcfs_clsize;
	pc_unlockfs(fsp);
	return (0);
}


/*ARGSUSED*/
static int
pcfs_setattr(
	struct vnode *vp,
	struct vattr *vap,
	int flags,
	struct cred *cr,
	caller_context_t *ct)
{
	struct pcnode *pcp;
	mode_t mask = vap->va_mask;
	int error;
	struct pcfs *fsp;
	timestruc_t now;

	PC_DPRINTF2(6, "pcfs_setattr: vp=%p mask=%x\n", (void *)vp, (int)mask);
	/*
	 * cannot set these attributes
	 */
	if (mask & (AT_NOSET | AT_UID | AT_GID)) {
		return (EINVAL);
	}
	/*
	 * pcfs_settar is now allowed on directories to avoid silly warnings
	 * from 'tar' when it tries to set times on a directory, and console
	 * printf's on the NFS server when it gets EINVAL back on such a
	 * request. One possible problem with that since a directory entry
	 * identifies a file, '.' and all the '..' entries in subdirectories
	 * may get out of sync when the directory is updated since they're
	 * treated like separate files. We could fix that by looking for
	 * '.' and giving it the same attributes, and then looking for
	 * all the subdirectories and updating '..', but that's pretty
	 * expensive for something that doesn't seem likely to matter.
	 */
	/* can't do some ops on directories anyway */
	if ((vp->v_type == VDIR) &&
	    (mask & AT_SIZE)) {
		return (EINVAL);
	}

	fsp = VFSTOPCFS(vp->v_vfsp);
	error = pc_lockfs(fsp, 0, 0);
	if (error)
		return (error);
	if ((pcp = VTOPC(vp)) == NULL) {
		pc_unlockfs(fsp);
		return (EIO);
	}

	if (fsp->pcfs_flags & PCFS_BOOTPART) {
		if (secpolicy_pcfs_modify_bootpartition(cr) != 0) {
			pc_unlockfs(fsp);
			return (EACCES);
		}
	}

	/*
	 * Change file access modes.
	 * If nobody has write permission, file is marked readonly.
	 * Otherwise file is writable by anyone.
	 */
	if ((mask & AT_MODE) && (vap->va_mode != (mode_t)-1)) {
		if ((vap->va_mode & 0222) == 0)
			pcp->pc_entry.pcd_attr |= PCA_RDONLY;
		else
			pcp->pc_entry.pcd_attr &= ~PCA_RDONLY;
		pcp->pc_flags |= PC_CHG;
	}
	/*
	 * Truncate file. Must have write permission.
	 */
	if ((mask & AT_SIZE) && (vap->va_size != (u_offset_t)-1)) {
		if (pcp->pc_entry.pcd_attr & PCA_RDONLY) {
			error = EACCES;
			goto out;
		}
		if (vap->va_size > UINT32_MAX) {
			error = EFBIG;
			goto out;
		}
		error = pc_truncate(pcp, (uint_t)vap->va_size);
		if (error)
			goto out;
	}
	/*
	 * Change file modified times.
	 */
	if ((mask & (AT_MTIME | AT_CTIME)) && (vap->va_mtime.tv_sec != -1)) {
		/*
		 * If SysV-compatible option to set access and
		 * modified times if privileged, owner, or write access,
		 * use current time rather than va_mtime.
		 *
		 * XXX - va_mtime.tv_sec == -1 flags this.
		 */
		if (vap->va_mtime.tv_sec == -1) {
			gethrestime(&now);
			pc_tvtopct(&now, &pcp->pc_entry.pcd_mtime);
		} else {
			pc_tvtopct(&vap->va_mtime, &pcp->pc_entry.pcd_mtime);
		}
		pcp->pc_flags |= PC_CHG;
	}
	/*
	 * Change file access times.
	 */
	if ((mask & (AT_ATIME)) && (vap->va_atime.tv_sec != -1)) {
		/*
		 * If SysV-compatible option to set access and
		 * modified times if privileged, owner, or write access,
		 * use current time rather than va_mtime.
		 *
		 * XXX - va_atime.tv_sec == -1 flags this.
		 */
		struct pctime	atime;

		if (vap->va_atime.tv_sec == -1) {
			gethrestime(&now);
			pc_tvtopct(&now, &atime);
		} else {
			pc_tvtopct(&vap->va_atime, &atime);
		}
		pcp->pc_entry.pcd_ladate = atime.pct_date;
		pcp->pc_flags |= PC_CHG;
	}
out:
	pc_unlockfs(fsp);
	return (error);
}


/*ARGSUSED*/
static int
pcfs_access(
	struct vnode *vp,
	int mode,
	int flags,
	struct cred *cr)
{
	struct pcnode *pcp;
	struct pcfs *fsp;


	fsp = VFSTOPCFS(vp->v_vfsp);

	if ((pcp = VTOPC(vp)) == NULL)
		return (EIO);
	if ((mode & VWRITE) && (pcp->pc_entry.pcd_attr & PCA_RDONLY))
		return (EACCES);

	/*
	 * If this is a boot partition, privileged users have full access while
	 * others have read-only access.
	 */
	if (fsp->pcfs_flags & PCFS_BOOTPART) {
		if ((mode & VWRITE) &&
		    secpolicy_pcfs_modify_bootpartition(cr) != 0)
			return (EACCES);
	}
	return (0);
}


/*ARGSUSED*/
static int
pcfs_fsync(
	struct vnode *vp,
	int syncflag,
	struct cred *cr)
{
	struct pcfs *fsp;
	struct pcnode *pcp;
	int error;

	fsp = VFSTOPCFS(vp->v_vfsp);
	if (error = pc_verify(fsp))
		return (error);
	error = pc_lockfs(fsp, 0, 0);
	if (error)
		return (error);
	if ((pcp = VTOPC(vp)) == NULL) {
		pc_unlockfs(fsp);
		return (EIO);
	}
	rw_enter(&pcnodes_lock, RW_WRITER);
	error = pc_nodesync(pcp);
	rw_exit(&pcnodes_lock);
	pc_unlockfs(fsp);
	return (error);
}


/*ARGSUSED*/
static void
pcfs_inactive(
	struct vnode *vp,
	struct cred *cr)
{
	struct pcnode *pcp;
	struct pcfs *fsp;
	int error;

	fsp = VFSTOPCFS(vp->v_vfsp);
	error = pc_lockfs(fsp, 0, 1);

	mutex_enter(&vp->v_lock);
	ASSERT(vp->v_count >= 1);
	if (vp->v_count > 1) {
		vp->v_count--;  /* release our hold from vn_rele */
		mutex_exit(&vp->v_lock);
		pc_unlockfs(fsp);
		return;
	}
	mutex_exit(&vp->v_lock);

	/*
	 * Check again to confirm that no intervening I/O error
	 * with a subsequent pc_diskchanged() call has released
	 * the pcnode.  If it has then release the vnode as above.
	 */
	if ((pcp = VTOPC(vp)) == NULL)
		vn_free(vp);
	else
		pc_rele(pcp);

	if (!error)
		pc_unlockfs(fsp);
}

/*ARGSUSED*/
static int
pcfs_lookup(
	struct vnode *dvp,
	char *nm,
	struct vnode **vpp,
	struct pathname *pnp,
	int flags,
	struct vnode *rdir,
	struct cred *cr)
{
	struct pcfs *fsp;
	struct pcnode *pcp;
	int error;

	/*
	 * verify that the dvp is still valid on the disk
	 */
	fsp = VFSTOPCFS(dvp->v_vfsp);
	if (error = pc_verify(fsp))
		return (error);
	error = pc_lockfs(fsp, 0, 0);
	if (error)
		return (error);
	if (VTOPC(dvp) == NULL) {
		pc_unlockfs(fsp);
		return (EIO);
	}
	/*
	 * Null component name is a synonym for directory being searched.
	 */
	if (*nm == '\0') {
		VN_HOLD(dvp);
		*vpp = dvp;
		pc_unlockfs(fsp);
		return (0);
	}

	error = pc_dirlook(VTOPC(dvp), nm, &pcp);
	if (!error) {
		*vpp = PCTOV(pcp);
		pcp->pc_flags |= PC_EXTERNAL;
	}
	pc_unlockfs(fsp);
	return (error);
}


/*ARGSUSED*/
static int
pcfs_create(
	struct vnode *dvp,
	char *nm,
	struct vattr *vap,
	enum vcexcl exclusive,
	int mode,
	struct vnode **vpp,
	struct cred *cr,
	int flag)
{
	int error;
	struct pcnode *pcp;
	struct vnode *vp;
	struct pcfs *fsp;

	/*
	 * can't create directories. use pcfs_mkdir.
	 * can't create anything other than files.
	 */
	if (vap->va_type == VDIR)
		return (EISDIR);
	else if (vap->va_type != VREG)
		return (EINVAL);

	pcp = NULL;
	fsp = VFSTOPCFS(dvp->v_vfsp);
	error = pc_lockfs(fsp, 0, 0);
	if (error)
		return (error);
	if (VTOPC(dvp) == NULL) {
		pc_unlockfs(fsp);
		return (EIO);
	}

	if (fsp->pcfs_flags & PCFS_BOOTPART) {
		if (secpolicy_pcfs_modify_bootpartition(cr) != 0) {
			pc_unlockfs(fsp);
			return (EACCES);
		}
	}

	if (*nm == '\0') {
		/*
		 * Null component name refers to the directory itself.
		 */
		VN_HOLD(dvp);
		pcp = VTOPC(dvp);
		error = EEXIST;
	} else {
		error = pc_direnter(VTOPC(dvp), nm, vap, &pcp);
	}
	/*
	 * if file exists and this is a nonexclusive create,
	 * check for access permissions
	 */
	if (error == EEXIST) {
		vp = PCTOV(pcp);
		if (exclusive == NONEXCL) {
			if (vp->v_type == VDIR) {
				error = EISDIR;
			} else if (mode) {
				error = pcfs_access(PCTOV(pcp), mode, 0,
					cr);
			} else {
				error = 0;
			}
		}
		if (error) {
			VN_RELE(PCTOV(pcp));
		} else if ((vp->v_type == VREG) && (vap->va_mask & AT_SIZE) &&
			(vap->va_size == 0)) {
			error = pc_truncate(pcp, 0L);
			if (error)
				VN_RELE(PCTOV(pcp));
		}
	}
	if (error) {
		pc_unlockfs(fsp);
		return (error);
	}
	*vpp = PCTOV(pcp);
	pcp->pc_flags |= PC_EXTERNAL;
	pc_unlockfs(fsp);
	return (error);
}

/*ARGSUSED*/
static int
pcfs_remove(
	struct vnode *vp,
	char *nm,
	struct cred *cr)
{
	struct pcfs *fsp;
	struct pcnode *pcp;
	int error;

	fsp = VFSTOPCFS(vp->v_vfsp);
	if (error = pc_verify(fsp))
		return (error);
	error = pc_lockfs(fsp, 0, 0);
	if (error)
		return (error);
	if ((pcp = VTOPC(vp)) == NULL) {
		pc_unlockfs(fsp);
		return (EIO);
	}
	if (fsp->pcfs_flags & PCFS_BOOTPART) {
		if (secpolicy_pcfs_modify_bootpartition(cr) != 0) {
			pc_unlockfs(fsp);
			return (EACCES);
		}
	}
	error = pc_dirremove(pcp, nm, (struct vnode *)0, VREG);
	pc_unlockfs(fsp);
	return (error);
}

/*
 * Rename a file or directory
 * This rename is restricted to only rename files within a directory.
 * XX should make rename more general
 */
/*ARGSUSED*/
static int
pcfs_rename(
	struct vnode *sdvp,		/* old (source) parent vnode */
	char *snm,			/* old (source) entry name */
	struct vnode *tdvp,		/* new (target) parent vnode */
	char *tnm,			/* new (target) entry name */
	struct cred *cr)
{
	struct pcfs *fsp;
	struct pcnode *dp;	/* parent pcnode */
	struct pcnode *tdp;
	int error;

	fsp = VFSTOPCFS(sdvp->v_vfsp);
	if (error = pc_verify(fsp))
		return (error);
	if (((dp = VTOPC(sdvp)) == NULL) || ((tdp = VTOPC(tdvp)) == NULL)) {
		return (EIO);
	}

	/*
	 * make sure we can muck with this directory.
	 */
	error = pcfs_access(sdvp, VWRITE, 0, cr);
	if (error) {
		return (error);
	}
	error = pc_lockfs(fsp, 0, 0);
	if (error)
		return (error);
	if ((VTOPC(sdvp) == NULL) || (VTOPC(tdvp) == NULL)) {
		pc_unlockfs(fsp);
		return (EIO);
	}
	error = pc_rename(dp, tdp, snm, tnm);
	pc_unlockfs(fsp);
	return (error);
}

/*ARGSUSED*/
static int
pcfs_mkdir(
	struct vnode *dvp,
	char *nm,
	struct vattr *vap,
	struct vnode **vpp,
	struct cred *cr)
{
	struct pcfs *fsp;
	struct pcnode *pcp;
	int error;

	fsp = VFSTOPCFS(dvp->v_vfsp);
	if (error = pc_verify(fsp))
		return (error);
	error = pc_lockfs(fsp, 0, 0);
	if (error)
		return (error);
	if (VTOPC(dvp) == NULL) {
		pc_unlockfs(fsp);
		return (EIO);
	}

	if (fsp->pcfs_flags & PCFS_BOOTPART) {
		if (secpolicy_pcfs_modify_bootpartition(cr) != 0) {
			pc_unlockfs(fsp);
			return (EACCES);
		}
	}

	error = pc_direnter(VTOPC(dvp), nm, vap, &pcp);

	if (!error) {
		pcp -> pc_flags |= PC_EXTERNAL;
		*vpp = PCTOV(pcp);
	} else if (error == EEXIST) {
		VN_RELE(PCTOV(pcp));
	}
	pc_unlockfs(fsp);
	return (error);
}

/*ARGSUSED*/
static int
pcfs_rmdir(
	struct vnode *dvp,
	char *nm,
	struct vnode *cdir,
	struct cred *cr)
{
	struct pcfs *fsp;
	struct pcnode *pcp;
	int error;

	fsp = VFSTOPCFS(dvp -> v_vfsp);
	if (error = pc_verify(fsp))
		return (error);
	if (error = pc_lockfs(fsp, 0, 0))
		return (error);

	if ((pcp = VTOPC(dvp)) == NULL) {
		pc_unlockfs(fsp);
		return (EIO);
	}

	if (fsp->pcfs_flags & PCFS_BOOTPART) {
		if (secpolicy_pcfs_modify_bootpartition(cr) != 0) {
			pc_unlockfs(fsp);
			return (EACCES);
		}
	}

	error = pc_dirremove(pcp, nm, cdir, VDIR);
	pc_unlockfs(fsp);
	return (error);
}

/*
 * read entries in a directory.
 * we must convert pc format to unix format
 */

/*ARGSUSED*/
static int
pcfs_readdir(
	struct vnode *dvp,
	struct uio *uiop,
	struct cred *cr,
	int *eofp)
{
	struct pcnode *pcp;
	struct pcfs *fsp;
	struct pcdir *ep;
	struct buf *bp = NULL;
	offset_t offset;
	int boff;
	struct pc_dirent lbp;
	struct pc_dirent *ld = &lbp;
	int error;

	if ((uiop->uio_iovcnt != 1) ||
	    (uiop->uio_loffset % sizeof (struct pcdir)) != 0) {
		return (EINVAL);
	}
	fsp = VFSTOPCFS(dvp->v_vfsp);
	/*
	 * verify that the dp is still valid on the disk
	 */
	if (error = pc_verify(fsp)) {
		return (error);
	}
	error = pc_lockfs(fsp, 0, 0);
	if (error)
		return (error);
	if ((pcp = VTOPC(dvp)) == NULL) {
		pc_unlockfs(fsp);
		return (EIO);
	}

	bzero(ld, sizeof (*ld));

	if (eofp != NULL)
		*eofp = 0;
	offset = uiop->uio_loffset;

	if (dvp->v_flag & VROOT) {
		/*
		 * kludge up entries for "." and ".." in the root.
		 */
		if (offset == 0) {
			(void) strcpy(ld->d_name, ".");
			ld->d_reclen = DIRENT64_RECLEN(1);
			ld->d_off = (off64_t)sizeof (struct pcdir);
			ld->d_ino = (ino64_t)UINT_MAX;
			if (ld->d_reclen > uiop->uio_resid) {
				pc_unlockfs(fsp);
				return (ENOSPC);
			}
			(void) uiomove(ld, ld->d_reclen, UIO_READ, uiop);
			uiop->uio_loffset = ld->d_off;
			offset = uiop->uio_loffset;
		}
		if (offset == sizeof (struct pcdir)) {
			(void) strcpy(ld->d_name, "..");
			ld->d_reclen = DIRENT64_RECLEN(2);
			if (ld->d_reclen > uiop->uio_resid) {
				pc_unlockfs(fsp);
				return (ENOSPC);
			}
			ld->d_off = (off64_t)(uiop->uio_loffset +
			    sizeof (struct pcdir));
			ld->d_ino = (ino64_t)UINT_MAX;
			(void) uiomove(ld, ld->d_reclen, UIO_READ, uiop);
			uiop->uio_loffset = ld->d_off;
			offset = uiop->uio_loffset;
		}
		offset -= 2 * sizeof (struct pcdir);
		/* offset now has the real offset value into directory file */
	}

	for (;;) {
		boff = pc_blkoff(fsp, offset);
		if (boff == 0 || bp == NULL || boff >= bp->b_bcount) {
			if (bp != NULL) {
				brelse(bp);
				bp = NULL;
			}
			error = pc_blkatoff(pcp, offset, &bp, &ep);
			if (error) {
				if (error == ENOENT) {
					error = 0;
					if (eofp)
						*eofp = 1;
				}
				break;
			}
		}
		if (ep->pcd_filename[0] == PCD_UNUSED) {
			if (eofp)
				*eofp = 1;
			break;
		}
		/*
		 * Don't display label because it may contain funny characters.
		 */
		if (ep->pcd_filename[0] == PCD_ERASED) {
			uiop->uio_loffset += sizeof (struct pcdir);
			offset += sizeof (struct pcdir);
			ep++;
			continue;
		}
		if (PCDL_IS_LFN(ep)) {
			if (pc_read_long_fn(dvp, uiop, ld, &ep, &offset, &bp) !=
			    0)
				break;
			continue;
		}

		if (pc_read_short_fn(dvp, uiop, ld, &ep, &offset, &bp) != 0)
			break;
	}
	if (bp)
		brelse(bp);
	pc_unlockfs(fsp);
	return (error);
}


/*
 * Called from pvn_getpages or pcfs_getpage to get a particular page.
 * When we are called the pcfs is already locked.
 */
/*ARGSUSED*/
static int
pcfs_getapage(
	struct vnode *vp,
	u_offset_t off,
	size_t len,
	uint_t *protp,
	page_t *pl[],		/* NULL if async IO is requested */
	size_t plsz,
	struct seg *seg,
	caddr_t addr,
	enum seg_rw rw,
	struct cred *cr)
{
	struct pcnode *pcp;
	struct pcfs *fsp = VFSTOPCFS(vp->v_vfsp);
	struct vnode *devvp;
	page_t *pp;
	page_t *pagefound;
	int err;

	PC_DPRINTF3(5, "pcfs_getapage: vp=%p off=%lld len=%lu\n",
	    (void *)vp, off, len);

	if ((pcp = VTOPC(vp)) == NULL)
		return (EIO);
	devvp = fsp->pcfs_devvp;

	/* pcfs doesn't do readaheads */
	if (pl == NULL)
		return (0);

	pl[0] = NULL;
	err = 0;
	/*
	 * If the accessed time on the pcnode has not already been
	 * set elsewhere (e.g. for read/setattr) we set the time now.
	 * This gives us approximate modified times for mmap'ed files
	 * which are accessed via loads in the user address space.
	 */
	if ((pcp->pc_flags & PC_ACC) == 0 &&
	    ((fsp->pcfs_vfs->vfs_flag & VFS_RDONLY) == 0)) {
		pcp->pc_flags |= PC_ACC;
		pc_mark_acc(pcp);
	}
reread:
	if ((pagefound = page_exists(vp, off)) == NULL) {
		/*
		 * Need to really do disk IO to get the page(s).
		 */
		struct buf *bp;
		daddr_t lbn, bn;
		u_offset_t io_off;
		size_t io_len;
		u_offset_t lbnoff, xferoffset;
		u_offset_t pgoff;
		uint_t	xfersize;
		int err1;

		lbn = pc_lblkno(fsp, off);
		lbnoff = off & ~(fsp->pcfs_clsize - 1);
		xferoffset = off & ~(fsp->pcfs_secsize - 1);

		pp = pvn_read_kluster(vp, off, seg, addr, &io_off, &io_len,
		    off, (size_t)MIN(pc_blksize(fsp, pcp, off), PAGESIZE), 0);
		if (pp == NULL)
			/*
			 * XXX - If pcfs is made MT-hot, this should go
			 * back to reread.
			 */
			panic("pcfs_getapage pvn_read_kluster");

		for (pgoff = 0; pgoff < PAGESIZE && xferoffset < pcp->pc_size;
		    pgoff += xfersize,
		    lbn +=  howmany(xfersize, fsp->pcfs_clsize),
		    lbnoff += xfersize, xferoffset += xfersize) {
			/*
			 * read as many contiguous blocks as possible to
			 * fill this page
			 */
			xfersize = PAGESIZE - pgoff;
			err1 = pc_bmap(pcp, lbn, &bn, &xfersize);
			if (err1) {
				PC_DPRINTF1(1, "pc_getapage err=%d", err1);
				err = err1;
				goto out;
			}
			bp = pageio_setup(pp, xfersize, devvp, B_READ);
			bp->b_edev = devvp->v_rdev;
			bp->b_dev = cmpdev(devvp->v_rdev);
			bp->b_blkno = bn +
			    /* add a sector offset within the cluster */
			    /* when the clustersize > PAGESIZE */
			    (xferoffset - lbnoff) / fsp->pcfs_secsize;
			bp->b_un.b_addr = (caddr_t)(uintptr_t)pgoff;
			bp->b_file = vp;
			bp->b_offset = (offset_t)(off + pgoff);

			(void) bdev_strategy(bp);

			lwp_stat_update(LWP_STAT_INBLK, 1);

			if (err == 0)
				err = biowait(bp);
			else
				(void) biowait(bp);
			pageio_done(bp);
			if (err)
				goto out;
		}
		if (pgoff < PAGESIZE) {
			pagezero(pp->p_prev, pgoff, PAGESIZE - pgoff);
		}
		pvn_plist_init(pp, pl, plsz, off, io_len, rw);
	}
out:
	if (err) {
		if (pp != NULL)
			pvn_read_done(pp, B_ERROR);
		return (err);
	}

	if (pagefound) {
		/*
		 * Page exists in the cache, acquire the "shared"
		 * lock.  If this fails, go back to reread.
		 */
		if ((pp = page_lookup(vp, off, SE_SHARED)) == NULL) {
			goto reread;
		}
		pl[0] = pp;
		pl[1] = NULL;
	}
	return (err);
}

/*
 * Return all the pages from [off..off+len] in given file
 */
static int
pcfs_getpage(
	struct vnode *vp,
	offset_t off,
	size_t len,
	uint_t *protp,
	page_t *pl[],
	size_t plsz,
	struct seg *seg,
	caddr_t addr,
	enum seg_rw rw,
	struct cred *cr)
{
	struct pcfs *fsp = VFSTOPCFS(vp->v_vfsp);
	int err;

	PC_DPRINTF0(6, "pcfs_getpage\n");
	if (err = pc_verify(fsp))
		return (err);
	if (vp->v_flag & VNOMAP)
		return (ENOSYS);
	ASSERT(off <= UINT32_MAX);
	err = pc_lockfs(fsp, 0, 0);
	if (err)
		return (err);
	if (protp != NULL)
		*protp = PROT_ALL;

	ASSERT((off & PAGEOFFSET) == 0);
	if (len <= PAGESIZE) {
		err = pcfs_getapage(vp, off, len, protp, pl,
		    plsz, seg, addr, rw, cr);
	} else {
		err = pvn_getpages(pcfs_getapage, vp, off,
		    len, protp, pl, plsz, seg, addr, rw, cr);
	}
	pc_unlockfs(fsp);
	return (err);
}


/*
 * Flags are composed of {B_INVAL, B_FREE, B_DONTNEED, B_FORCE}
 * If len == 0, do from off to EOF.
 *
 * The normal cases should be len == 0 & off == 0 (entire vp list),
 * len == MAXBSIZE (from segmap_release actions), and len == PAGESIZE
 * (from pageout).
 *
 */
/*ARGSUSED*/
static int
pcfs_putpage(
	struct vnode *vp,
	offset_t off,
	size_t len,
	int flags,
	struct cred *cr)
{
	struct pcnode *pcp;
	page_t *pp;
	struct pcfs *fsp;
	u_offset_t io_off;
	size_t io_len;
	offset_t eoff;
	int err;

	PC_DPRINTF1(6, "pcfs_putpage vp=0x%p\n", (void *)vp);
	if (vp->v_flag & VNOMAP)
		return (ENOSYS);

	fsp = VFSTOPCFS(vp->v_vfsp);

	if (err = pc_verify(fsp))
		return (err);
	if ((pcp = VTOPC(vp)) == NULL) {
		PC_DPRINTF1(3, "pcfs_putpage NULL vp=0x%p\n", (void *)vp);
		return (EIO);
	}

	if (curproc == proc_pageout) {
		/*
		 * XXX - This is a quick hack to avoid blocking
		 * pageout. Also to avoid pcfs_getapage deadlocking
		 * with putpage when memory is running out,
		 * since we only have one global lock and we don't
		 * support async putpage.
		 * It should be fixed someday.
		 *
		 * Interestingly, this used to be a test of NOMEMWAIT().
		 * We only ever got here once pcfs started supporting
		 * NFS sharing, and then only because the NFS server
		 * threads seem to do writes in sched's process context.
		 * Since everyone else seems to just care about pageout,
		 * the test was changed to look for pageout directly.
		 */
		return (ENOMEM);
	}

	ASSERT(off <= UINT32_MAX);

	flags &= ~B_ASYNC;	/* XXX should fix this later */

	err = pc_lockfs(fsp, 0, 0);
	if (err)
		return (err);
	if (!vn_has_cached_data(vp) || off >= pcp->pc_size) {
		pc_unlockfs(fsp);
		return (0);
	}

	if (len == 0) {
		/*
		 * Search the entire vp list for pages >= off
		 */
		err = pvn_vplist_dirty(vp, off,
		    pcfs_putapage, flags, cr);
	} else {
		eoff = off + len;

		for (io_off = off; io_off < eoff &&
		    io_off < pcp->pc_size; io_off += io_len) {
			/*
			 * If we are not invalidating, synchronously
			 * freeing or writing pages use the routine
			 * page_lookup_nowait() to prevent reclaiming
			 * them from the free list.
			 */
			if ((flags & B_INVAL) || ((flags & B_ASYNC) == 0)) {
				pp = page_lookup(vp, io_off,
					(flags & (B_INVAL | B_FREE)) ?
					    SE_EXCL : SE_SHARED);
			} else {
				pp = page_lookup_nowait(vp, io_off,
					(flags & B_FREE) ? SE_EXCL : SE_SHARED);
			}

			if (pp == NULL || pvn_getdirty(pp, flags) == 0)
				io_len = PAGESIZE;
			else {
				err = pcfs_putapage(vp, pp, &io_off, &io_len,
					flags, cr);
				if (err != 0)
					break;
				/*
				 * "io_off" and "io_len" are returned as
				 * the range of pages we actually wrote.
				 * This allows us to skip ahead more quickly
				 * since several pages may've been dealt
				 * with by this iteration of the loop.
				 */
			}
		}
	}
	if (err == 0 && (flags & B_INVAL) &&
	    off == 0 && len == 0 && vn_has_cached_data(vp)) {
		/*
		 * If doing "invalidation", make sure that
		 * all pages on the vnode list are actually
		 * gone.
		 */
		cmn_err(CE_PANIC,
			"pcfs_putpage: B_INVAL, pages not gone");
	} else if (err) {
		PC_DPRINTF1(1, "pcfs_putpage err=%d\n", err);
	}
	pc_unlockfs(fsp);
	return (err);
}

/*
 * Write out a single page, possibly klustering adjacent dirty pages.
 */
/*ARGSUSED*/
int
pcfs_putapage(
	struct vnode *vp,
	page_t *pp,
	u_offset_t *offp,
	size_t *lenp,
	int flags,
	struct cred *cr)
{
	struct pcnode *pcp;
	struct pcfs *fsp;
	struct vnode *devvp;
	size_t io_len;
	daddr_t bn;
	u_offset_t lbn, lbnoff, xferoffset;
	uint_t pgoff, xfersize;
	int err = 0;
	u_offset_t io_off;

	pcp = VTOPC(vp);
	fsp = VFSTOPCFS(vp->v_vfsp);
	devvp = fsp->pcfs_devvp;

	/*
	 * If the modified time on the inode has not already been
	 * set elsewhere (e.g. for write/setattr) and this is not
	 * a call from msync (B_FORCE) we set the time now.
	 * This gives us approximate modified times for mmap'ed files
	 * which are modified via stores in the user address space.
	 */
	if ((pcp->pc_flags & PC_MOD) == 0 || (flags & B_FORCE)) {
		pcp->pc_flags |= PC_MOD;
		pc_mark_mod(pcp);
	}
	pp = pvn_write_kluster(vp, pp, &io_off, &io_len, pp->p_offset,
	    PAGESIZE, flags);

	if (fsp->pcfs_flags & PCFS_IRRECOV) {
		goto out;
	}

	PC_DPRINTF1(7, "pc_putpage writing dirty page off=%llu\n", io_off);

	lbn = pc_lblkno(fsp, io_off);
	lbnoff = io_off & ~(fsp->pcfs_clsize - 1);
	xferoffset = io_off & ~(fsp->pcfs_secsize - 1);

	for (pgoff = 0; pgoff < io_len && xferoffset < pcp->pc_size;
	    pgoff += xfersize,
	    lbn += howmany(xfersize, fsp->pcfs_clsize),
	    lbnoff += xfersize, xferoffset += xfersize) {

		struct buf *bp;
		int err1;

		/*
		 * write as many contiguous blocks as possible from this page
		 */
		xfersize = io_len - pgoff;
		err1 = pc_bmap(pcp, (daddr_t)lbn, &bn, &xfersize);
		if (err1) {
			err = err1;
			goto out;
		}
		bp = pageio_setup(pp, xfersize, devvp, B_WRITE | flags);
		bp->b_edev = devvp->v_rdev;
		bp->b_dev = cmpdev(devvp->v_rdev);
		bp->b_blkno = bn +
		    /* add a sector offset within the cluster */
		    /* when the clustersize > PAGESIZE */
		    (xferoffset - lbnoff) / fsp->pcfs_secsize;
		bp->b_un.b_addr = (caddr_t)(uintptr_t)pgoff;
		bp->b_file = vp;
		bp->b_offset = (offset_t)(io_off + pgoff);

		(void) bdev_strategy(bp);

		lwp_stat_update(LWP_STAT_OUBLK, 1);

		if (err == 0)
			err = biowait(bp);
		else
			(void) biowait(bp);
		pageio_done(bp);
	}
	pvn_write_done(pp, ((err) ? B_ERROR : 0) | B_WRITE | flags);
	pp = NULL;

out:
	if ((fsp->pcfs_flags & PCFS_IRRECOV) && pp != NULL) {
		pvn_write_done(pp, B_WRITE | flags);
	} else if (err != 0 && pp != NULL) {
		pvn_write_done(pp, B_ERROR | B_WRITE | flags);
	}

	if (offp)
		*offp = io_off;
	if (lenp)
		*lenp = io_len;
		PC_DPRINTF4(4, "pcfs_putapage: vp=%p pp=%p off=%lld len=%lu\n",
		    (void *)vp, (void *)pp, io_off, io_len);
	if (err) {
		PC_DPRINTF1(1, "pcfs_putapage err=%d", err);
	}
	return (err);
}

/*ARGSUSED*/
static int
pcfs_map(
	struct vnode *vp,
	offset_t off,
	struct as *as,
	caddr_t *addrp,
	size_t len,
	uchar_t prot,
	uchar_t maxprot,
	uint_t flags,
	struct cred *cr)
{
	struct segvn_crargs vn_a;
	int error;

	PC_DPRINTF0(6, "pcfs_map\n");
	if (vp->v_flag & VNOMAP)
		return (ENOSYS);

	if (off > UINT32_MAX || off + len > UINT32_MAX)
		return (ENXIO);

	as_rangelock(as);
	if ((flags & MAP_FIXED) == 0) {
		map_addr(addrp, len, off, 1, flags);
		if (*addrp == NULL) {
			as_rangeunlock(as);
			return (ENOMEM);
		}
	} else {
		/*
		 * User specified address - blow away any previous mappings
		 */
		(void) as_unmap(as, *addrp, len);
	}

	vn_a.vp = vp;
	vn_a.offset = off;
	vn_a.type = flags & MAP_TYPE;
	vn_a.prot = prot;
	vn_a.maxprot = maxprot;
	vn_a.flags = flags & ~MAP_TYPE;
	vn_a.cred = cr;
	vn_a.amp = NULL;
	vn_a.szc = 0;
	vn_a.lgrp_mem_policy_flags = 0;

	error = as_map(as, *addrp, len, segvn_create, &vn_a);
	as_rangeunlock(as);
	return (error);
}

/* ARGSUSED */
static int
pcfs_seek(
	struct vnode *vp,
	offset_t ooff,
	offset_t *noffp)
{
	if (*noffp < 0)
		return (EINVAL);
	else if (*noffp > MAXOFFSET_T)
		return (EINVAL);
	else
		return (0);
}

/* ARGSUSED */
static int
pcfs_addmap(
	struct vnode *vp,
	offset_t off,
	struct as *as,
	caddr_t addr,
	size_t len,
	uchar_t prot,
	uchar_t maxprot,
	uint_t flags,
	struct cred *cr)
{
	if (vp->v_flag & VNOMAP)
		return (ENOSYS);
	return (0);
}

/*ARGSUSED*/
static int
pcfs_delmap(
	struct vnode *vp,
	offset_t off,
	struct as *as,
	caddr_t addr,
	size_t len,
	uint_t prot,
	uint_t maxprot,
	uint_t flags,
	struct cred *cr)
{
	if (vp->v_flag & VNOMAP)
		return (ENOSYS);
	return (0);
}

/*
 * POSIX pathconf() support.
 */
/* ARGSUSED */
static int
pcfs_pathconf(
	struct vnode *vp,
	int cmd,
	ulong_t *valp,
	struct cred *cr)
{
	ulong_t val;
	int error = 0;
	struct statvfs64 vfsbuf;
	struct pcfs *fsp = VFSTOPCFS(vp->v_vfsp);

	switch (cmd) {

	case _PC_LINK_MAX:
		val = 1;
		break;

	case _PC_MAX_CANON:
		val = MAX_CANON;
		break;

	case _PC_MAX_INPUT:
		val = MAX_INPUT;
		break;

	case _PC_NAME_MAX:
		bzero(&vfsbuf, sizeof (vfsbuf));
		if (error = VFS_STATVFS(vp->v_vfsp, &vfsbuf))
			break;
		val = vfsbuf.f_namemax;
		break;

	case _PC_PATH_MAX:
	case _PC_SYMLINK_MAX:
		val = PCMAXPATHLEN;
		break;

	case _PC_PIPE_BUF:
		val = PIPE_BUF;
		break;

	case _PC_NO_TRUNC:
		val = (ulong_t)-1; 	/* Will truncate long file name */
		break;

	case _PC_VDISABLE:
		val = _POSIX_VDISABLE;
		break;

	case _PC_CHOWN_RESTRICTED:
		if (rstchown)
			val = rstchown;		/* chown restricted enabled */
		else
			val = (ulong_t)-1;
		break;

	case _PC_ACL_ENABLED:
		val = 0;
		break;

	case _PC_FILESIZEBITS:
		/*
		 * Both FAT16 and FAT32 support 4GB - 1 byte for file size.
		 * FAT12 can only go up to the maximum filesystem capacity
		 * which is ~509MB.
		 */
		val = IS_FAT12(fsp) ? 30 : 33;
		break;
	default:
		error = EINVAL;
		break;
	}

	if (error == 0)
		*valp = val;
	return (error);
}

/* ARGSUSED */
static int
pcfs_space(
	struct vnode *vp,
	int cmd,
	struct flock64 *bfp,
	int flag,
	offset_t offset,
	cred_t *cr,
	caller_context_t *ct)
{
	struct vattr vattr;
	int error;

	if (cmd != F_FREESP)
		return (EINVAL);

	if ((error = convoff(vp, bfp, 0, offset)) == 0) {
		if ((bfp->l_start > UINT32_MAX) || (bfp->l_len > UINT32_MAX))
			return (EFBIG);
		/*
		 * we only support the special case of l_len == 0,
		 * meaning free to end of file at this moment.
		 */
		if (bfp->l_len != 0)
			return (EINVAL);
		vattr.va_mask = AT_SIZE;
		vattr.va_size = bfp->l_start;
		error = VOP_SETATTR(vp, &vattr, 0, cr, ct);
	}
	return (error);
}

/*
 * Break up 'len' chars from 'buf' into a long file name chunk.
 * Pad with '0xff' to make Norton Disk Doctor and Microsoft ScanDisk happy.
 */
void
set_long_fn_chunk(struct pcdir_lfn *ep, char *buf, int len)
{
	char 	*tmp = buf;
	int	i;


	for (i = 0; i < PCLF_FIRSTNAMESIZE; i += 2) {
		if (len > 0) {
			ep->pcdl_firstfilename[i] = *tmp;
			ep->pcdl_firstfilename[i+1] = 0;
			len--;
			tmp++;
		} else {
			ep->pcdl_firstfilename[i] = (uchar_t)0xff;
			ep->pcdl_firstfilename[i+1] = (uchar_t)0xff;
		}
	}

	for (i = 0; i < PCLF_SECONDNAMESIZE; i += 2) {
		if (len > 0) {
			ep->pcdl_secondfilename[i] = *tmp;
			ep->pcdl_secondfilename[i+1] = 0;
			len--;
			tmp++;
		} else {
			ep->pcdl_secondfilename[i] = (uchar_t)0xff;
			ep->pcdl_secondfilename[i+1] = (uchar_t)0xff;
		}
	}
	for (i = 0; i < PCLF_THIRDNAMESIZE; i += 2) {
		if (len > 0) {
			ep->pcdl_thirdfilename[i] = *tmp;
			ep->pcdl_thirdfilename[i+1] = 0;
			len--;
			tmp++;
		} else {
			ep->pcdl_thirdfilename[i] = (uchar_t)0xff;
			ep->pcdl_thirdfilename[i+1] = (uchar_t)0xff;
		}
	}
}

/*
 * Extract the characters from the long filename chunk into 'buf'.
 * Return the number of characters extracted.
 */
static int
get_long_fn_chunk(struct pcdir_lfn *ep, char *buf, int foldcase)
{
	char 	*tmp = buf;
	int	i;

	for (i = 0; i < PCLF_FIRSTNAMESIZE; i += 2, tmp++) {
		if (ep->pcdl_firstfilename[i+1] != '\0')
			return (-1);
		if (foldcase)
			*tmp = tolower(ep->pcdl_firstfilename[i]);
		else
			*tmp = ep->pcdl_firstfilename[i];
		if (*tmp == '\0')
			return (tmp - buf);
	}
	for (i = 0; i < PCLF_SECONDNAMESIZE; i += 2, tmp++) {
		if (ep->pcdl_secondfilename[i+1] != '\0')
			return (-1);
		if (foldcase)
			*tmp = tolower(ep->pcdl_secondfilename[i]);
		else
			*tmp = ep->pcdl_secondfilename[i];
		if (*tmp == '\0')
			return (tmp - buf);
	}
	for (i = 0; i < PCLF_THIRDNAMESIZE; i += 2, tmp++) {
		if (ep->pcdl_thirdfilename[i+1] != '\0')
			return (-1);
		if (foldcase)
			*tmp = tolower(ep->pcdl_thirdfilename[i]);
		else
			*tmp = ep->pcdl_thirdfilename[i];
		if (*tmp == '\0')
			return (tmp - buf);
	}
	*tmp = '\0';
	return (tmp - buf);
}


/*
 * Checksum the passed in short filename.
 * This is used to validate each component of the long name to make
 * sure the long name is valid (it hasn't been "detached" from the
 * short filename). This algorithm was found in FreeBSD.
 * (sys/fs/msdosfs/msdosfs_conv.c:winChksum(), Wolfgang Solfrank)
 */

uchar_t
pc_checksum_long_fn(char *name, char *ext)
{
	uchar_t c;
	char	b[11];

	bcopy(name, b, 8);
	bcopy(ext, b+8, 3);

	c = b[0];
	c = ((c << 7) | (c >> 1)) + b[1];
	c = ((c << 7) | (c >> 1)) + b[2];
	c = ((c << 7) | (c >> 1)) + b[3];
	c = ((c << 7) | (c >> 1)) + b[4];
	c = ((c << 7) | (c >> 1)) + b[5];
	c = ((c << 7) | (c >> 1)) + b[6];
	c = ((c << 7) | (c >> 1)) + b[7];
	c = ((c << 7) | (c >> 1)) + b[8];
	c = ((c << 7) | (c >> 1)) + b[9];
	c = ((c << 7) | (c >> 1)) + b[10];

	return (c);
}

/*
 * Read a chunk of long filename entries into 'namep'.
 * Return with offset pointing to short entry (on success), or next
 * entry to read (if this wasn't a valid lfn really).
 * Uses the passed-in buffer if it can, otherwise kmem_allocs() room for
 * a long filename.
 *
 * Can also be called with a NULL namep, in which case it just returns
 * whether this was really a valid long filename and consumes it
 * (used by pc_dirempty()).
 */
int
pc_extract_long_fn(struct pcnode *pcp, char *namep,
    struct pcdir **epp, offset_t *offset, struct buf **bp)
{
	struct pcdir *ep = *epp;
	struct pcdir_lfn *lep = (struct pcdir_lfn *)ep;
	struct vnode *dvp = PCTOV(pcp);
	struct pcfs *fsp = VFSTOPCFS(dvp->v_vfsp);
	char	*lfn;
	char	*lfn_base;
	int	boff;
	int	i, cs;
	char	buf[20];
	uchar_t	cksum;
	int	detached = 0;
	int	error = 0;
	int	foldcase;

	foldcase = (fsp->pcfs_flags & PCFS_FOLDCASE);
	/* use callers buffer unless we didn't get one */
	if (namep)
		lfn_base = namep;
	else
		lfn_base = kmem_alloc(PCMAXNAMLEN+1, KM_SLEEP);
	lfn = lfn_base + PCMAXNAMLEN - 1;
	*lfn = '\0';
	cksum = lep->pcdl_checksum;

	for (i = (lep->pcdl_ordinal & ~0xc0); i > 0; i--) {
		/* read next block if necessary */
		boff = pc_blkoff(fsp, *offset);
		if (boff == 0 || *bp == NULL || boff >= (*bp)->b_bcount) {
			if (*bp != NULL) {
				brelse(*bp);
				*bp = NULL;
			}
			error = pc_blkatoff(pcp, *offset, bp, &ep);
			if (error) {
				if (namep == NULL)
					kmem_free(lfn_base, PCMAXNAMLEN+1);
				return (error);
			}
			lep = (struct pcdir_lfn *)ep;
		}
		/* can this happen? Bad fs? */
		if (!PCDL_IS_LFN((struct pcdir *)lep)) {
			detached = 1;
			break;
		}
		if (cksum != lep->pcdl_checksum)
			detached = 1;
		/* process current entry */
		cs = get_long_fn_chunk(lep, buf, foldcase);
		if (cs == -1) {
			detached = 1;
		} else {
			for (; cs > 0; cs--) {
				/* see if we underflow */
				if (lfn >= lfn_base)
					*--lfn = buf[cs - 1];
				else
					detached = 1;
			}
		}
		lep++;
		*offset += sizeof (struct pcdir);
	}
	/* read next block if necessary */
	boff = pc_blkoff(fsp, *offset);
	ep = (struct pcdir *)lep;
	if (boff == 0 || *bp == NULL || boff >= (*bp)->b_bcount) {
		if (*bp != NULL) {
			brelse(*bp);
			*bp = NULL;
		}
		error = pc_blkatoff(pcp, *offset, bp, &ep);
		if (error) {
			if (namep == NULL)
				kmem_free(lfn_base, PCMAXNAMLEN+1);
			return (error);
		}
	}
	/* should be on the short one */
	if (PCDL_IS_LFN(ep) || ((ep->pcd_filename[0] == PCD_UNUSED) ||
	    (ep->pcd_filename[0] == PCD_ERASED))) {
		detached = 1;
	}
	if (detached ||
	    (cksum != pc_checksum_long_fn(ep->pcd_filename, ep->pcd_ext)) ||
	    !pc_valid_long_fn(lfn)) {
		/*
		 * process current entry again. This may end up another lfn
		 * or a short name.
		 */
		*epp = ep;
		if (namep == NULL)
			kmem_free(lfn_base, PCMAXNAMLEN+1);
		return (EINVAL);
	}
	if (PCA_IS_HIDDEN(fsp, ep->pcd_attr)) {
		/*
		 * Don't display label because it may contain
		 * funny characters.
		 */
		*offset += sizeof (struct pcdir);
		ep++;
		*epp = ep;
		if (namep == NULL)
			kmem_free(lfn_base, PCMAXNAMLEN+1);
		return (EINVAL);
	}
	if (namep) {
		/* lfn is part of namep, but shifted. shift it back */
		cs = strlen(lfn);
		for (i = 0; i < cs; i++)
			namep[i] = lfn[i];
		namep[i] = '\0';
	} else {
		kmem_free(lfn_base, PCMAXNAMLEN+1);
	}
	*epp = ep;
	return (0);
}
/*
 * Read a long filename into the pc_dirent structure and copy it out.
 */
int
pc_read_long_fn(struct vnode *dvp, struct uio *uiop, struct pc_dirent *ld,
    struct pcdir **epp, offset_t *offset, struct buf **bp)
{
	struct pcdir *ep;
	struct pcnode *pcp = VTOPC(dvp);
	struct pcfs *fsp = VFSTOPCFS(dvp->v_vfsp);
	offset_t uiooffset = uiop->uio_loffset;
	int	error = 0;
	offset_t oldoffset;

	oldoffset = *offset;
	error = pc_extract_long_fn(pcp, ld->d_name, epp, offset, bp);
	if (error) {
		if (error == EINVAL) {
			uiop->uio_loffset += *offset - oldoffset;
			return (0);
		} else
			return (error);
	}

	ep = *epp;
	uiop->uio_loffset += *offset - oldoffset;
	ld->d_reclen = DIRENT64_RECLEN(strlen(ld->d_name));
	if (ld->d_reclen > uiop->uio_resid) {
		uiop->uio_loffset = uiooffset;
		return (ENOSPC);
	}
	ld->d_off = uiop->uio_loffset + sizeof (struct pcdir);
	ld->d_ino = pc_makenodeid(pc_daddrdb(fsp, (*bp)->b_blkno),
	    pc_blkoff(fsp, *offset), ep->pcd_attr,
	    pc_getstartcluster(fsp, ep), fsp->pcfs_entps);
	(void) uiomove((caddr_t)ld, ld->d_reclen, UIO_READ, uiop);
	uiop->uio_loffset = ld->d_off;
	*offset += sizeof (struct pcdir);
	ep++;
	*epp = ep;
	return (0);
}

/*
 * Read a short filename into the pc_dirent structure and copy it out.
 */
int
pc_read_short_fn(struct vnode *dvp, struct uio *uiop, struct pc_dirent *ld,
    struct pcdir **epp, offset_t *offset, struct buf **bp)
{
	struct pcfs *fsp = VFSTOPCFS(dvp->v_vfsp);
	int	boff = pc_blkoff(fsp, *offset);
	struct pcdir *ep = *epp;
	offset_t	oldoffset = uiop->uio_loffset;
	int	error;
	int	foldcase;

	if (PCA_IS_HIDDEN(fsp, ep->pcd_attr)) {
		uiop->uio_loffset += sizeof (struct pcdir);
		*offset += sizeof (struct pcdir);
		ep++;
		*epp = ep;
		return (0);
	}
	ld->d_ino = (ino64_t)pc_makenodeid(pc_daddrdb(fsp, (*bp)->b_blkno),
	    boff, ep->pcd_attr, pc_getstartcluster(fsp, ep), fsp->pcfs_entps);
	foldcase = (fsp->pcfs_flags & PCFS_FOLDCASE);
	error = pc_fname_ext_to_name(&ld->d_name[0], &ep->pcd_filename[0],
	    &ep->pcd_ext[0], foldcase);
	if (error == 0) {
		ld->d_reclen = DIRENT64_RECLEN(strlen(ld->d_name));
		if (ld->d_reclen > uiop->uio_resid) {
			uiop->uio_loffset = oldoffset;
			return (ENOSPC);
		}
		ld->d_off = (off64_t)(uiop->uio_loffset +
		    sizeof (struct pcdir));
		(void) uiomove((caddr_t)ld,
		    ld->d_reclen, UIO_READ, uiop);
		uiop->uio_loffset = ld->d_off;
	} else {
		uiop->uio_loffset += sizeof (struct pcdir);
	}
	*offset += sizeof (struct pcdir);
	ep++;
	*epp = ep;
	return (0);
}

static int
pcfs_fid(struct vnode *vp, struct fid *fidp)
{
	struct pc_fid *pcfid;
	struct pcnode *pcp;
	struct pcfs	*fsp;
	int	error;

	fsp = VFSTOPCFS(vp->v_vfsp);
	if (fsp == NULL)
		return (EIO);
	error = pc_lockfs(fsp, 0, 0);
	if (error)
		return (error);
	if ((pcp = VTOPC(vp)) == NULL) {
		pc_unlockfs(fsp);
		return (EIO);
	}
	if (fidp->fid_len < (sizeof (struct pc_fid) - sizeof (ushort_t))) {
		fidp->fid_len = sizeof (struct pc_fid) - sizeof (ushort_t);
		pc_unlockfs(fsp);
		return (ENOSPC);
	}

	pcfid = (struct pc_fid *)fidp;
	bzero(pcfid, sizeof (struct pc_fid));
	pcfid->pcfid_len = sizeof (struct pc_fid) - sizeof (ushort_t);
	if (vp->v_flag & VROOT) {
		pcfid->pcfid_block = 0;
		pcfid->pcfid_offset = 0;
		pcfid->pcfid_ctime = 0;
	} else {
		pcfid->pcfid_block = pcp->pc_eblkno;
		pcfid->pcfid_offset = pcp->pc_eoffset;
		pcfid->pcfid_ctime = pcp->pc_entry.pcd_crtime.pct_time;
	}
	pc_unlockfs(fsp);
	return (0);
}
