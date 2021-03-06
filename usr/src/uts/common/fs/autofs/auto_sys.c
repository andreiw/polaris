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

#pragma ident	"@(#)auto_sys.c	1.4	06/06/09 SMI"

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/zone.h>
#include <sys/errno.h>
#include <sys/cred.h>
#include <sys/policy.h>

#include <sys/fs/autofs.h>

extern struct autofs_globals *autofs_zone_init(void);

int
autofssys(enum autofssys_op opcode, uintptr_t arg)
{
	int error = 0;

	switch (opcode) {
	case AUTOFS_UNMOUNTALL: { /* attempt to remove all autofs mounts */
		zone_t *zone;
		zoneid_t zoneid;
		struct autofs_globals *fngp;

		zoneid = (zoneid_t)arg;
		if (secpolicy_fs_unmount(CRED(), NULL) != 0 ||
		    crgetzoneid(CRED()) != GLOBAL_ZONEID)
			return (set_errno(EPERM));
		if ((zone = zone_find_by_id(zoneid)) == NULL)
			return (set_errno(EINVAL));
		fngp = zone_getspecific(autofs_key, zone);
		if (fngp == NULL) {
			zone_rele(zone);
			/*
			 * There were no mounts, so no work to do. Success.
			 */
			return (0);
		}
		unmount_tree(fngp, 1);
		zone_rele(zone);
		break;
	}
	case AUTOFS_SETDOOR: { /* set door handle for zone */
		uint_t did;
		struct autofs_globals *fngp;

		fngp = zone_getspecific(autofs_key, curproc->p_zone);
		if (fngp == NULL) {
			fngp = autofs_zone_init();
			(void) zone_setspecific(autofs_key,
						curproc->p_zone, fngp);
		}
		ASSERT(fngp != NULL);

		if (copyin((uint_t *)arg, &did, sizeof (uint_t)))
			return (set_errno(EFAULT));

		mutex_enter(&fngp->fng_autofs_daemon_lock);
		if (fngp->fng_autofs_daemon_dh)
			door_ki_rele(fngp->fng_autofs_daemon_dh);
		fngp->fng_autofs_daemon_dh = door_ki_lookup(did);
		fngp->fng_autofs_pid = curproc->p_pid;
		mutex_exit(&fngp->fng_autofs_daemon_lock);
		break;
	}
	default:
		error = EINVAL;
		break;
	}
	return (error ? set_errno(error) : 0);
}
