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
 * Copyright 2005 Cyril Plisko.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"%Z%%M%	%I%	%E% CP"

/*
 * chrp root nexus driver
 */
#include <sys/conf.h>
#include <sys/modctl.h>
#ifdef XXX_DDI_SUBRDEFS
#include <sys/ddi_subrdefs.h>
#endif
#include <sys/devops.h>
#include <sys/sunndi.h>
#include <sys/rootnex.h>

static int rootnex_attach(dev_info_t *dip, ddi_attach_cmd_t cmd);

static	int rootnex_map(dev_info_t *dip, dev_info_t *rdip,
    ddi_map_req_t *mp, off_t offset, off_t len, caddr_t *vaddrp);

static	int rootnex_map_fault(dev_info_t *dip, dev_info_t *rdip,
    struct hat *hat, struct seg *seg, caddr_t addr, struct devpage *dp,
    pfn_t pfn, uint_t prot, uint_t lock);

static	int rootnex_dma_map(dev_info_t *dip, dev_info_t *rdip,
    struct ddi_dma_req *dmareq, ddi_dma_handle_t *handlep);

static	int rootnex_dma_allochdl(dev_info_t *dip, dev_info_t *rdip,
    ddi_dma_attr_t *attr, int (*waitfp)(caddr_t), caddr_t arg,
    ddi_dma_handle_t *handlep);

static  int rootnex_dma_freehdl(dev_info_t *dip, dev_info_t *rdip,
    ddi_dma_handle_t handle);

static	int rootnex_dma_bindhdl(dev_info_t *dip, dev_info_t *rdip,
    ddi_dma_handle_t handle, struct ddi_dma_req *dmareq,
    ddi_dma_cookie_t *, uint_t *);

static	int rootnex_dma_unbindhdl(dev_info_t *dip, dev_info_t *rdip,
    ddi_dma_handle_t handle);

static	int rootnex_dma_flush(dev_info_t *dip, dev_info_t *rdip,
    ddi_dma_handle_t handle, off_t off, size_t len, uint_t cache_flags);

static	int rootnex_dma_win(dev_info_t *dip, dev_info_t *rdip,
    ddi_dma_handle_t handle, uint_t win, off_t *offp,
    size_t *lenp, ddi_dma_cookie_t *cookiep, uint_t *ccountp);

static	int rootnex_dma_ctl(dev_info_t *dip, dev_info_t *rdip,
    ddi_dma_handle_t handle, enum ddi_dma_ctlops request, off_t *offp,
    size_t *lenp, caddr_t *objp, uint_t flags);

static	int rootnex_ctl(dev_info_t *dip, dev_info_t *rdip,
    ddi_ctl_enum_t ctlop, void *arg, void *result);

static	int rootnex_intr_op(dev_info_t *dip, dev_info_t *rdip,
    ddi_intr_op_t op, ddi_intr_handle_impl_t *hdlp, void *result);


static struct cb_ops rootnex_cb_ops = {
	nodev,		/* open */
	nodev,		/* close */
	nodev,		/* strategy */
	nodev,		/* print */
	nodev,		/* dump */
	nodev,		/* read */
	nodev,		/* write */
	nodev,		/* ioctl */
	nodev,		/* devmap */
	nodev,		/* mmap */
	nodev,		/* segmap */
	nochpoll,	/* chpoll */
	ddi_prop_op,	/* cb_prop_op */
	NULL,		/* struct streamtab */
	D_NEW | D_MP | D_HOTPLUG, /* compatibility flags */
	CB_REV,
	nodev,		/* aread */
	nodev 		/* awrite */
};

static struct bus_ops rootnex_bus_ops = {
	BUSO_REV,
	rootnex_map,
	NULL,		/* bus_get_intrspec (obsolete) */
	NULL,		/* bus_add_intrspec (obsolete) */
	NULL,		/* bus_remove_intrspec (obsolete) */
	rootnex_map_fault,
	rootnex_dma_map,
	rootnex_dma_allochdl,
	rootnex_dma_freehdl,
	rootnex_dma_bindhdl,
	rootnex_dma_unbindhdl,
	rootnex_dma_flush,
	rootnex_dma_win,
	rootnex_dma_ctl,
	rootnex_ctl,
	ddi_bus_prop_op,
	i_ddi_rootnex_get_eventcookie,
	i_ddi_rootnex_add_eventcall,
	i_ddi_rootnex_remove_eventcall,
	i_ddi_rootnex_post_event,
	NULL,		/* bus_intr_ctl (obsolete) */
	NULL,		/* bus_config */
	NULL,		/* bus_unconfig */
	NULL,		/* bus_fm_init */
	NULL,		/* bus_fm_fini */
	NULL,		/* bus_fm_access_enter */
	NULL,		/* bus_fm_access_exit */
	NULL,		/* bus_power */
	rootnex_intr_op
};

static struct dev_ops rootnex_ops = {
	DEVO_REV,
	0,
	ddi_no_info,
	nulldev,	/* identify (obsolete) */
	nulldev,	/* probe */
	rootnex_attach,
	nodev,
	nodev,		/* reset (not supported) */
	NULL,
	&rootnex_bus_ops,
	NULL		/* power */
};

static struct modldrv modldrv = {
	&mod_driverops,
	"chrp root nexus driver",
	&rootnex_ops
};

static struct modlinkage modlinkage = {
	MODREV_1,
	&modldrv,
	NULL
};

/*
 * This is our state structure. Vast majority of the drivers
 * keep the state in the linked list, but specific driver is
 * in "Highlander" mode - there can be only one instance of
 * root nexus
 */
static rootnex_state_t	rootnex_state;

int
_init(void)
{
	return (mod_install(&modlinkage));
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

int
_fini(void)
{
	return (EBUSY);
}

static int
rootnex_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	rootnex_state_t	*sp = &rootnex_state;

	switch (cmd) {
	case DDI_ATTACH:
		/* Fall through, the real work is below */
		break;
	case DDI_RESUME:
		/* Nothing to do for resume right now */
		return (DDI_SUCCESS);
	default:
		return (DDI_FAILURE);
	}

	sp->dip = dip;

	/*
	 * Apparently we cannot use ddi_report_dev() here [according to
	 * i86pc/io/rootnex.c] so fallback to cmn_err()
	 * Indeed, we _are_ the root nexus, so we gotta handle things
	 */
	cmn_err(CE_CONT, "?root nexus = %s\n", ddi_get_name(dip));

	return (DDI_SUCCESS);
}

/*
 * bus_map() entry point
 */
static int
rootnex_map(dev_info_t *dip, dev_info_t *rdip, ddi_map_req_t *mp,
    off_t offset, off_t len, caddr_t *vaddrp)
{
	ddi_map_req_t mr = *mp;	/* Create a private copy of map request */

	switch (mr.map_op) {
	case DDI_MO_MAP_LOCKED:
	case DDI_MO_UNMAP:
	case DDI_MO_MAP_HANDLE:
		break;
	default:
		return (DDI_ME_UNIMPLEMENTED);
	}

	if (mr.map_flags & DDI_MF_USER_MAPPING) {
		return (DDI_ME_UNIMPLEMENTED);
	}

	return (DDI_ME_UNIMPLEMENTED);
}

static int
rootnex_map_fault(dev_info_t *dip, dev_info_t *rdip, struct hat *hat,
    struct seg *seg, caddr_t addr, struct devpage *dp, pfn_t pfn,
    uint_t prot, uint_t lock)
{
	return (DDI_FAILURE);
}

static int
rootnex_dma_map(dev_info_t *dip, dev_info_t *rdip,
    struct ddi_dma_req *dmareq, ddi_dma_handle_t *handlep)
{
	return (DDI_FAILURE);
}

static int
rootnex_dma_allochdl(dev_info_t *dip, dev_info_t *rdip,
    ddi_dma_attr_t *attr, int (*waitfp)(caddr_t), caddr_t arg,
    ddi_dma_handle_t *handlep)
{
	return (DDI_FAILURE);
}

static int
rootnex_dma_freehdl(dev_info_t *dip, dev_info_t *rdip,
    ddi_dma_handle_t handle)
{
	return (DDI_FAILURE);
}

static int
rootnex_dma_bindhdl(dev_info_t *dip, dev_info_t *rdip, ddi_dma_handle_t handle,
    struct ddi_dma_req *dmareq, ddi_dma_cookie_t *cookie, uint_t *flags)
{
	return (DDI_FAILURE);
}

static int
rootnex_dma_unbindhdl(dev_info_t *dip, dev_info_t *rdip,
    ddi_dma_handle_t handle)
{
	return (DDI_FAILURE);
}

static int
rootnex_dma_flush(dev_info_t *dip, dev_info_t *rdip, ddi_dma_handle_t handle,
    off_t off, size_t len, uint_t cache_flags)
{
	return (DDI_FAILURE);
}

static int
rootnex_dma_win(dev_info_t *dip, dev_info_t *rdip, ddi_dma_handle_t handle,
    uint_t win, off_t *offp, size_t *lenp, ddi_dma_cookie_t *cookiep,
    uint_t *ccountp)
{
	return (DDI_FAILURE);
}

static int
rootnex_dma_ctl(dev_info_t *dip, dev_info_t *rdip, ddi_dma_handle_t handle,
    enum ddi_dma_ctlops request, off_t *offp, size_t *lenp, caddr_t *objp,
    uint_t flags)
{
	return (DDI_FAILURE);
}

static int
rootnex_ctl(dev_info_t *dip, dev_info_t *rdip, ddi_ctl_enum_t ctlop,
    void *arg, void *result)
{
	return (DDI_FAILURE);
}

static int
rootnex_intr_op(dev_info_t *dip, dev_info_t *rdip, ddi_intr_op_t op,
    ddi_intr_handle_impl_t *hdlp, void *result)
{
	return (DDI_FAILURE);
}
