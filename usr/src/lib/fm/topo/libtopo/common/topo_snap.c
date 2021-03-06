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
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"@(#)topo_snap.c	1.1	06/02/11 SMI"

/*
 * Snapshot Library Interfaces
 *
 * Consumers of topology data may use the interfaces in this file to open,
 * snapshot and close a topology exported by FMRI scheme (hc, mem and cpu)
 * builtin plugins and their helper modules.  A topology handle is obtained
 * by calling topo_open().  Upon a successful return, the caller may use this
 * handle to open a new snapshot.  Each snapshot is assigned a Universally
 * Unique Identifier that in a future enchancement to the libtopo API will be
 * used as the file locator in /var/fm/topo to persist new snapshots or lookup
 * a previously captured snapshot.  topo_snap_hold() will capture the current
 * system topology.  All consumers of the topo_hdl_t argument will be
 * blocked from accessing the topology trees until the snapshot completes.
 *
 * A snapshot may be cleared by calling topo_snap_rele().  As with
 * topo_snap_hold(), all topology accesses are blocked until the topology
 * trees have been released and deallocated.
 *
 * Walker Library Interfaces
 *
 * Once a snapshot has been taken with topo_snap_hold(), topo_hdl_t holders
 * may initiate topology tree walks on a scheme-tree basis.  topo_walk_init()
 * will initiate the data structures required to walk any one one of the
 * FMRI scheme trees.  The walker data structure, topo_walk_t, is an opaque
 * handle passed to topo_walk_step to begin the walk.  At each node in the
 * topology tree, a callback function is called with access to the node at
 * which our current walk falls.  The callback function is passed in during
 * calls to topo_walk_init() and used throughout the walk_step of the
 * scheme tree.  At any time, the callback may terminate the walk by returning
 * TOPO_WALK_TERMINATE or TOPO_WALK_ERR.  TOPO_WALK_NEXT will continue the
 * walk.
 *
 * Walks through the tree may be breadth first or depth first by
 * respectively passing in TOPO_WALK_SIBLING or TOPO_WALK_CHILD to
 * the topo_walk_step() function.  Topology nodes associated with an
 * outstanding walk are held in place and will not be deallocated until
 * the walk through that node completes.
 *
 * Once the walk has terminated, the walking process should call
 * topo_walk_fini() to clean-up resources created in topo_walk_init()
 * and release nodes that may be still held.
 */

#include <pthread.h>
#include <limits.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <uuid/uuid.h>

#include <fm/libtopo.h>

#include <topo_alloc.h>
#include <topo_builtin.h>
#include <topo_string.h>
#include <topo_error.h>
#include <topo_subr.h>

static void topo_snap_destroy(topo_hdl_t *);

static topo_hdl_t *
set_open_errno(topo_hdl_t *thp, int *errp, int err)
{
	if (thp != NULL) {
		topo_close(thp);
	}
	if (errp != NULL)
		*errp = err;
	return (NULL);
}

topo_hdl_t *
topo_open(int version, const char *rootdir, int *errp)
{
	topo_hdl_t *thp = NULL;
	topo_alloc_t *tap;
	struct stat st;

	if (version < TOPO_VERSION)
		return (set_open_errno(thp, errp, ETOPO_HDL_VER));

	if (version > TOPO_VERSION)
		return (set_open_errno(thp, errp, ETOPO_HDL_VER));

	if (rootdir != NULL && stat(rootdir, &st) < 0)
		return (set_open_errno(thp, errp, ETOPO_HDL_INVAL));

	if ((thp = topo_zalloc(sizeof (topo_hdl_t), 0)) == NULL)
		return (set_open_errno(thp, errp, ETOPO_NOMEM));

	if ((tap = topo_zalloc(sizeof (topo_alloc_t), 0)) == NULL)
		return (set_open_errno(thp, errp, ETOPO_NOMEM));

	/*
	 * Install default allocators
	 */
	tap->ta_flags = 0;
	tap->ta_alloc = topo_alloc;
	tap->ta_zalloc = topo_zalloc;
	tap->ta_free = topo_free;
	tap->ta_nvops.nv_ao_alloc = topo_nv_alloc;
	tap->ta_nvops.nv_ao_free = topo_nv_free;
	(void) nv_alloc_init(&tap->ta_nva, &tap->ta_nvops);
	thp->th_alloc = tap;

	if ((thp->th_modhash = topo_modhash_create(thp)) == NULL)
		return (set_open_errno(thp, errp, ETOPO_NOMEM));

	if (rootdir == NULL) {
		rootdir = topo_hdl_strdup(thp, "/");
		thp->th_rootdir = (char *)rootdir;
	} else {
		if (strlen(rootdir) > PATH_MAX)
			return (set_open_errno(thp, errp, EINVAL));

		thp->th_rootdir = topo_hdl_strdup(thp, rootdir);
	}

	if (thp->th_rootdir == NULL)
		return (set_open_errno(thp, errp, ETOPO_NOMEM));

	if (topo_builtin_create(thp, thp->th_rootdir) != 0) {
		topo_dprintf(TOPO_DBG_ERR, "failed to load builtin modules: "
		    "%s\n", topo_hdl_errmsg(thp));
		return (NULL);
	}

	return (thp);
}

void
topo_close(topo_hdl_t *thp)
{
	ttree_t *tp;

	topo_hdl_lock(thp);
	if (thp->th_rootdir != NULL)
		topo_hdl_strfree(thp, thp->th_rootdir);

	/*
	 * Clean-up snapshot
	 */
	topo_snap_destroy(thp);

	/*
	 * Clean-up trees
	 */
	while ((tp = topo_list_next(&thp->th_trees)) != NULL) {
		topo_list_delete(&thp->th_trees, tp);
		topo_tree_destroy(thp, tp);
	}

	/*
	 * Unload all plugins
	 */
	topo_modhash_unload_all(thp);

	if (thp->th_modhash != NULL)
		topo_modhash_destroy(thp);
	if (thp->th_alloc != NULL)
		topo_free(thp->th_alloc, sizeof (topo_alloc_t));

	topo_hdl_unlock(thp);

	topo_free(thp, sizeof (topo_hdl_t));
}

static char *
topo_snap_create(topo_hdl_t *thp, int *errp)
{
	uuid_t uuid;
	char *ustr = NULL;

	topo_hdl_lock(thp);
	if (thp->th_uuid != NULL) {
		*errp = ETOPO_HDL_UUID;
		topo_hdl_unlock(thp);
		return (NULL);
	}

	if ((thp->th_uuid = topo_hdl_zalloc(thp, TOPO_UUID_SIZE)) == NULL) {
		*errp = ETOPO_NOMEM;
		topo_dprintf(TOPO_DBG_ERR, "unable to allocate uuid: %s\n",
		    topo_strerror(*errp));
		topo_hdl_unlock(thp);
		return (NULL);
	}

	uuid_generate(uuid);
	uuid_unparse(uuid, thp->th_uuid);

	if (topo_tree_enum_all(thp) < 0) {
		topo_dprintf(TOPO_DBG_ERR, "enumeration failure: %s\n",
		    topo_hdl_errmsg(thp));
		if (topo_hdl_errno(thp) != ETOPO_ENUM_PARTIAL) {
			*errp = thp->th_errno;
			topo_hdl_unlock(thp);
			return (NULL);
		}
	}

	if ((ustr = topo_hdl_strdup(thp, thp->th_uuid)) == NULL)
		*errp = ETOPO_NOMEM;

	topo_hdl_unlock(thp);

	return (ustr);
}

/*ARGSUSED*/
static char *
topo_snap_log_create(topo_hdl_t *thp, const char *uuid, int *errp)
{
	return ((char *)uuid);
}

/*
 * Return snapshot id
 */
char *
topo_snap_hold(topo_hdl_t *thp, const char *uuid, int *errp)
{
	if (thp == NULL)
		return (NULL);

	if (uuid == NULL)
		return (topo_snap_create(thp, errp));
	else
		return (topo_snap_log_create(thp, uuid, errp));
}

/*ARGSUSED*/
static int
topo_walk_destroy(topo_hdl_t *thp, tnode_t *node, void *notused)
{
	tnode_t *cnode;

	cnode = topo_child_first(node);

	if (cnode != NULL)
		return (TOPO_WALK_NEXT);

	topo_node_unbind(node);

	return (TOPO_WALK_NEXT);
}

static void
topo_snap_destroy(topo_hdl_t *thp)
{
	int i;
	ttree_t *tp;
	topo_walk_t *twp;
	tnode_t *root;
	topo_nodehash_t *nhp;
	topo_mod_t *mod;

	for (tp = topo_list_next(&thp->th_trees); tp != NULL;
	    tp = topo_list_next(tp)) {

		root = tp->tt_root;
		twp = tp->tt_walk;
		/*
		 * Clean-up tree nodes from the bottom-up
		 */
		if ((twp->tw_node = topo_child_first(root)) != NULL) {
			twp->tw_cb = topo_walk_destroy;
			topo_node_hold(root);
			topo_node_hold(twp->tw_node); /* released at walk end */
			(void) topo_walk_bottomup(twp, TOPO_WALK_CHILD);
			topo_node_rele(root);
		}

		/*
		 * Tidy-up the root node
		 */
		while ((nhp = topo_list_next(&root->tn_children)) != NULL) {
			for (i = 0; i < nhp->th_arrlen; i++) {
				assert(nhp->th_nodearr[i] == NULL);
			}
			mod = nhp->th_enum;
			topo_mod_strfree(mod, nhp->th_name);
			topo_mod_free(mod, nhp->th_nodearr,
			    nhp->th_arrlen * sizeof (tnode_t *));
			topo_list_delete(&root->tn_children, nhp);
			topo_mod_free(mod, nhp, sizeof (topo_nodehash_t));
			topo_mod_rele(mod);
		}

		/*
		 * Release the file handle
		 */
		if (tp->tt_file != NULL)
			topo_file_unload(thp, tp);

	}

}

void
topo_snap_release(topo_hdl_t *thp)
{
	if (thp == NULL)
		return;

	topo_hdl_lock(thp);
	if (thp->th_uuid != NULL) {
		topo_hdl_free(thp, thp->th_uuid, TOPO_UUID_SIZE);
		topo_snap_destroy(thp);
		thp->th_uuid = NULL;
	}
	topo_hdl_unlock(thp);

}

topo_walk_t *
topo_walk_init(topo_hdl_t *thp, const char *scheme, topo_walk_cb_t cb_f,
    void *pdata, int *errp)
{
	tnode_t *child;
	ttree_t *tp;
	topo_walk_t *wp;

	for (tp = topo_list_next(&thp->th_trees); tp != NULL;
	    tp = topo_list_next(tp)) {
		if (strcmp(scheme, tp->tt_scheme) == 0) {

			/*
			 * Hold the root node and start walk at the first
			 * child node
			 */
			assert(tp->tt_root != NULL);

			topo_node_hold(tp->tt_root);

			/*
			 * Nothing to walk
			 */
			if ((child = topo_child_first(tp->tt_root)) == NULL) {
				*errp = ETOPO_WALK_EMPTY;
				topo_node_rele(tp->tt_root);
				return (NULL);
			}

			if ((wp	= topo_hdl_zalloc(thp, sizeof (topo_walk_t)))
			    == NULL) {
				*errp = ETOPO_NOMEM;
				topo_node_rele(tp->tt_root);
				return (NULL);
			}

			topo_node_hold(child);

			wp->tw_root = tp->tt_root;
			wp->tw_node = child;
			wp->tw_cb = cb_f;
			wp->tw_pdata = pdata;
			wp->tw_thp = thp;

			return (wp);
		}
	}

	*errp = ETOPO_WALK_NOTFOUND;
	return (NULL);
}

static int
step_child(tnode_t *cnp, topo_walk_t *wp, int bottomup)
{
	int status;
	tnode_t *nnp;

	nnp = topo_child_first(cnp);

	if (nnp == NULL)
		return (TOPO_WALK_TERMINATE);

	topo_dprintf(TOPO_DBG_WALK, "walk through child node %s=%d\n",
	    nnp->tn_name, nnp->tn_instance);

	topo_node_hold(nnp); /* released on return from walk_step */
	wp->tw_node = nnp;
	if (bottomup == 1)
		status = topo_walk_bottomup(wp, TOPO_WALK_CHILD);
	else
		status = topo_walk_step(wp, TOPO_WALK_CHILD);

	return (status);
}

static int
step_sibling(tnode_t *cnp, topo_walk_t *wp, int bottomup)
{
	int status;
	tnode_t *nnp;

	nnp = topo_child_next(cnp->tn_parent, cnp);

	if (nnp == NULL)
		return (TOPO_WALK_TERMINATE);

	topo_dprintf(TOPO_DBG_WALK, "walk through sibling node %s=%d\n",
	    nnp->tn_name, nnp->tn_instance);

	topo_node_hold(nnp); /* released on return from walk_step */
	wp->tw_node = nnp;
	if (bottomup == 1)
		status = topo_walk_bottomup(wp, TOPO_WALK_CHILD);
	else
		status = topo_walk_step(wp, TOPO_WALK_CHILD);

	return (status);
}

int
topo_walk_step(topo_walk_t *wp, int flag)
{
	int status;
	tnode_t *cnp = wp->tw_node;

	if (flag != TOPO_WALK_CHILD && flag != TOPO_WALK_SIBLING) {
		topo_node_rele(cnp);
		return (TOPO_WALK_ERR);
	}

	/*
	 * End of the line
	 */
	if (cnp == NULL) {
		topo_dprintf(TOPO_DBG_WALK, "walk_step terminated\n");
		topo_node_rele(cnp);
		return (TOPO_WALK_TERMINATE);
	}

	topo_dprintf(TOPO_DBG_WALK, "%s walk_step through node %s=%d\n",
	    (flag == TOPO_WALK_CHILD ? "TOPO_WALK_CHILD" : "TOPO_WALK_SIBLING"),
	    cnp->tn_name, cnp->tn_instance);

	if ((status = wp->tw_cb(wp->tw_thp, cnp, wp->tw_pdata))
	    != TOPO_WALK_NEXT) {
		topo_node_rele(cnp);
		return (status);
	}

	if (flag == TOPO_WALK_CHILD)
		status = step_child(cnp, wp, 0);
	else
		status = step_sibling(cnp, wp, 0);

	/*
	 * End of the walk, try next child or sibling
	 */
	if (status == TOPO_WALK_TERMINATE) {
		if (flag == TOPO_WALK_CHILD)
			status = step_sibling(cnp, wp, 0);
		else
			status = step_child(cnp, wp, 0);
	}

	topo_node_rele(cnp); /* done with current node */

	return (status);
}

void
topo_walk_fini(topo_walk_t *wp)
{
	if (wp == NULL)
		return;

	topo_node_rele(wp->tw_root);

	topo_hdl_free(wp->tw_thp, wp, sizeof (topo_walk_t));
}

int
topo_walk_bottomup(topo_walk_t *wp, int flag)
{
	int status;
	tnode_t *cnp;

	if (wp == NULL)
		return (TOPO_WALK_ERR);

	cnp = wp->tw_node;
	if (flag != TOPO_WALK_CHILD && flag != TOPO_WALK_SIBLING) {
		topo_node_rele(cnp);
		return (TOPO_WALK_ERR);
	}

	/*
	 * End of the line
	 */
	if (cnp == NULL) {
		topo_dprintf(TOPO_DBG_WALK, "walk_bottomup terminated\n");
		topo_node_rele(cnp);
		return (TOPO_WALK_TERMINATE);
	}

	topo_dprintf(TOPO_DBG_WALK, "%s walk_bottomup through node %s=%d\n",
	    (flag == TOPO_WALK_CHILD ? "TOPO_WALK_CHILD" : "TOPO_WALK_SIBLING"),
	    cnp->tn_name, cnp->tn_instance);

	if (flag == TOPO_WALK_CHILD)
		status = step_child(cnp, wp, 1);
	else
		status = step_sibling(cnp, wp, 1);

	/*
	 * At a leaf, run the callback
	 */
	if (status == TOPO_WALK_TERMINATE) {
		if ((status = wp->tw_cb(wp->tw_thp, cnp, wp->tw_pdata))
		    != TOPO_WALK_NEXT) {
			topo_node_rele(cnp);
			return (status);
		}
	}

	/*
	 * Try next child or sibling
	 */
	if (status == TOPO_WALK_NEXT) {
		if (flag == TOPO_WALK_CHILD)
			status = step_sibling(cnp, wp, 1);
		else
			status = step_child(cnp, wp, 1);
	}

	topo_node_rele(cnp); /* done with current node */

	return (status);
}
