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

/*
 * This file contains support required for IB cfgadm plugin.
 */

#pragma ident	"@(#)ibnex_ioctl.c	1.16	05/12/09 SMI"


#include <sys/conf.h>
#include <sys/stat.h>
#include <sys/modctl.h>
#include <sys/ib/mgt/ibdm/ibdm_impl.h>
#include <sys/ib/ibnex/ibnex.h>
#include <sys/ib/ibnex/ibnex_devctl.h>
#include <sys/ib/ibtl/impl/ibtl_ibnex.h>
#include <sys/file.h>
#include <sys/sunndi.h>
#include <sys/fs/dv_node.h>
#include <sys/mdi_impldefs.h>
#include <sys/sunmdi.h>

/*
 * function prototypes
 */
int			ibnex_open(dev_t *, int, int, cred_t *);
int			ibnex_close(dev_t, int, int, cred_t *);
int			ibnex_ioctl(dev_t, int, intptr_t, int, cred_t *, int *);
int			ibnex_offline_childdip(dev_info_t *);
static int		ibnex_get_num_devices(void);
static int		ibnex_get_snapshot(char **, size_t *, int);
static int		ibnex_get_commsvcnode_snapshot(nvlist_t **, ib_guid_t,
			    ib_guid_t, int, ib_pkey_t, ibnex_node_type_t);
static int		ibnex_fill_ioc_tmp(nvlist_t **, ibdm_ioc_info_t *);
static int		ibnex_fill_nodeinfo(nvlist_t **, ibnex_node_data_t *,
			    void *);
static void		ibnex_figure_ap_devstate(dev_info_t *,
			    devctl_ap_state_t *);
static void		ibnex_figure_ib_apid_devstate(devctl_ap_state_t *);
static	char 		*ibnex_get_apid(struct devctl_iocdata *);
static int		ibnex_get_dip_from_apid(char *, dev_info_t **,
			    ibnex_node_data_t **);
static ibnex_rval_t	ibnex_handle_pseudo_configure(char *);
static ibnex_rval_t	ibnex_handle_ioc_configure(char *);
static ibnex_rval_t	ibnex_handle_commsvcnode_configure(char *);
static void		ibnex_return_apid(dev_info_t *, char **);
static void		ibnex_port_conf_entry_add(char *);
static void		ibnex_vppa_conf_entry_add(char *);
static void		ibnex_hcasvc_conf_entry_add(char *);
static int		ibnex_port_conf_entry_delete(char *, char *);
static int		ibnex_vppa_conf_entry_delete(char *, char *);
static int		ibnex_hcasvc_conf_entry_delete(char *, char *);

static ibnex_rval_t	ibnex_ioc_fininode(dev_info_t *, ibnex_ioc_node_t *);
static ibnex_rval_t	ibnex_commsvc_fininode(dev_info_t *);
static ibnex_rval_t	ibnex_pseudo_fininode(dev_info_t *);

extern uint64_t		ibnex_str2hex(char *, int, int *);
extern int		ibnex_ioc_initnode(ibdm_ioc_info_t *, int,
			    dev_info_t *);
extern dev_info_t	*ibnex_commsvc_initnode(dev_info_t *,
			    ibdm_port_attr_t *, int, int, ib_pkey_t, int *,
			    int);
extern int		ibnex_get_dip_from_guid(ib_guid_t, int,
			    ib_pkey_t, dev_info_t **);
extern void		ibnex_reprobe_ioc_dev(void *arg);
extern void		ibnex_reprobe_ioc_all();
extern int		ibnex_pseudo_create_pi(ibnex_node_data_t *,
			    dev_info_t *);
extern void		ibnex_pseudo_initnodes(void);

extern ibnex_t	ibnex;

/*
 * ibnex_open()
 */
/* ARGSUSED */
int
ibnex_open(dev_t *dev, int flag, int otyp, cred_t *credp)
{
	IBTF_DPRINTF_L4("ibnex", "\topen");
	return (0);
}


/*
 * ibnex_close()
 */
/* ARGSUSED */
int
ibnex_close(dev_t dev, int flag, int otyp, cred_t *credp)
{
	IBTF_DPRINTF_L4("ibnex", "\tclose");
	return (0);
}


/*
 * ibnex_ioctl()
 *	Ioctl routine for cfgadm controls
 *	DEVCTL_AP_GETSTATE:	returns attachment point state
 *	DEVCTL_AP_CONTROL:	Does "ibnex" specific ioctls listed below
 *		IBNEX_NUM_DEVICE_NODES	Gives how many device nodes exist?
 *		IBNEX_NUM_HCA_NODES	Gives how many HCAs exist in the fabric
 *		IBNEX_UPDATE_PKEY_TBLS	"-x update_pkey_tbls"
 *		IBNEX_GET_SNAPSHOT	Gets the "snapshot" back to user-land
 *		IBNEX_SNAPSHOT_SIZE	What is "snapshot" size
 *		IBNEX_DEVICE_PATH_SZ	What is device-path size
 *		IBNEX_GET_DEVICE_PATH	Gets the device path for Dynamic ap
 *		IBNEX_HCA_LIST_SZ	"-x list" option size for the HCA ap_id
 *		IBNEX_HCA_LIST_INFO	"-x list" option info for the HCA ap_id
 *		IBNEX_UNCFG_CLNTS_SZ	"-x unconfig_client option size"
 *		IBNEX_UNCFG_CLNTS_INFO	"-x unconfig_client data"
 *		IBNEX_CONF_ENTRY_ADD:	"-x add_service"
 *		IBNEX_CONF_ENTRY_DEL:	"-x delete_service"
 *		IBNEX_HCA_VERBOSE_SZ:	"-alv hca_apid data size"
 *		IBNEX_HCA_VERBOSE_INFO: "-alv hca_apid actual data"
 *		IBNEX_UPDATE_IOC_CONF	"-x update_ioc_conf"
 *	DEVCTL_AP_CONFIGURE:	"configure" the attachment point
 *	DEVCTL_AP_UNCONFIGURE:	"unconfigure" the attachment point
 */
/* ARGSUSED */
int
ibnex_ioctl(dev_t dev, int cmd, intptr_t arg, int mode, cred_t *credp,
    int *rvalp)
{
	int			ret, rv = 0, ioc_reprobe_pending = 0;
	int			circ;
	char			*snapshot = NULL;
	char			*apid_n = NULL;
	char			*service = NULL;
	char			*devnm = NULL;
	char			*msg;
	char			*guid_str;
	uint_t			num_hcas = 0;
	size_t			snapshot_sz  = 0;
	uint32_t		ssiz;
	uint32_t		apid_len;
	ib_guid_t		hca_guid;
	boolean_t		apid_alloced = B_FALSE;
	dev_info_t		*apid_dip = NULL;
	dev_info_t		*pdip;
	ibnex_rval_t		ret_val;
	ib_service_type_t	svc_type = IB_NONE;
	devctl_ap_state_t	ap_state;
	ibnex_node_data_t	*nodep, *scanp;
	struct devctl_iocdata	*dcp = NULL;

	IBTF_DPRINTF_L4("ibnex", "\tioctl: cmd=%x, arg=%p, mode=%x, cred=%p, "
	    "\t\trval=%p dev=0x%x", cmd, arg, mode, credp, rvalp, dev);

	/* read devctl ioctl data */
	if ((cmd != DEVCTL_AP_CONTROL) &&
	    (ndi_dc_allochdl((void *)arg, &dcp) != NDI_SUCCESS)) {
		IBTF_DPRINTF_L4("ibnex",
		    "\tioctl: ndi_dc_allochdl failed\n");
		return (EFAULT);
	}

	mutex_enter(&ibnex.ibnex_mutex);
	switch (cmd) {
	case DEVCTL_AP_GETSTATE:
		msg = "\tioctl: DEVCTL_AP_GETSTATE";
		IBTF_DPRINTF_L4("ibnex", "%s:", msg);

		apid_n = ibnex_get_apid(dcp);
		if (*apid_n == '\0') {
			IBTF_DPRINTF_L2("ibnex",
			    "%s: ibnex_get_apid failed", msg);
			rv = EIO;
			break;
		}

		if (strncmp(apid_n, IBNEX_FABRIC, strlen(IBNEX_FABRIC)) == 0) {
			ibnex_figure_ib_apid_devstate(&ap_state);
			apid_dip = ibnex.ibnex_dip;
		} else {
			/* if this apid is already seen by IBNEX, get the dip */
			rv = ibnex_get_dip_from_apid(apid_n, &apid_dip, &nodep);
			if (rv != IBNEX_DYN_APID) {
				IBTF_DPRINTF_L2("ibnex",
				    "%s: ibnex_get_dip_from_apid failed", msg);
				rv = EIO;
				break;
			}
			if (apid_dip)
				ndi_rele_devi(apid_dip);
			/* rv could be something undesirable, so reset it */
			rv = 0;

			ibnex_figure_ap_devstate(apid_dip, &ap_state);
		}

		/* copy the return-AP-state information to the user space */
		if (ndi_dc_return_ap_state(&ap_state, dcp) != NDI_SUCCESS) {
			IBTF_DPRINTF_L2("ibnex",
			    "%s: ndi_dc_return_ap_state failed", msg);
			rv = EFAULT;
		}
		break;

	case DEVCTL_AP_CONTROL:
	{
		int			num_nodes = 0;
		ibnex_ioctl_data_t	ioc;	/* for 64-bit copies only */

		msg = "\tioctl: DEVCTL_AP_CONTROL";
#ifdef	_MULTI_DATAMODEL
		if (ddi_model_convert_from(mode & FMODELS) == DDI_MODEL_ILP32) {
			ibnex_ioctl_data_32_t ioc32;

			if (ddi_copyin((void *)arg, &ioc32,
			    sizeof (ioc32), mode) != 0) {
				IBTF_DPRINTF_L2("ibnex",
				    "%s: ddi_copyin err 1", msg);
				rv = EFAULT;
				break;
			}
			ioc.cmd		= (uint_t)ioc32.cmd;
			ioc.buf		= (caddr_t)(uintptr_t)ioc32.buf;
			ioc.bufsiz	= (uint_t)ioc32.bufsiz;
			ioc.ap_id	= (caddr_t)(uintptr_t)ioc32.ap_id;
			ioc.ap_id_len	= (uint_t)ioc32.ap_id_len;
			ioc.misc_arg	= (uint_t)ioc32.misc_arg;
		}
#else
		if (ddi_copyin((void *)arg, &ioc, sizeof (ioc),
		    mode) != 0) {
			IBTF_DPRINTF_L2("ibnex",
			    "%s: ddi_copyin 2 failed", msg);
			rv = EFAULT;
			break;
		}
#endif	/* _MULTI_DATAMODEL */

		IBTF_DPRINTF_L4("ibnex", "%s: \n\tioc: cmd=%x buf=%p, "
		    "bufsiz=%d", msg, ioc.cmd, ioc.buf, ioc.bufsiz);

		/*
		 * figure out ap_id name as passed from user-land
		 * NOTE: We don't need to figure out ap_id for these
		 * two sub-commands:-
		 *	IBNEX_NUM_DEVICE_NODES, IBNEX_NUM_HCA_NODES
		 *
		 * Hence, In user-land, these two ioctls force "ap_id_len" to 0.
		 */
		if (ioc.ap_id_len > 0) {
			apid_alloced = B_TRUE;
			apid_len = ioc.ap_id_len + 1;
			apid_n = kmem_zalloc(apid_len, KM_SLEEP);
			if (ddi_copyin((void *)ioc.ap_id, apid_n,
			    ioc.ap_id_len, mode) != 0) {
				IBTF_DPRINTF_L2("ibnex",
				    "%s: ddi_copyin err 3", msg);
				rv = EFAULT;
				break;
			}

			IBTF_DPRINTF_L3("ibnex", "%s: apid_n= %s", msg, apid_n);
		}


		/* process sub-commands */
		switch (ioc.cmd) {
		case IBNEX_NUM_DEVICE_NODES:
			msg = "\tioctl: DEVCTL_AP_CONTROL: NUM_DEVICE_NODES";

			/*
			 * figure out how many IOC, VPPA,
			 * Pseudo and Port nodes are present
			 */
			num_nodes = ibnex_get_num_devices();
			IBTF_DPRINTF_L4("ibnex", "%s: num_nodes = %d",
			    msg, num_nodes);

			if (ddi_copyout(&num_nodes, ioc.buf,
			    ioc.bufsiz, mode) != 0) {
				IBTF_DPRINTF_L2("ibnex", "%s: copyout", msg);
				rv = EIO;
			}
			mutex_exit(&ibnex.ibnex_mutex);
			return (rv);

		case IBNEX_NUM_HCA_NODES:
			msg = "\tioctl: DEVCTL_AP_CONTROL: NUM_HCA_NODES";

			/* figure out how many HCAs are present in the host */
			mutex_exit(&ibnex.ibnex_mutex);
			num_hcas = ibt_get_hca_list(NULL);
			IBTF_DPRINTF_L4("ibnex", "%s: num %d", msg, num_hcas);

			if (ddi_copyout(&num_hcas, ioc.buf,
			    ioc.bufsiz, mode) != 0) {
				IBTF_DPRINTF_L2("ibnex", "%s: copyout", msg);
				rv = EIO;
			}
			return (rv);

		case IBNEX_UPDATE_PKEY_TBLS:
			msg = "\tioctl: DEVCTL_AP_CONTROL: UPDATE_PKEY_TBLS";
			IBTF_DPRINTF_L4("ibnex", "%s", msg);

			/*
			 * update P_Key tables:
			 *	ibdm_ibnex_update_pkey_tbls() calls
			 *	ibt_query_hca_ports_byguids() for all the
			 *	HCAs that the IBDM has "seen" in the system.
			 *	This ends up updating the IBTL P_Key database.
			 *	NOTE: Changes in this area will break this
			 *	assumption. Initially the plan was to call
			 *	ibt_query_hca_ports_byguids() in IBTL but
			 *	IBDM needs to call it as well. So, eliminating
			 *	the first invocation.
			 *
			 *	It next updates the DM P_Key database.
			 *	Note that the DM P_Key database updating
			 *	will always be driven through cfgadm.
			 */
			mutex_exit(&ibnex.ibnex_mutex);
			ibdm_ibnex_update_pkey_tbls();
			mutex_enter(&ibnex.ibnex_mutex);
			break;

		case IBNEX_GET_SNAPSHOT:
		case IBNEX_SNAPSHOT_SIZE:
			msg = (ioc.cmd == IBNEX_SNAPSHOT_SIZE) ?
			    "\tioctl: DEVCTL_AP_CONTROL: IBNEX_SNAPSHOT_SIZE" :
			    "\tioctl: DEVCTL_AP_CONTROL: IBNEX_GET_SNAPSHOT";

			IBTF_DPRINTF_L4("ibnex", "%s:", msg);

			if (ibnex_get_snapshot(&snapshot, &snapshot_sz,
			    ioc.misc_arg) != 0) {
				IBTF_DPRINTF_L2("ibnex",
				    "%s:\n\tibnex_get_snapshot failed", msg);
				rv = EIO;
				break;
			}

			/* ssiz needs to be reinitialized again */
			ssiz = snapshot_sz;
			IBTF_DPRINTF_L4("ibnex",
			    "%s:\n\tsize =%x", msg, snapshot_sz);

			if (ioc.cmd == IBNEX_SNAPSHOT_SIZE) {
				if (ddi_copyout(&ssiz, ioc.buf,
				    ioc.bufsiz, mode) != 0) {
					IBTF_DPRINTF_L2("ibnex",
					    "%s:\n\tddi_copyout 2 failed", msg);
					rv = EFAULT;
				}

			} else {
				if (ioc.bufsiz != snapshot_sz) {
					IBTF_DPRINTF_L2("ibnex",
					    "%s:\n\tinvalid buffer size (%x %x)"
					    " ", msg, ioc.bufsiz, snapshot_sz);
					rv = EINVAL;

				} else if (ddi_copyout(snapshot, ioc.buf,
				    ioc.bufsiz, mode) != 0) {
					IBTF_DPRINTF_L2("ibnex",
					    "%s:\n\tddi_copyout 3 failed", msg);
					rv = EFAULT;
				}
			}

			kmem_free(snapshot, snapshot_sz);
			break;

		case IBNEX_DEVICE_PATH_SZ:
		case IBNEX_GET_DEVICE_PATH:
		{
			char	 path[MAXPATHLEN];

			msg = (ioc.cmd == IBNEX_DEVICE_PATH_SZ) ?
			    "\tioctl:DEVCTL_AP_CONTROL: IBNEX_DEVICE_PATH_SZ" :
			    "\tioctl:DEVCTL_AP_CONTROL: IBNEX_GET_DEVICE_PATH";

			IBTF_DPRINTF_L4("ibnex", "%s: apid = %s", msg, apid_n);

			/* if this apid is already seen by IBNEX, get the dip */
			rv = ibnex_get_dip_from_apid(apid_n, &apid_dip, &nodep);
			if (rv != IBNEX_DYN_APID || apid_dip == NULL) {
				IBTF_DPRINTF_L2("ibnex",
				    "%s:\n\tget_dip_from_apid failed", msg);
				rv = EIO;
				break;
			}
			ndi_rele_devi(apid_dip);

			/* ddi_pathname doesn't supply /devices, so we do. */
			(void) strcpy(path, "/devices");
			(void) ddi_pathname(apid_dip, path + strlen(path));
			ssiz = (uint32_t)strlen(path) + 1;
			IBTF_DPRINTF_L4("ibnex",
			    "%s: len = %x\n\tpath = %s", msg, ssiz, path);

			/* rv could be something undesirable, so reset it */
			rv = 0;

			if (ioc.cmd == IBNEX_DEVICE_PATH_SZ) {
				if (ddi_copyout(&ssiz, ioc.buf,
				    ioc.bufsiz, mode) != 0) {
					IBTF_DPRINTF_L2("ibnex",
					    "%s: ddi_copyout 4 failed", msg);
					rv = EFAULT;
				}

			} else {
				if (ioc.bufsiz != ssiz) {
					IBTF_DPRINTF_L2("ibnex",
					    "%s: invalid size (%x, %x)",
					    msg, ioc.bufsiz, ssiz);
					rv = EINVAL;
				} else if (ddi_copyout(&path, ioc.buf,
				    ioc.bufsiz, mode) != 0) {
					IBTF_DPRINTF_L2("ibnex", "%s "
					    "ddi_copyout 5 failed", msg);
					rv = EFAULT;
				}
			}
			break;
		}

		case IBNEX_HCA_LIST_SZ:
		case IBNEX_HCA_LIST_INFO:
			msg = (ioc.cmd == IBNEX_HCA_LIST_SZ) ?
			    "DEVCTL_AP_CONTROL: IBNEX_HCA_LIST_SZ" :
			    "DEVCTL_AP_CONTROL: IBNEX_HCA_LIST_INFO";

			guid_str = strrchr(apid_n, ':') + 1;
			IBTF_DPRINTF_L4("ibnex", "%s, input apid = %s, "
			    "guid = %s", msg, apid_n, guid_str);

			if (guid_str == NULL) {
				IBTF_DPRINTF_L2("ibnex", "%s: invalid input "
				    "GUID passed %s", msg, guid_str);
				rv = EFAULT;
				break;
			}

			/* Get the GUID(hex value) from apid_n */
			hca_guid = ibnex_str2hex(guid_str, strlen(guid_str),
			    &ret);
			if (ret != IBNEX_SUCCESS) {
				IBTF_DPRINTF_L2("ibnex", "%s: Invalid HCA "
				    "GUID string", msg);
				rv = EIO;
				break;
			}
			IBTF_DPRINTF_L4("ibnex", "%s HCA GUID = %llX",
			    msg, hca_guid);
			if (ibtl_ibnex_get_hca_info(hca_guid,
			    IBTL_IBNEX_LIST_CLNTS_FLAG, &snapshot, &snapshot_sz,
			    ibnex_return_apid) != IBT_SUCCESS) {
				IBTF_DPRINTF_L2("ibnex",
				    "%s: get HCA consumers failed", msg);
				rv = EIO;
				break;
			}

			ssiz = snapshot_sz;
			IBTF_DPRINTF_L4("ibnex", "%s: size =%x", msg, ssiz);

			if (ioc.cmd == IBNEX_HCA_LIST_SZ) {
				if (ddi_copyout(&ssiz, ioc.buf,
				    ioc.bufsiz, mode) != 0) {
					IBTF_DPRINTF_L2("ibnex",
					    "%s: ddi_copyout 6 failed", msg);
					rv = EFAULT;
				}
			} else {
				if (ioc.bufsiz != ssiz) {
					IBTF_DPRINTF_L2("ibnex", "%s: invalid "
					    "size (%x, %x)", msg, ioc.bufsiz,
					    ssiz);
					rv = EINVAL;
				} else if (ddi_copyout(snapshot, ioc.buf,
				    ioc.bufsiz, mode) != 0) {
					IBTF_DPRINTF_L2("ibnex", "%s "
					    "ddi_copyout 7 failed", msg);
					rv = EFAULT;
				}
			}

			kmem_free(snapshot, snapshot_sz);
			break;

		case IBNEX_UNCFG_CLNTS_SZ:
		case IBNEX_UNCFG_CLNTS_INFO:
			msg = (ioc.cmd == IBNEX_UNCFG_CLNTS_SZ) ?
			    "\tioctl:DEVCTL_AP_CONTROL: IBNEX_UNCFG_CLNTS_SZ" :
			    "\tioctl:DEVCTL_AP_CONTROL: IBNEX_UNCFG_CLNTS_INFO";

			guid_str = strrchr(apid_n, ':') + 1;
			IBTF_DPRINTF_L4("ibnex", "%s, apid = %s, guid = %s",
			    msg, apid_n, guid_str);

			if (guid_str == NULL) {
				IBTF_DPRINTF_L2("ibnex", "%s: invalid input "
				    "GUID %s", msg, guid_str);
				rv = EFAULT;
				break;
			}

			/* Get the GUID(hex value) from apid_n */
			hca_guid = ibnex_str2hex(guid_str, strlen(guid_str),
			    &ret);
			if (ret != IBNEX_SUCCESS) {
				IBTF_DPRINTF_L2("ibnex", "%s: Invalid HCA "
				    "GUID string passed", msg);
				rv = EIO;
				break;
			}
			IBTF_DPRINTF_L4("ibnex", "%s G = %llX", msg, hca_guid);
			if (ibtl_ibnex_get_hca_info(hca_guid,
			    IBTL_IBNEX_UNCFG_CLNTS_FLAG, &snapshot,
			    &snapshot_sz, ibnex_return_apid) != IBT_SUCCESS) {
				IBTF_DPRINTF_L2("ibnex",
				    "%s: get HCA consumers failed", msg);
				rv = EIO;
				break;
			}
			/* ssiz needs to be reinitialized again */
			ssiz = snapshot_sz;

			IBTF_DPRINTF_L4("ibnex", "%s: size =%x", msg, ssiz);

			if (ioc.cmd == IBNEX_UNCFG_CLNTS_SZ) {
				if (ddi_copyout(&ssiz, ioc.buf,
				    ioc.bufsiz, mode) != 0) {
					IBTF_DPRINTF_L2("ibnex",
					    "%s: ddi_copyout 9 failed", msg);
					rv = EFAULT;
				}

			} else {
				if (ioc.bufsiz != ssiz) {
					IBTF_DPRINTF_L2("ibnex",
					    "%s: invalid size (%x, %x)",
					    msg, ioc.bufsiz, ssiz);
					rv = EINVAL;
				} else if (ddi_copyout(snapshot, ioc.buf,
				    ioc.bufsiz, mode) != 0) {
					IBTF_DPRINTF_L2("ibnex", "%s "
					    "ddi_copyout 10 failed", msg);
					rv = EFAULT;
				}
			}

			kmem_free(snapshot, snapshot_sz);
			break;

		case IBNEX_CONF_ENTRY_ADD:
			msg = "\tioctl: IBNEX_CONF_ENTRY_ADD: ";
			service = kmem_zalloc(ioc.bufsiz + 1, KM_SLEEP);
			/* read in the "service" name */
			if (ddi_copyin(ioc.buf, service,
			    ioc.bufsiz, mode) != 0) {
				IBTF_DPRINTF_L2("ibnex", "%s: ddi_copyin err 6",
				    msg);
				rv = EFAULT;
				break;
			}

			/* read in the "service type" */
			svc_type = ioc.misc_arg;
			IBTF_DPRINTF_L4("ibnex", "%s: service = %s, type = %d",
			    msg, service, svc_type);

			if (svc_type == IB_PORT_SERVICE) {
				ibnex_port_conf_entry_add(service);
			} else if (svc_type == IB_VPPA_SERVICE) {
				ibnex_vppa_conf_entry_add(service);
			} else if (svc_type == IB_HCASVC_SERVICE) {
				ibnex_hcasvc_conf_entry_add(service);
			}
			kmem_free(service, ioc.bufsiz + 1);
			break;

		case IBNEX_CONF_ENTRY_DEL:
			msg = "\tioctl:IBNEX_CONF_ENTRY_DEL: ";
			service = kmem_zalloc(ioc.bufsiz + 1, KM_SLEEP);
			/* read in the "service" name */
			if (ddi_copyin(ioc.buf, service,
			    ioc.bufsiz, mode) != 0) {
				IBTF_DPRINTF_L2("ibnex", "%s: ddi_copyin err 7",
				    msg);
				rv = EFAULT;
				break;
			}

			/* read in the "service type" */
			svc_type = ioc.misc_arg;
			IBTF_DPRINTF_L4("ibnex", "%s: service = %s, type = %d",
			    msg, service, svc_type);

			if (svc_type == IB_PORT_SERVICE) {
				rv = ibnex_port_conf_entry_delete(msg, service);
			} else if (svc_type == IB_VPPA_SERVICE) {
				rv = ibnex_vppa_conf_entry_delete(msg, service);
			} else if (svc_type == IB_HCASVC_SERVICE) {
				rv = ibnex_hcasvc_conf_entry_delete(msg,
				    service);
			}
			kmem_free(service, ioc.bufsiz + 1);
			break;

		case IBNEX_HCA_VERBOSE_SZ:
		case IBNEX_HCA_VERBOSE_INFO:
			msg = (ioc.cmd == IBNEX_HCA_VERBOSE_SZ) ?
			    "DEVCTL_AP_CONTROL: IBNEX_HCA_VERBOSE_SZ" :
			    "DEVCTL_AP_CONTROL: IBNEX_HCA_VERBOSE_INFO";

			guid_str = strrchr(apid_n, ':') + 1;
			IBTF_DPRINTF_L4("ibnex", "%s, apid = %s, guid = %s",
			    msg, apid_n, guid_str);

			if (guid_str == NULL) {
				IBTF_DPRINTF_L2("ibnex", "%s: invalid GUID %s",
				    msg, guid_str);
				rv = EFAULT;
				break;
			}

			/* Get the GUID(hex value) from apid_n */
			hca_guid = ibnex_str2hex(guid_str, strlen(guid_str),
			    &ret);
			if (ret != IBNEX_SUCCESS) {
				IBTF_DPRINTF_L2("ibnex", "%s: Invalid HCA GUID "
				    "string", msg);
				rv = EIO;
				break;
			}
			IBTF_DPRINTF_L4("ibnex", "%s HCA GUID = 0x%llX",
			    msg, hca_guid);

			if (ibtl_ibnex_get_hca_verbose_data(hca_guid, &snapshot,
			    &snapshot_sz) != IBT_SUCCESS) {
				IBTF_DPRINTF_L2("ibnex", "%s: get HCA verbose "
				    "data failed", msg);
				rv = EIO;
				break;
			}

			ssiz = snapshot_sz;
			IBTF_DPRINTF_L4("ibnex", "%s: size =%x", msg, ssiz);

			if (ioc.cmd == IBNEX_HCA_VERBOSE_SZ) {
				if (ddi_copyout(&ssiz, ioc.buf,
				    ioc.bufsiz, mode) != 0) {
					IBTF_DPRINTF_L2("ibnex",
					    "%s: ddi_copyout 11 failed", msg);
					rv = EFAULT;
				}
			} else {
				if (ioc.bufsiz != ssiz) {
					IBTF_DPRINTF_L2("ibnex",
					    "%s: invalid size (%x, %x)",
					    msg, ioc.bufsiz, ssiz);
					rv = EINVAL;
				} else if (ddi_copyout(snapshot,
				    ioc.buf, ioc.bufsiz, mode) != 0) {
					IBTF_DPRINTF_L2("ibnex", "%s "
					    "ddi_copyout 12 failed", msg);
					rv = EFAULT;
				}
			}

			kmem_free(snapshot, snapshot_sz);
			break;

		case IBNEX_UPDATE_IOC_CONF :
			msg = "\tioctl:IBNEX_UPDATE_IOC_CONF: ";

			/*
			 * If IB fabric APID, call ibnex_update_all
			 * If IOC APID, get the apid dip and call
			 * ibnex_update_ioc
			 */
			if (ioc.misc_arg == IBNEX_BASE_APID) {
				/*
				 * If reprobe is in progress or another reprobe
				 * is already waiting, wait.
				 */
				if (ibnex.ibnex_reprobe_state != 0) {
					if (ibnex.ibnex_reprobe_state ==
					    IBNEX_REPROBE_ALL_PROGRESS)
						ibnex.ibnex_reprobe_state =
						    IBNEX_REPROBE_ALL_WAIT;
					while (ibnex.ibnex_reprobe_state) {
						cv_wait(&ibnex.ibnex_reprobe_cv,
						    &ibnex.ibnex_mutex);
					}

					/*
					 * Pending reprobe all completed, return
					 */
					break;
				}

				/* Check if reprobe for any IOC is pending */
				/* CONSTCOND */
				while (1) {
					ioc_reprobe_pending = 0;
					for (scanp = ibnex.ibnex_ioc_node_head;
					    scanp;
					    scanp = scanp->node_next) {
						if (scanp->node_reprobe_state
						    != 0) {
							ioc_reprobe_pending =
							    1;
							break;
						}
					}
					if (ioc_reprobe_pending == 0) {
						ibnex.ibnex_reprobe_state &=
						    ~IBNEX_REPROBE_IOC_WAIT;
						break;
					}

					ibnex.ibnex_reprobe_state =
					    IBNEX_REPROBE_IOC_WAIT;
					cv_wait(&ibnex.ibnex_reprobe_cv,
					    &ibnex.ibnex_mutex);
				}

				/*
				 * Set the REPROBE_ALL_PROGRESS state &
				 * start reprobe
				 */
				ibnex.ibnex_reprobe_state =
				    IBNEX_REPROBE_ALL_PROGRESS;
				mutex_exit(&ibnex.ibnex_mutex);
				ibnex_reprobe_ioc_all();
				mutex_enter(&ibnex.ibnex_mutex);
			} else if (ioc.misc_arg == IBNEX_DYN_APID) {
				rv = ibnex_get_dip_from_apid(apid_n, &apid_dip,
				    &nodep);
				ASSERT(rv == IBNEX_DYN_APID);

				/* Device unconfigured: return */
				if (apid_dip == NULL)
					break;

				ndi_rele_devi(apid_dip);
				/* Reset return value back to 0 */
				rv = 0;
				if (ibnex.ibnex_reprobe_state != 0 ||
				    nodep->node_reprobe_state != 0) {
					while (ibnex.ibnex_reprobe_state != 0 &&
					    nodep->node_reprobe_state != 0) {
						cv_wait(&ibnex.ibnex_reprobe_cv,
						    &ibnex.ibnex_mutex);
					}
					/* Pending reprobe completed, return */
					break;
				}

				/* Set node_reprobe_state and start reprobe */
				nodep->node_reprobe_state =
				    IBNEX_NODE_REPROBE_NOTIFY_ON_UPDATE;
				mutex_exit(&ibnex.ibnex_mutex);
				ibnex_reprobe_ioc_dev((void *)apid_dip);
				mutex_enter(&ibnex.ibnex_mutex);
			} else {
				rv = EINVAL;
			}

			break;

		default:
			IBTF_DPRINTF_L2("ibnex",
			    "DEVCTL_AP_CONTROL: ioc:unknown cmd = %x", ioc.cmd);
			break;
		}
	}
	break;

	case DEVCTL_AP_UNCONFIGURE:
		msg = "DEVCTL_AP_UNCONFIGURE";
		IBTF_DPRINTF_L4("ibnex", "%s", msg);

		/* Check for write permissions */
		if (!(mode & FWRITE)) {
			IBTF_DPRINTF_L2("ibnex", "%s: invalid mode %x",
			    msg, mode);
			rv = EPERM;
			break;
		}

		if ((apid_n = ibnex_get_apid(dcp)) == '\0') {
			IBTF_DPRINTF_L2("ibnex",
			    "%s: ibnex_get_apid failed", msg);
			rv = EIO;
			break;
		}

		/*
		 * If this apid is already seen by IBNEX, get the dip
		 * NOTE: ibnex_get_dip_from_apid() finds a valid dip
		 * and also does a ndi_devi_hold() on the child.
		 */
		rv = ibnex_get_dip_from_apid(apid_n, &apid_dip, &nodep);
		if ((rv != IBNEX_DYN_APID) || (apid_dip == NULL)) {
			IBTF_DPRINTF_L2("ibnex", "%s: get_dip_from_apid "
			    "failed with 0x%x", msg, rv);
			rv = EIO;
			break;
		}
		IBTF_DPRINTF_L4("ibnex", "%s: DIP = %p", msg, apid_dip);

		/* Check if it is a valid node type? */
		if (!IBNEX_VALID_NODE_TYPE(nodep)) {
			IBTF_DPRINTF_L2("ibnex", "%s: invalid IB node", msg);
			rv = ENODEV;
			ndi_rele_devi(apid_dip);
			break;
		}

		/*
		 * continue unconfigure operation, only if device node
		 * is already configured. Return EBUSY if another
		 * configure/unconfigure operation is in progress.
		 */
		if (nodep->node_state == IBNEX_CFGADM_CONFIGURING ||
		    nodep->node_state == IBNEX_CFGADM_UNCONFIGURING) {
			rv = EBUSY;
			ndi_rele_devi(apid_dip);
			break;
		}

		/* do this before to avoid races */
		nodep->node_dip = NULL;
		nodep->node_state = IBNEX_CFGADM_UNCONFIGURING;

		/*
		 * Call devfs_clean first
		 * NOTE: The code so far is protected by holding ibnex_mutex
		 * and by doing a ndi_devi_hold() on the child.
		 */
		pdip = ddi_get_parent(apid_dip);
		if (i_ddi_node_state(apid_dip) >= DS_INITIALIZED) {
			devnm = kmem_alloc(MAXNAMELEN + 1, KM_SLEEP);
			(void) ddi_deviname(apid_dip, devnm);
			mutex_exit(&ibnex.ibnex_mutex);
			if (devfs_clean(pdip, devnm + 1, DV_CLEAN_FORCE)) {
				IBTF_DPRINTF_L2("ibnex", "%s: devfs_clean of %s"
				    " failed", msg, devnm);
				rv = EBUSY;
				mutex_enter(&ibnex.ibnex_mutex);
				ndi_rele_devi(apid_dip);
				nodep->node_dip = apid_dip;
				nodep->node_state = IBNEX_CFGADM_CONFIGURED;
				kmem_free(devnm, MAXNAMELEN + 1);
				break;
			}
			mutex_enter(&ibnex.ibnex_mutex);
			kmem_free(devnm, MAXNAMELEN + 1);
		}

		mutex_exit(&ibnex.ibnex_mutex);
		ndi_devi_enter(pdip, &circ);
		ndi_rele_devi(apid_dip);
		mutex_enter(&ibnex.ibnex_mutex);

		/* unconfigure the Port/VPPA/HCA_SVC node */
		if (IBNEX_COMMSVC_NODE_TYPE(nodep)) {
			ret_val = ibnex_commsvc_fininode(apid_dip);
		} else if (nodep->node_type == IBNEX_IOC_NODE) {
			/* unconfigure the IOC node */
			ret_val = ibnex_ioc_fininode(apid_dip,
			    &nodep->node_data.ioc_node);
		} else if (nodep->node_type == IBNEX_PSEUDO_NODE) {
			/* unconfigure the pseudo node */
			ret_val = ibnex_pseudo_fininode(apid_dip);
		}

		/* reset upon failure */
		if (ret_val != IBNEX_SUCCESS) {
			nodep->node_dip = apid_dip;
			nodep->node_state = IBNEX_CFGADM_CONFIGURED;
		} else
			nodep->node_state = IBNEX_CFGADM_UNCONFIGURED;

		rv = (ret_val != IBNEX_SUCCESS) ? EIO : 0;
		ndi_devi_exit(pdip, circ);
		IBTF_DPRINTF_L2("ibnex", "%s: DONE !! It %s", msg,
		    rv ? "failed" : "succeeded");
		break;

	case DEVCTL_AP_CONFIGURE:
		msg = "DEVCTL_AP_CONFIGURE";
		IBTF_DPRINTF_L4("ibnex", "%s", msg);
		mutex_exit(&ibnex.ibnex_mutex);
		ndi_devi_enter(ibnex.ibnex_dip, &circ);
		mutex_enter(&ibnex.ibnex_mutex);

		/* Check for write permissions */
		if (!(mode & FWRITE)) {
			IBTF_DPRINTF_L2("ibnex", "%s: invalid mode %x",
			    msg, mode);
			rv = EPERM;
			ndi_devi_exit(ibnex.ibnex_dip, circ);
			break;
		}

		if ((apid_n = ibnex_get_apid(dcp)) == '\0') {
			IBTF_DPRINTF_L2("ibnex",
			    "%s: ibnex_get_apid failed", msg);
			rv = EIO;
			ndi_devi_exit(ibnex.ibnex_dip, circ);
			break;
		}

		/*
		 * Five types of APIDs are supported:
		 *	o HCA_GUID,0,service-name	(HCA-SVC device)
		 *	o IOC_GUID 			(IOC device)
		 *	o PORT_GUID,0,service-name	(Port device)
		 *	o pseudo_name,unit-address, 	(Pseudo device)
		 *	o PORT_GUID,P_Key,service-name	(VPPA device)
		 * If the apid doesn't have "," then treat it as an IOC
		 * If the apid has one "," then it is Pseudo device
		 * If the apid has 2 ","s then it is one of the
		 * Port,VPPA,HCA_SVC devices
		 */
		if (strrchr(apid_n, ',') == NULL) {
			ret_val = ibnex_handle_ioc_configure(apid_n);
		} else {
			char *first = strchr(apid_n, ',');
			char *second;

			second = first ? strchr(first + 1, ',') : NULL;
			if (first != NULL && second == NULL) {
				ret_val = ibnex_handle_pseudo_configure(apid_n);
			} else if (first != NULL && second != NULL) {
				ret_val = ibnex_handle_commsvcnode_configure(
				    apid_n);
			}
		} /* end of else */

		if (ret_val != IBNEX_SUCCESS)
			rv = (ret_val == IBNEX_BUSY) ? EBUSY : EIO;
		IBTF_DPRINTF_L2("ibnex", "%s: DONE !! It %s", msg,
		    rv ? "failed" : "succeeded");
		ndi_devi_exit(ibnex.ibnex_dip, circ);
		break;

	default:
		rv = EIO;
		break;
	}
	mutex_exit(&ibnex.ibnex_mutex);

	if ((apid_alloced == B_TRUE) && (apid_n != NULL)) {
		kmem_free(apid_n, apid_len);
	}

	if (dcp) {
		ndi_dc_freehdl(dcp);
	}
	return (rv);
}


/*
 * ibnex_get_num_devices()
 *	Figure out how many IOC, VPPA, Pseudo, HCA_SVC and Port devices exist
 */
static int
ibnex_get_num_devices(void)
{
	int			j, k, l, hca_count;
	int			num_nodes = 0;
	ibdm_hca_list_t		*hca_list, *hcap;
	ibdm_port_attr_t	*pattr;
	ibnex_node_data_t	*nodep;

	ASSERT(mutex_owned(&ibnex.ibnex_mutex));

	/* Get a count of HCAs, first. */
	mutex_exit(&ibnex.ibnex_mutex);
	ibdm_ibnex_get_hca_list(&hca_list, &hca_count);
	mutex_enter(&ibnex.ibnex_mutex);
	for (hcap = hca_list; hca_list != NULL; hca_list = hca_list->hl_next) {
		for (j = 0; j < ibnex.ibnex_nhcasvc_comm_svcs; j++)
			num_nodes++;
		for (j = 0; j < hca_list->hl_nports; j++) {
			for (k = 0; k < ibnex.ibnex_num_comm_svcs; k++)
				num_nodes++;

			pattr = &hca_list->hl_port_attr[j];
			for (k = 0; k < pattr->pa_npkeys; k++) {
				if (IBNEX_INVALID_PKEY(pattr->pa_pkey_tbl[k].
				    pt_pkey))
					continue;

				for (l = 0; l < ibnex.ibnex_nvppa_comm_svcs;
				    l++, ++num_nodes);
			} /* end of pa_npkeys */
		} /* end of  hl_nports */
	} /* end of hca_list != NULL */
	if (hcap)
		ibdm_ibnex_free_hca_list(hcap);

	/*
	 * Now figure out how many IOC nodes are present.
	 * Add count of configured "diconnected" IOCs
	 */
	mutex_exit(&ibnex.ibnex_mutex);
	num_nodes += ibdm_ibnex_get_ioc_count();
	mutex_enter(&ibnex.ibnex_mutex);
	num_nodes += ibnex.ibnex_num_disconnect_iocs;

	/* Last: figure out how many Pseudo nodes are present. */
	for (nodep = ibnex.ibnex_pseudo_node_head; nodep;
	    nodep = nodep->node_next) {
		if (nodep->node_data.pseudo_node.pseudo_merge_node == 1)
			continue;

		num_nodes++;
	}
	return (num_nodes);
}


/*
 * ibnex_get_snapshot()
 *	Get a snapshot of all Port/IOC/VPPA/HCA_SVC/Pseudo nodes
 *	Snapshot includes IBNEX_NODE_INFO_NVL, IBNEX_NODE_TYPE_NVL,
 *	IBNEX_NODE_RSTATE_NVL, IBNEX_NODE_OSTATE_NVL and
 *	IBNEX_NODE_COND_NVL
 */
static int
ibnex_get_snapshot(char **buf, size_t *sz, int allow_probe)
{
	int			i, j, k, l, hca_count;
	nvlist_t		*nvl;
	ib_pkey_t 		pkey;
	boolean_t		found;
	ibdm_ioc_info_t		*ioc_listp;
	ibdm_ioc_info_t		*iocp;
	ibdm_hca_list_t		*hca_list, *hcap;
	ibdm_port_attr_t	*port_attr;
	ibnex_node_data_t	*nodep;

	ASSERT(mutex_owned(&ibnex.ibnex_mutex));

	*buf = NULL;
	*sz = 0;

	if (!ibnex.ibnex_pseudo_inited) {
		mutex_exit(&ibnex.ibnex_mutex);
		ibnex_pseudo_initnodes();
		mutex_enter(&ibnex.ibnex_mutex);
		ibnex.ibnex_pseudo_inited = 1;
	}

	/* First, Port/VPPA/HCA_SVC nodes */
	mutex_exit(&ibnex.ibnex_mutex);
	ibdm_ibnex_get_hca_list(&hca_list, &hca_count);
	mutex_enter(&ibnex.ibnex_mutex);

	(void) nvlist_alloc(&nvl, 0, KM_SLEEP);

	/* Go thru all the ports of all the HCAs and all the port-svc indices */
	for (hcap = hca_list, i = 0; i < hca_count;
	    hca_list = hca_list->hl_next, i++) {

		IBTF_DPRINTF_L4("ibnex", "ibnex_get_snapshot: "
		    "fill in  COMM service HCA_SVC nodes");
		port_attr = hca_list->hl_hca_port_attr;
		for (j = 0; j < ibnex.ibnex_nhcasvc_comm_svcs; j++) {
			if (ibnex_get_commsvcnode_snapshot(&nvl,
			    port_attr->pa_hca_guid,
			    port_attr->pa_hca_guid, j, (ib_pkey_t)0,
			    IBNEX_HCASVC_COMMSVC_NODE) != 0) {
				IBTF_DPRINTF_L2("ibnex",
				    "ibnex_get_snapshot: failed to fill"
				    " HCA_SVC device (%x %x)", i, j);
				ibdm_ibnex_free_hca_list(hcap);
				nvlist_free(nvl);
				return (-1);
			}

		}

		for (j = 0; j < hca_list->hl_nports; j++) {
			port_attr = &hca_list->hl_port_attr[j];

			IBTF_DPRINTF_L4("ibnex", "ibnex_get_snapshot: "
			    "fill in  COMM service Port nodes");
			for (k = 0; k < ibnex.ibnex_num_comm_svcs; k++) {

				if (ibnex_get_commsvcnode_snapshot(&nvl,
				    port_attr->pa_hca_guid,
				    port_attr->pa_port_guid, k, (ib_pkey_t)0,
				    IBNEX_PORT_COMMSVC_NODE) != 0) {
					IBTF_DPRINTF_L2("ibnex",
					    "ibnex_get_snapshot: failed to fill"
					    " Port device (%x %x %x)", i, j, k);
					ibdm_ibnex_free_hca_list(hcap);
					nvlist_free(nvl);
					return (-1);
				}

			} /* end of num_comm_svcs for loop */

			IBTF_DPRINTF_L4("ibnex", "ibnex_get_snapshot: "
			    "fill in  VPPA service port nodes");
			for (l = 0; l < port_attr->pa_npkeys; l++) {
				pkey = port_attr->pa_pkey_tbl[l].pt_pkey;
				if (IBNEX_INVALID_PKEY(pkey))
					continue;

				for (k = 0; k < ibnex.ibnex_nvppa_comm_svcs;
				    k++) {

					if (ibnex_get_commsvcnode_snapshot(&nvl,
					    port_attr->pa_hca_guid,
					    port_attr->pa_port_guid, k, pkey,
					    IBNEX_VPPA_COMMSVC_NODE) != 0) {
						IBTF_DPRINTF_L2("ibnex",
						    "ibnex_get_snapshot: "
						    "failed to fill VPPA "
						    "device (%x %x %x % x)",
						    i, j, k, l);
						ibdm_ibnex_free_hca_list(hcap);
						nvlist_free(nvl);
						return (-1);
					}
				} /* end of ibnex_nvppa_comm_svcs loop */

			} /* end of pa_npkeys for loop */

		} /* end of hl_nports for loop */

	} /* end of hca_count for loop */

	if (hcap)
		ibdm_ibnex_free_hca_list(hcap);

	/* save it to free up the entire list */
	mutex_exit(&ibnex.ibnex_mutex);
	iocp = ioc_listp = ibdm_ibnex_get_ioc_list(allow_probe);
	mutex_enter(&ibnex.ibnex_mutex);
	for (; ioc_listp != NULL; ioc_listp = ioc_listp->ioc_next) {

		/*
		 * Say we have N IOCs and all were deleted from ibnex
		 * but not from IBDM
		 */
		if (ibnex.ibnex_ioc_node_head == NULL) {
			if (ibnex_fill_ioc_tmp(&nvl, ioc_listp) != 0) {
				IBTF_DPRINTF_L2("ibnex", "ibnex_get_snapshot: "
				    "filling NVL data failed");
				ibdm_ibnex_free_ioc_list(iocp);
				nvlist_free(nvl);
				return (-1);
			}
			continue;

		} else {
			found = B_FALSE;

			/* Check first, if we have already seen this IOC? */
			for (nodep = ibnex.ibnex_ioc_node_head; nodep != NULL;
			    nodep = nodep->node_next) {
				if (ioc_listp->ioc_profile.ioc_guid ==
				    nodep->node_data.ioc_node.ioc_guid) {
					found = B_TRUE;
					break;
				}
			}


			/* have we seen this IOC before? */
			if (found == B_TRUE) {
				if (ibnex_fill_nodeinfo(&nvl, nodep,
				    &ioc_listp->ioc_profile) != 0) {
					IBTF_DPRINTF_L2("ibnex",
					    "ibnex_get_snapshot: filling NVL "
					    "for IOC node %p failed", nodep);
					ibdm_ibnex_free_ioc_list(iocp);
					nvlist_free(nvl);
					return (-1);
				}

			} else {

				if (ibnex_fill_ioc_tmp(&nvl, ioc_listp) != 0) {
					IBTF_DPRINTF_L2("ibnex",
					    "ibnex_get_snapshot: filling NVL "
					    "tmp for IOC node %p failed",
					    ioc_listp);
					ibdm_ibnex_free_ioc_list(iocp);
					nvlist_free(nvl);
					return (-1);
				}
			}

		} /* end of else ibnex_ioc_node_head == NULL */
	} /* end of external for */

	ibdm_ibnex_free_ioc_list(iocp);

	/*
	 * Add list of "disconnected" IOCs, not unconfigured.
	 */
	for (nodep = ibnex.ibnex_ioc_node_head; nodep != NULL;
	    nodep = nodep->node_next) {
		if (nodep->node_data.ioc_node.ioc_ngids == 0 &&
		    nodep->node_data.ioc_node.ioc_profile != NULL &&
		    nodep->node_state != IBNEX_CFGADM_UNCONFIGURED) {
			if (ibnex_fill_nodeinfo(&nvl, nodep,
			    nodep->node_data.ioc_node.ioc_profile) != 0) {
					IBTF_DPRINTF_L2("ibnex",
					    "ibnex_get_snapshot: filling NVL "
					    "for disconnected IOC node %p "
					    "failed", nodep);
					nvlist_free(nvl);
					return (-1);
			}
		}
	}

	/* lastly; pseudo nodes */
	for (nodep = ibnex.ibnex_pseudo_node_head; nodep;
	    nodep = nodep->node_next) {
		if (nodep->node_data.pseudo_node.pseudo_merge_node == 1)
			continue;
		if (ibnex_fill_nodeinfo(&nvl, nodep, NULL) != 0) {
			IBTF_DPRINTF_L2("ibnex", "ibnex_get_snapshot: "
			    "filling NVL data for Pseudo %p failed", nodep);
			nvlist_free(nvl);
			return (-1);
		}
	}

	/* pack the data into the buffer */
	if (nvlist_pack(nvl, buf, sz, NV_ENCODE_NATIVE, KM_SLEEP)) {
		IBTF_DPRINTF_L2("ibnex",
		    "ibnex_get_snapshot: nvlist_pack failed");
		nvlist_free(nvl);
		return (-1);
	}

	IBTF_DPRINTF_L4("ibnex", "ibnex_get_snapshot: size = 0x%x", *sz);
	nvlist_free(nvl);
	return (0);
}


/*
 * ibnex_get_commsvcnode_snapshot()
 *	A utility function to fill in a "dummy" Port/VPPA/HCA_SVC
 *	information. Cfgadm plugin will display all Port/VPPA/
 *	HCA_SVCs seen even if they are not all configured by IBNEX.
 *
 *	This function uses information from IBDM to fill up Port/VPPA/
 *	HCA_SVC snapshot. If none exists then it makes up a "temporary"
 *	node which will be displayed as "connected/unconfigured/unknown".
 *
 *	For HCA_SVC node port_guid will be same as hca_guid.
 */
static int
ibnex_get_commsvcnode_snapshot(nvlist_t **nvlpp, ib_guid_t hca_guid,
    ib_guid_t port_guid, int svc_index, ib_pkey_t p_key,
    ibnex_node_type_t node_type)
{
	int			rval;
	dev_info_t		*dip = NULL;
	ibnex_node_data_t	*nodep;
	ibnex_node_data_t	dummy;
	ibnex_node_data_t	*tmp = &dummy;

	IBTF_DPRINTF_L4("ibnex", "ibnex_get_commsvcnode_snapshot: "
	    "HCA GUID: %llX Port GUID: %llX svc_index = %x pkey = %x "
	    "node_type = %x", hca_guid, port_guid, svc_index, p_key, node_type);

	/* check if this node was seen before? */
	rval = ibnex_get_dip_from_guid(port_guid, svc_index, p_key, &dip);
	if (rval == IBNEX_SUCCESS && dip) {
		nodep = ddi_get_parent_data(dip);

		if (ibnex_fill_nodeinfo(nvlpp, nodep, NULL) != 0) {
			IBTF_DPRINTF_L2("ibnex",
			    "ibnex_get_commsvcnode_snapshot: failed to fill "
			    "Port/VPPA device node %p NVL data", nodep);
			return (-1);
		}

	} else {
		/* Fake up a Port/VPPA/HCA_SVC node */
		IBTF_DPRINTF_L4("ibnex", "ibnex_get_commsvcnode_snapshot: "
		    "VPPA/Port/HCA_SVC not seen by ibnex");
		bzero(tmp, sizeof (ibnex_node_data_t));
		tmp->node_type = node_type;
		tmp->node_data.port_node.port_guid = port_guid;
		tmp->node_data.port_node.port_hcaguid = hca_guid;
		tmp->node_data.port_node.port_commsvc_idx = svc_index;
		/* Fill P_Key only for VPPA nodes */
		if (node_type == IBNEX_VPPA_COMMSVC_NODE) {
			tmp->node_data.port_node.port_pkey = p_key;
		}

		if (ibnex_fill_nodeinfo(nvlpp, tmp, NULL) != 0) {
			IBTF_DPRINTF_L2("ibnex",
			    "ibnex_get_commsvcnode_snapshot: failed to fill "
			    "tmp Port/VPPA device node %p NVL data", tmp);
			return (-1);
		}
	}

	return (0);
}


/*
 * ibnex_fill_ioc_tmp()
 *	A utility function to fill in a "dummy" IOC information.
 *	Cfgadm plugin will display all IOCs seen by IBDM even if they
 *	are configured or not by IBNEX.
 *
 *	This function uses information from IBDM to fill up a
 *	dummy IOC information. It will be displayed as
 *	"connected/unconfigured/unknown".
 */
static int
ibnex_fill_ioc_tmp(nvlist_t **nvlpp, ibdm_ioc_info_t *ioc_listp)
{
	ibnex_node_data_t	dummy;
	ibnex_node_data_t	*nodep = &dummy;

	IBTF_DPRINTF_L4("ibnex", "\tibnex_fill_ioc_tmp:");

	bzero(nodep, sizeof (ibnex_node_data_t));
	nodep->node_type = IBNEX_IOC_NODE;
	nodep->node_data.ioc_node.ioc_guid = ioc_listp->ioc_profile.ioc_guid;
	nodep->node_data.ioc_node.iou_guid = ioc_listp->ioc_iou_guid;
	(void) strncpy(nodep->node_data.ioc_node.ioc_id_string,
	    (char *)ioc_listp->ioc_profile.ioc_id_string,
	    IB_DM_IOC_ID_STRING_LEN);
	IBTF_DPRINTF_L4("ibnex", "\tibnex_fill_ioc_tmp: %s",
	    nodep->node_data.ioc_node.ioc_id_string);

	if (ibnex_fill_nodeinfo(nvlpp, nodep, &ioc_listp->ioc_profile) != 0) {
		IBTF_DPRINTF_L2("ibnex", "\tibnex_fill_ioc_tmp: filling NVL "
		    "data for IOC node %p failed", nodep);
		return (-1);
	}

	return (0);
}


/*
 * ibnex_fill_nodeinfo()
 *	A utility function to fill in to the NVLIST information about
 *	a Port/IOC/VPPA/HCA_SVC/Pseudo driver that is then passed over
 *	to cfgadm utility for display. This information is used only
 *	for cfgadm -ll displays.
 *
 *	Information that is filled in here is:-
 *		AP_ID_NAME
 *		AP_ID_INFO
 *		AP_ID_TYPE
 *		AP_ID_OCCUPANT_STATE
 *		AP_ID_RECEPTACLE_STATE
 *		AP_ID_CONDITION
 */
static int
ibnex_fill_nodeinfo(nvlist_t **nvlpp, ibnex_node_data_t *node_datap, void *tmp)
{
	char			*svcname;
	char			*node_name;
	char			apid[IBTL_IBNEX_APID_LEN];
	char			info_data[MAXNAMELEN];
	ib_dm_ioc_ctrl_profile_t *profilep;
	devctl_ap_state_t	state;

	IBTF_DPRINTF_L5("ibnex", "ibnex_fill_nodeinfo: 0x%x addr is %p",
	    node_datap->node_type, node_datap);

	if (node_datap->node_type == IBNEX_PORT_COMMSVC_NODE) {
		svcname = ibnex.ibnex_comm_svc_names[node_datap->node_data.
		    port_node.port_commsvc_idx];
		(void) snprintf(apid, IBTL_IBNEX_APID_LEN, "%llX,0,%s",
		    (longlong_t)node_datap->node_data.port_node.port_guid,
		    svcname);

		/* Node APID */
		if (nvlist_add_string(*nvlpp, IBNEX_NODE_APID_NVL, apid)) {
			IBTF_DPRINTF_L2("ibnex", "ibnex_fill_nodeinfo: "
			    "failed to fill %s", IBNEX_NODE_APID_NVL);
			return (-1);
		}

		/* Node Info */
		if (nvlist_add_string(*nvlpp, IBNEX_NODE_INFO_NVL, svcname)) {
			IBTF_DPRINTF_L2("ibnex", "ibnex_fill_nodeinfo: "
			    "failed to fill Port %s", IBNEX_NODE_INFO_NVL);
			return (-1);
		}

		IBTF_DPRINTF_L5("ibnex", "ibnex_fill_nodeinfo: "
		    "Port %s = %s, %s = %s",
		    IBNEX_NODE_INFO_NVL, apid, IBNEX_NODE_APID_NVL, svcname);

	} else if (node_datap->node_type == IBNEX_VPPA_COMMSVC_NODE) {
		svcname = ibnex.ibnex_vppa_comm_svc_names[node_datap->node_data.
		    port_node.port_commsvc_idx];
		(void) snprintf(apid, IBTL_IBNEX_APID_LEN, "%llX,%x,%s",
		    (longlong_t)node_datap->node_data.port_node.port_guid,
		    node_datap->node_data.port_node.port_pkey, svcname);

		/* Node APID */
		if (nvlist_add_string(*nvlpp, IBNEX_NODE_APID_NVL, apid)) {
			IBTF_DPRINTF_L2("ibnex", "ibnex_fill_nodeinfo: "
			    "failed to fill %s", IBNEX_NODE_APID_NVL);
			return (-1);
		}

		/* Node Info */
		if (nvlist_add_string(*nvlpp, IBNEX_NODE_INFO_NVL, svcname)) {
			IBTF_DPRINTF_L2("ibnex", "ibnex_fill_nodeinfo: "
			    "failed to fill VPPA %s", IBNEX_NODE_INFO_NVL);
			return (-1);
		}

		IBTF_DPRINTF_L5("ibnex", "ibnex_fill_nodeinfo: "
		    "VPPA %s = %s, %s = %s",
		    IBNEX_NODE_APID_NVL, apid, IBNEX_NODE_INFO_NVL, svcname);

	} else if (node_datap->node_type == IBNEX_HCASVC_COMMSVC_NODE) {
		svcname = ibnex.ibnex_hcasvc_comm_svc_names[node_datap->
		    node_data.port_node.port_commsvc_idx];
		(void) snprintf(apid, IBTL_IBNEX_APID_LEN, "%llX,0,%s",
		    (longlong_t)node_datap->node_data.port_node.port_guid,
		    svcname);

		/* Node APID */
		if (nvlist_add_string(*nvlpp, IBNEX_NODE_APID_NVL, apid)) {
			IBTF_DPRINTF_L2("ibnex", "ibnex_fill_nodeinfo: "
			    "failed to fill %s", IBNEX_NODE_APID_NVL);
			return (-1);
		}

		/* Node Info */
		if (nvlist_add_string(*nvlpp, IBNEX_NODE_INFO_NVL, svcname)) {
			IBTF_DPRINTF_L2("ibnex", "ibnex_fill_nodeinfo: "
			    "failed to fill Port %s", IBNEX_NODE_INFO_NVL);
			return (-1);
		}

		IBTF_DPRINTF_L5("ibnex", "ibnex_fill_nodeinfo: "
		    "Port %s = %s, %s = %s",
		    IBNEX_NODE_INFO_NVL, apid, IBNEX_NODE_APID_NVL, svcname);

	} else if (node_datap->node_type == IBNEX_IOC_NODE) {

		/*
		 * get the IOC profile pointer from the args
		 */
		profilep = (ib_dm_ioc_ctrl_profile_t *)tmp;
		IBNEX_FORM_GUID(apid, IBTL_IBNEX_APID_LEN, profilep->ioc_guid);

		/* Node APID */
		if (nvlist_add_string(*nvlpp, IBNEX_NODE_APID_NVL, apid)) {
			IBTF_DPRINTF_L2("ibnex", "ibnex_fill_nodeinfo: "
			    "failed to fill in %s", IBNEX_NODE_APID_NVL);
			return (-1);
		}
		IBTF_DPRINTF_L5("ibnex", "ibnex_fill_nodeinfo: %s %s",
		    IBNEX_NODE_APID_NVL, apid);

		/*
		 * IOC "info" filed will display the following fields
		 * VendorID, IOCDeviceID, DeviceVersion, SubsystemVendorID,
		 * SubsystemID, Class, Subclass, Protocol, ProtocolVersion
		 */
		(void) snprintf(info_data, MAXNAMELEN,
		    "VID: 0x%x DEVID: 0x%x VER: 0x%x SUBSYS_VID: 0x%x "
		    "SUBSYS_ID: 0x%x CLASS: 0x%x SUBCLASS: 0x%x PROTO: 0x%x "
		    "PROTOVER: 0x%x ID_STRING: %s", profilep->ioc_vendorid,
		    profilep->ioc_deviceid, profilep->ioc_device_ver,
		    profilep->ioc_subsys_vendorid, profilep->ioc_subsys_id,
		    profilep->ioc_io_class, profilep->ioc_io_subclass,
		    profilep->ioc_protocol, profilep->ioc_protocol_ver,
		    (char *)profilep->ioc_id_string);
		IBTF_DPRINTF_L5("ibnex", "ibnex_fill_nodeinfo: %s", info_data);

		/* Node Info */
		if (nvlist_add_string(*nvlpp, IBNEX_NODE_INFO_NVL, info_data)) {
			IBTF_DPRINTF_L2("ibnex", "ibnex_fill_nodeinfo: "
			    "failed to fill IOC %s", IBNEX_NODE_INFO_NVL);
			return (-1);
		}

	} else if (node_datap->node_type == IBNEX_PSEUDO_NODE) {
		(void) snprintf(apid, IBTL_IBNEX_APID_LEN, "%s",
		    node_datap->node_data.pseudo_node.pseudo_node_addr);

		/* Node APID */
		if (nvlist_add_string(*nvlpp, IBNEX_NODE_APID_NVL, apid)) {
			IBTF_DPRINTF_L2("ibnex", "ibnex_fill_nodeinfo: "
			    "failed to fill in %s", IBNEX_NODE_APID_NVL);
			return (-1);
		}

		/* Node Info */
		node_name = node_datap->node_data.pseudo_node.pseudo_devi_name;
		(void) snprintf(info_data, MAXNAMELEN,
		    "Pseudo Driver = \"%s\", Unit-address = \"%s\"",
		    node_name, apid + strlen(node_name) + 1);
		if (nvlist_add_string(*nvlpp, IBNEX_NODE_INFO_NVL, info_data)) {
			IBTF_DPRINTF_L2("ibnex", "ibnex_fill_nodeinfo: "
			    "failed to fill Pseudo %s", IBNEX_NODE_INFO_NVL);
			return (-1);
		}

		IBTF_DPRINTF_L5("ibnex", "ibnex_fill_nodeinfo: Pseudo %s = %s,"
		    "%s = %s", IBNEX_NODE_APID_NVL, apid, IBNEX_NODE_INFO_NVL,
		    info_data);
	}

	/* Node type */
	if (nvlist_add_int32(*nvlpp, IBNEX_NODE_TYPE_NVL,
	    node_datap->node_type)) {
		IBTF_DPRINTF_L2("ibnex", "ibnex_fill_nodeinfo: "
		    "failed to fill in %s", IBNEX_NODE_TYPE_NVL);
		return (-1);
	}
	IBTF_DPRINTF_L5("ibnex", "ibnex_fill_nodeinfo: %s %d",
	    IBNEX_NODE_TYPE_NVL, node_datap->node_type);

	/* figure out "ostate", "rstate" and "condition" */
	ibnex_figure_ap_devstate(node_datap->node_dip, &state);

	if (nvlist_add_int32(*nvlpp, IBNEX_NODE_RSTATE_NVL, state.ap_rstate)) {
		IBTF_DPRINTF_L2("ibnex", "ibnex_fill_nodeinfo: "
		    "failed to fill in %s", IBNEX_NODE_RSTATE_NVL);
		return (-1);
	}
	IBTF_DPRINTF_L5("ibnex", "ibnex_fill_nodeinfo: %s %d",
	    IBNEX_NODE_RSTATE_NVL, state.ap_rstate);

	if (nvlist_add_int32(*nvlpp, IBNEX_NODE_OSTATE_NVL, state.ap_ostate)) {
		IBTF_DPRINTF_L2("ibnex", "ibnex_fill_nodeinfo: "
		    "failed to fill in %s", IBNEX_NODE_OSTATE_NVL);
		return (-1);
	}
	IBTF_DPRINTF_L5("ibnex", "ibnex_fill_nodeinfo: %s %d",
	    IBNEX_NODE_OSTATE_NVL, state.ap_ostate);

	if (nvlist_add_int32(*nvlpp, IBNEX_NODE_COND_NVL, state.ap_condition)) {
		IBTF_DPRINTF_L2("ibnex", "ibnex_fill_nodeinfo: "
		    "failed to fill in %s", IBNEX_NODE_COND_NVL);
		return (-1);
	}
	IBTF_DPRINTF_L5("ibnex", "ibnex_fill_nodeinfo: %s %d",
	    IBNEX_NODE_COND_NVL, state.ap_condition);

	return (0);
}


/*
 * ibnex_figure_ap_devstate()
 *	Fills the "devctl_ap_state_t" for a given ap_id
 *
 *	currently it assumes that we don't support "error_code" and
 *	"last_change" value.
 */
static void
ibnex_figure_ap_devstate(dev_info_t *dip, devctl_ap_state_t *ap_state)
{
	IBTF_DPRINTF_L5("ibnex", "ibnex_figure_ap_devstate: dip = %p", dip);

	ap_state->ap_rstate = AP_RSTATE_CONNECTED;
	if (dip == NULL) {	/* for nodes not seen by IBNEX yet */
		ap_state->ap_ostate = AP_OSTATE_UNCONFIGURED;
		ap_state->ap_condition = AP_COND_UNKNOWN;
	} else {
		if (i_ddi_node_state(dip) < DS_BOUND) {
			ap_state->ap_ostate = AP_OSTATE_UNCONFIGURED;
			ap_state->ap_condition = AP_COND_UNKNOWN;
		} else {
			ap_state->ap_ostate = AP_OSTATE_CONFIGURED;
			ap_state->ap_condition = AP_COND_OK;
		}
	}
	ap_state->ap_last_change = (time_t)-1;
	ap_state->ap_error_code = 0;
	ap_state->ap_in_transition = 0;
}


/*
 * ibnex_figure_ib_apid_devstate()
 *	Fills the "devctl_ap_state_t" for a IB static ap_id
 */
static void
ibnex_figure_ib_apid_devstate(devctl_ap_state_t *ap_state)
{
	ap_state->ap_rstate = AP_RSTATE_CONNECTED;
	ap_state->ap_condition = AP_COND_OK;
	ap_state->ap_ostate = (ibt_get_hca_list(NULL) == 0) ?
	    AP_OSTATE_UNCONFIGURED : AP_OSTATE_CONFIGURED;
	ap_state->ap_last_change = (time_t)-1;
	ap_state->ap_error_code = 0;
	ap_state->ap_in_transition = 0;
}


/*
 * ibnex_get_apid()
 *	Reads in the ap_id passed as an nvlist_string from user-land
 */
static char *
ibnex_get_apid(struct devctl_iocdata *dcp)
{
	char *ap_id;

	ASSERT(mutex_owned(&ibnex.ibnex_mutex));

	/* Get which ap_id to operate on.  */
	if (nvlist_lookup_string(ndi_dc_get_ap_data(dcp), "apid",
	    &ap_id) != 0) {
		IBTF_DPRINTF_L4("ibnex", "ibnex_get_apid: ap_id lookup failed");
		ap_id = NULL;
	}

	IBTF_DPRINTF_L4("ibnex", "ibnex_get_apid: ap_id=%s", ap_id);
	return (ap_id);
}


/*
 * ibnex_get_dip_from_apid()
 *	Figures out the dip/node_data from an ap_id given that this ap_id
 *	exists as a "name" in the "ibnex" list
 *
 * NOTE: ap_id was on stack earlier and gets manipulated here. Since this
 * function may be called twice; it is better to make a local copy of
 * ap_id; if the ap_id were to be reused.
 */
static int
ibnex_get_dip_from_apid(char *apid, dev_info_t **ret_dip,
    ibnex_node_data_t **ret_node_datap)
{
	int			rv, ret;
	int			index;
	int			len = strlen((char *)apid) + 1;
	char			*dyn;
	char			*ap_id;
	char			*first;
	char			*second = NULL;
	char			*node_addr;
	char			name[100];
	ibnex_node_data_t	*nodep = NULL;

	ap_id = i_ddi_strdup(apid, KM_SLEEP);
	IBTF_DPRINTF_L4("ibnex", "\tibnex_get_dip_from_apid: %s", ap_id);
	ASSERT(mutex_owned(&ibnex.ibnex_mutex));

	if ((dyn = GET_DYN(ap_id)) != NULL) {
		rv = IBNEX_DYN_APID;
	} else {	/* either static, hca or unknown */
		*ret_dip = NULL;
		if (strstr(ap_id, "hca") != 0) {
			rv = IBNEX_HCA_APID;
		} else if (strstr(ap_id, IBNEX_FABRIC) != 0) {
			rv = IBNEX_BASE_APID;
		} else {
			rv = IBNEX_UNKNOWN_APID;
		}
		kmem_free(ap_id, len);
		return (rv);
	}

	dyn += strlen(DYN_SEP);
	if (*dyn == '\0') {
		*ret_dip = NULL;
		kmem_free(ap_id, len);
		return (IBNEX_UNKNOWN_APID);
	}

	/* APID */
	first = strchr(dyn, ',');
	if (first != NULL)
		second = strchr(first+1, ',');

	/* Implies Port or VPPA or HCA_SVC Driver ap_id */
	if (first != NULL && second != NULL) {
		int	str_len;
		int	pkey_val = 0;
		char	*pkey_str = strchr(ap_id, ',');
		char	*svc_str = strrchr(pkey_str, ',');

		/* dyn contains ,GUID,p_key,svc_name. Change it to GUID */
		str_len = strlen(dyn) - strlen(pkey_str);
		dyn[str_len] = '\0';
		IBTF_DPRINTF_L4("ibnex", "\tibnex_get_dip_from_apid: "
		    "Port / Node Guid %s", dyn);

		/* figure out comm or vppa. figure out pkey  */
		++pkey_str; /* pkey_str used to point to ",p_key,svc_name" */

		/* pkey_str contains p_key,svc_name. Change it to p_key */
		str_len = strlen(pkey_str) - strlen(svc_str);
		pkey_str[str_len] = '\0';

		/* convert the string P_KEY to hex value */
		pkey_val = ibnex_str2hex(pkey_str, strlen(pkey_str), &ret);
		if (ret != IBNEX_SUCCESS) {
			*ret_dip = NULL;
			kmem_free(ap_id, len);
			return (IBNEX_UNKNOWN_APID);
		}

		++svc_str;	/* svc_str used to point to ",svc_name" */
		IBTF_DPRINTF_L5("ibnex", "\tibnex_get_dip_from_apid: pkey %s"
		    ":%x service name = %s", pkey_str, pkey_val, svc_str);

		for (nodep = ibnex.ibnex_port_node_head;
		    nodep != NULL; nodep = nodep->node_next) {
			index = nodep->node_data.port_node.port_commsvc_idx;
			IBNEX_FORM_GUID(name, IBTL_IBNEX_APID_LEN,
			    nodep->node_data.port_node.port_guid);

			/*
			 * Match P_Key, name string & service string:
			 * For COMM / HCA_SVC services these should be true:
			 *	P_Key matches to 0, svc_str in comm_svc_names[]
			 *	and name matches the dynamic part of the ap_id
			 * For VPPA services this should be true:
			 *	P_Key != 0 & matches, svc_str in
			 *	vppa_comm_svc_names[] and the name matches the
			 *	dynamic part of the ap_id.
			 */
			if ((pkey_val == nodep->node_data.port_node.
			    port_pkey) && (strstr(dyn, name) != NULL)) {

				/* pkey != 0, COMM / HCA_SVC service */
				if (((pkey_val == 0) && (
					/* Port Service */
				    ((ibnex.ibnex_comm_svc_names != NULL) &&
				    (index < ibnex.ibnex_num_comm_svcs) &&
				    (strstr(svc_str, ibnex.
				    ibnex_comm_svc_names[index]) != NULL)) ||
					/* HCA_SVC service */
				    ((ibnex.ibnex_hcasvc_comm_svc_names !=
				    NULL) && (index <
				    ibnex.ibnex_nhcasvc_comm_svcs) &&
				    (strstr(svc_str, ibnex.
				    ibnex_hcasvc_comm_svc_names[index])
					!= NULL)))) ||
					/* next the VPPA strings */
				    ((pkey_val != 0) && (strstr(svc_str, ibnex.
				    ibnex_vppa_comm_svc_names[index]) !=
				    NULL))) {
					if (nodep->node_dip)
						ndi_hold_devi(nodep->node_dip);
					*ret_node_datap = nodep;
					*ret_dip = nodep->node_dip;
					kmem_free(ap_id, len);
					return (rv);
				}
			}

		} /* end of for */

	} else if (first != NULL && second == NULL) {
		/* pseudo ap_id */
		for (nodep = ibnex.ibnex_pseudo_node_head; nodep;
		    nodep = nodep->node_next) {
			if (nodep->node_data.pseudo_node.pseudo_merge_node
			    == 1)
				continue;
			node_addr = nodep->node_data.pseudo_node.
			    pseudo_node_addr;
			if (strncmp(dyn, node_addr, strlen(node_addr)) == 0) {
				if (nodep->node_dip)
					ndi_hold_devi(nodep->node_dip);
				*ret_node_datap = nodep;
				*ret_dip = nodep->node_dip;
				kmem_free(ap_id, len);
				return (rv);
			}
		}

	} else if (first == NULL && second == NULL) {
		/* This is an IOC ap_id */
		for (nodep = ibnex.ibnex_ioc_node_head; nodep != NULL;
		    nodep = nodep->node_next) {
			IBNEX_FORM_GUID(name, IBTL_IBNEX_APID_LEN,
			    nodep->node_data.ioc_node.ioc_guid);
			if (strstr(dyn, name) != NULL) {
				if (nodep->node_dip)
					ndi_hold_devi(nodep->node_dip);
				*ret_node_datap = nodep;
				*ret_dip = nodep->node_dip;
				kmem_free(ap_id, len);
				return (rv);
			}
		}
	}

	/* Could not find a matching IB device */
	*ret_dip = (nodep) ? nodep->node_dip : NULL;
	kmem_free(ap_id, len);
	return (rv);
}


/*
 * ibnex_handle_pseudo_configure()
 *	Do DEVCTL_AP_CONNECT processing for Pseudo devices only.
 *	The code also checks if the given ap_id is valid or not.
 */
static ibnex_rval_t
ibnex_handle_pseudo_configure(char *apid)
{
	char			*node_addr;
	char			*last = strrchr(apid, ':') + 1;
	ibnex_rval_t		retval = IBNEX_FAILURE;
	ibnex_node_data_t	*nodep;

	IBTF_DPRINTF_L4("ibnex", "\tibnex_handle_pseudo_configure: "
	    "last = %s\n\t\tapid = %s", last, apid);
	ASSERT(MUTEX_HELD(&ibnex.ibnex_mutex));

	/* Check if the APID is valid first */
	if (apid == NULL || last == NULL) {
		IBTF_DPRINTF_L4("ibnex", "\tibnex_handle_pseudo_configure: "
		    "invalid apid %s", apid);
		return (retval);
	}

	/* find the matching entry and configure it */
	for (nodep = ibnex.ibnex_pseudo_node_head; nodep != NULL;
	    nodep = nodep->node_next) {
		if (nodep->node_data.pseudo_node.pseudo_merge_node == 1)
			continue;
		node_addr = nodep->node_data.pseudo_node.pseudo_node_addr;
		if (strncmp(node_addr, last, strlen(last)))
			continue;

		if (nodep->node_dip != NULL) {
			/*
			 * Return BUSY if another configure
			 * operation is in progress
			 */
			if (nodep->node_state ==
			    IBNEX_CFGADM_CONFIGURING)
				return (IBNEX_BUSY);
			else
				return (IBNEX_SUCCESS);
		}

		/*
		 * Return BUSY if another unconfigure operation is
		 * in progress
		 */
		if (nodep->node_state == IBNEX_CFGADM_UNCONFIGURING)
			return (IBNEX_BUSY);

		ASSERT(nodep->node_state != IBNEX_CFGADM_CONFIGURED);
		nodep->node_state = IBNEX_CFGADM_CONFIGURING;
		nodep->node_data.pseudo_node.pseudo_new_node = 0;

		mutex_exit(&ibnex.ibnex_mutex);
		retval = ibnex_pseudo_create_pi(nodep, NULL);
		mutex_enter(&ibnex.ibnex_mutex);
		if (retval == NDI_SUCCESS) {
			nodep->node_state = IBNEX_CFGADM_CONFIGURED;
			return (IBNEX_SUCCESS);
		} else {
			nodep->node_state = IBNEX_CFGADM_UNCONFIGURED;
			return (IBNEX_FAILURE);
		}
	}

	IBTF_DPRINTF_L4("ibnex", "\thandle_pseudo_configure: retval=%d",
	    retval);
	return (retval);
}


/*
 * ibnex_handle_ioc_configure()
 *	Do DEVCTL_AP_CONNECT processing for IOCs only.
 *	The code also checks if the given ap_id is valid or not.
 */
static ibnex_rval_t
ibnex_handle_ioc_configure(char *apid)
{
	int			ret;
	char			*guid_str = strrchr(apid, ':') + 1;
	ib_guid_t		ioc_guid;
	ibnex_rval_t		retval = IBNEX_FAILURE;
	ibdm_ioc_info_t		*ioc_info;

	ASSERT(MUTEX_HELD(&ibnex.ibnex_mutex));
	IBTF_DPRINTF_L4("ibnex", "\tibnex_handle_ioc_configure: %s", apid);

	/* Check if the APID is valid first */
	if (guid_str == NULL) {
		IBTF_DPRINTF_L4("ibnex",
		    "\tibnex_handle_ioc_configure: invalid apid %s", apid);
		return (retval);
	}

	/*
	 * Call into IBDM to get IOC information
	 */
	ioc_guid = ibnex_str2hex(guid_str, strlen(guid_str), &ret);
	if (ret != IBNEX_SUCCESS)
		return (ret);

	IBTF_DPRINTF_L4("ibnex",
	    "\tibnex_handle_ioc_configure: IOC GUID = %llX", ioc_guid);
	mutex_exit(&ibnex.ibnex_mutex);
	ioc_info = ibdm_ibnex_get_ioc_info(ioc_guid);
	mutex_enter(&ibnex.ibnex_mutex);
	if (ioc_info == NULL) {
		IBTF_DPRINTF_L2("ibnex",
		    "\tibnex_handle_ioc_configure: probe_iocguid failed");
		return (retval);
	}

	retval = ibnex_ioc_initnode(ioc_info, IBNEX_CFGADM_ENUMERATE, NULL);
	ibdm_ibnex_free_ioc_list(ioc_info);

	IBTF_DPRINTF_L4("ibnex", "\tibnex_handle_ioc_configure: "
	    "done retval = %d", retval);
	return (retval);
}


/*
 * ibnex_handle_commsvcnode_configure()
 *	Do DEVCTL_AP_CONNECT processing
 *	This is done for Port/VPPA/HCA_SVC drivers Only.
 *	The code also checks if the given ap_id is valid or not.
 */
static ibnex_rval_t
ibnex_handle_commsvcnode_configure(char *apid)
{
	int			ret, str_len;
	int			sndx;
	int			port_pkey = 0;
	char			*pkey_str = strchr(apid, ',');
	char			*guid_str = strrchr(apid, ':') + 1;
	char			*svc_str = strrchr(pkey_str, ',');
	boolean_t		found = B_FALSE;
	boolean_t		is_hcasvc_node = B_FALSE;
	ib_guid_t		guid;	/* Port / Node GUID */
	dev_info_t		*parent;
	ibnex_rval_t		retval = IBNEX_FAILURE;
	ibdm_port_attr_t	*port_attr;
	int			node_type;
	ibdm_hca_list_t		*hca_list;

	ASSERT(MUTEX_HELD(&ibnex.ibnex_mutex));
	IBTF_DPRINTF_L4("ibnex", "\tibnex_handle_commsvcnode_configure: %s",
	    apid);

	/* Check if the APID is valid first */
	if (guid_str == NULL || ((guid_str != NULL) &&
	    (pkey_str == NULL || svc_str == NULL))) {
		IBTF_DPRINTF_L4("ibnex",
		    "\tibnex_handle_commsvcnode_configure: "
		    "invalid apid %s", apid);
		return (retval);
	}

	/* guid_str contains GUID,p_key,svc_name. Change it to GUID */
	str_len = strlen(guid_str) - strlen(pkey_str);
	guid_str[str_len] = '\0';

	/* convert the string GUID to hex value */
	guid = ibnex_str2hex(guid_str, strlen(guid_str), &ret);
	if (ret == IBNEX_FAILURE)
		return (ret);
	IBTF_DPRINTF_L4("ibnex", "\tibnex_handle_commsvcnode_configure: "
	    "Port / Node Guid %llX", guid);

	/* figure out Port/HCA_SVC or VPPA. Also figure out the P_Key.  */
	++pkey_str;	/* pkey_str used to point to ",p_key,svc_name" */

	/* pkey_str contains p_key,svc_name. Change it to P_Key */
	str_len = strlen(pkey_str) - strlen(svc_str);
	pkey_str[str_len] = '\0';
	IBTF_DPRINTF_L5("ibnex", "\tibnex_handle_commsvcnode_configure: "
	    "p_key %s", pkey_str);

	/* convert the string P_Key to a hexadecimal value */
	port_pkey = ibnex_str2hex(pkey_str, strlen(pkey_str), &ret);
	IBTF_DPRINTF_L5("ibnex", "\tibnex_handle_commsvcnode_configure: "
	    "PKEY num %x", port_pkey);
	if (ret == IBNEX_FAILURE)
		return (ret);

	++svc_str;	/* svc_str used to point to ",svc_name" */

	/* find the service index */
	if (port_pkey == 0) {
		/* PORT Devices */
		for (sndx = 0; sndx < ibnex.ibnex_num_comm_svcs; sndx++) {
			if (strncmp(ibnex.ibnex_comm_svc_names[sndx],
			    svc_str, strlen(svc_str)) == 0) {
				found = B_TRUE;
				break;
			}
		}

		/* HCA_SVC Devices */
		if (found == B_FALSE) {
			for (sndx = 0; sndx < ibnex.ibnex_nhcasvc_comm_svcs;
			    sndx++) {
				if (strncmp(ibnex.ibnex_hcasvc_comm_svc_names
				    [sndx], svc_str, strlen(svc_str)) == 0) {
					found = B_TRUE;
					is_hcasvc_node = B_TRUE;
					break;
				}
			}
		}

	} else {
		for (sndx = 0; sndx < ibnex.ibnex_nvppa_comm_svcs; sndx++) {
			if (strncmp(ibnex.ibnex_vppa_comm_svc_names[sndx],
			    svc_str, strlen(svc_str)) == 0) {
				found = B_TRUE;
				break;
			}
		}
	}

	if (found == B_FALSE) {
		IBTF_DPRINTF_L2("ibnex",
		    "\tibnex_handle_commsvcnode_configure: "
		    "invalid service %s", svc_str);
		return (retval);
	}

	/* get Port attributes structure */
	mutex_exit(&ibnex.ibnex_mutex);
	if (is_hcasvc_node == B_FALSE) {
		port_attr = ibdm_ibnex_get_port_attrs(guid);
		if (port_attr == NULL) {
			IBTF_DPRINTF_L2("ibnex",
			    "\tibnex_handle_commsvcnode_configure: "
			    "ibdm_ibnex_get_port_attrs failed");
			mutex_enter(&ibnex.ibnex_mutex);
			return (retval);
		}
	} else {
		hca_list = ibdm_ibnex_get_hca_info_by_guid(guid);
		if (hca_list == NULL) {
			IBTF_DPRINTF_L2("ibnex",
			    "\tibnex_handle_commsvcnode_configure: "
			    "ibdm_ibnex_get_hca_info_by_guid failed");
			mutex_enter(&ibnex.ibnex_mutex);
			return (retval);
		}
		port_attr = hca_list->hl_hca_port_attr;
	}

	/* get HCA's dip */
	parent = ibtl_ibnex_hcaguid2dip(port_attr->pa_hca_guid);

	if (parent == NULL) {
		IBTF_DPRINTF_L2("ibnex",
		    "\tibnex_handle_commsvcnode_configure: "
		    "no HCA present");
		mutex_enter(&ibnex.ibnex_mutex);
		if (is_hcasvc_node == B_FALSE)
			ibdm_ibnex_free_port_attr(port_attr);
		else
			ibdm_ibnex_free_hca_list(hca_list);
		return (retval);
	}

	if (port_pkey == 0)
		node_type = (is_hcasvc_node == B_FALSE) ?
		    IBNEX_PORT_COMMSVC_NODE : IBNEX_HCASVC_COMMSVC_NODE;
	else
		node_type = IBNEX_VPPA_COMMSVC_NODE;

	mutex_enter(&ibnex.ibnex_mutex);
	if (ibnex_commsvc_initnode(parent, port_attr, sndx, node_type,
	    port_pkey, &ret, IBNEX_CFGADM_ENUMERATE) != NULL) {
		retval = IBNEX_SUCCESS;
	} else {
		retval = (ret == IBNEX_BUSY) ? IBNEX_BUSY : IBNEX_FAILURE;
	}

	if (is_hcasvc_node == B_FALSE)
		ibdm_ibnex_free_port_attr(port_attr);
	else
		ibdm_ibnex_free_hca_list(hca_list);

	IBTF_DPRINTF_L4("ibnex", "\tibnex_handle_commsvcnode_configure: "
	    "done retval = %d", retval);

	return (retval);
}


/*
 * ibnex_return_apid()
 *	Construct the ap_id of a given IBTF client in kernel
 */
static void
ibnex_return_apid(dev_info_t *childp, char **ret_apid)
{
	ibnex_node_data_t	*nodep;

	IBTF_DPRINTF_L4("ibnex", "ibnex_return_apid:");

	ASSERT(childp != NULL);
	nodep = ddi_get_parent_data(childp);

	if (nodep->node_type == IBNEX_PORT_COMMSVC_NODE) {
		(void) snprintf(*ret_apid, IBTL_IBNEX_APID_LEN,
		    "ib%s%llX,0,%s", DYN_SEP,
		    (longlong_t)nodep->node_data.port_node.port_guid,
		    ibnex.ibnex_comm_svc_names[nodep->node_data.port_node.
		    port_commsvc_idx]);

	} else if (nodep->node_type == IBNEX_HCASVC_COMMSVC_NODE) {
		(void) snprintf(*ret_apid, IBTL_IBNEX_APID_LEN,
		    "ib%s%llX,0,%s", DYN_SEP,
		    (longlong_t)nodep->node_data.port_node.port_guid, ibnex.
		    ibnex_hcasvc_comm_svc_names[nodep->node_data.port_node.
		    port_commsvc_idx]);

	} else if (nodep->node_type == IBNEX_VPPA_COMMSVC_NODE) {
		(void) snprintf(*ret_apid, IBTL_IBNEX_APID_LEN,
		    "ib%s%llX,%x,%s", DYN_SEP,
		    (longlong_t)nodep->node_data.port_node.port_guid,
		    nodep->node_data.port_node.port_pkey,
		    ibnex.ibnex_vppa_comm_svc_names[nodep->node_data.port_node.
		    port_commsvc_idx]);

	} else if (nodep->node_type == IBNEX_IOC_NODE) {
		(void) snprintf(*ret_apid, IBTL_IBNEX_APID_LEN,
		    "ib%s%llX", DYN_SEP,
		    (longlong_t)nodep->node_data.ioc_node.ioc_guid);

	} else if (nodep->node_type == IBNEX_PSEUDO_NODE) {
		(void) snprintf(*ret_apid, IBTL_IBNEX_APID_LEN, "ib%s%s",
		    DYN_SEP, nodep->node_data.pseudo_node.pseudo_node_addr);

	} else {
		(void) snprintf(*ret_apid, IBTL_IBNEX_APID_LEN, "%s", "-");
	}

	IBTF_DPRINTF_L4("ibnex", "ibnex_return_apid: %x %s",
	    nodep->node_type, ret_apid);
}


/*
 * ibnex_vppa_conf_entry_add()
 *	Add a new service to the ibnex data base of VPPA communication
 *	services.
 */
static void
ibnex_vppa_conf_entry_add(char *service)
{
	int	i, nsvcs;
	char	**service_name;

	ASSERT(MUTEX_HELD(&ibnex.ibnex_mutex));
	nsvcs = ibnex.ibnex_nvppa_comm_svcs;

	/* Allocate space for new "ibnex.ibnex_nvppa_comm_svcs + 1" */
	service_name = kmem_alloc((nsvcs + 1) * sizeof (char *), KM_SLEEP);
	/*
	 * Copy over the existing "ibnex.ibnex_vppa_comm_svc_names"
	 * array. Add the new service at the end.
	 */
	for (i = 0; i < nsvcs; i++)
		service_name[i] = ibnex.ibnex_vppa_comm_svc_names[i];
	service_name[i] = kmem_alloc(strlen(service) + 1, KM_SLEEP);
	(void) snprintf(service_name[i], 5, "%s", service);

	/* Replace existing pointer to VPPA services w/ newly allocated one */
	if (ibnex.ibnex_vppa_comm_svc_names) {
		kmem_free(ibnex.ibnex_vppa_comm_svc_names, nsvcs *
		    sizeof (char *));
	}
	ibnex.ibnex_nvppa_comm_svcs++;
	ibnex.ibnex_vppa_comm_svc_names = service_name;
}

/*
 * ibnex_port_conf_entry_add()
 *	Add a new service to the ibnex data base of Port communication
 *	services.
 */
static void
ibnex_port_conf_entry_add(char *service)
{
	int	i, nsvcs;
	char	**service_name;

	ASSERT(MUTEX_HELD(&ibnex.ibnex_mutex));
	nsvcs = ibnex.ibnex_num_comm_svcs;

	/* Allocate space for new "ibnex.ibnex_num_comm_svcs + 1" */
	service_name = kmem_alloc((nsvcs + 1) * sizeof (char *), KM_SLEEP);
	/*
	 * Copy over the existing "ibnex.ibnex_comm_svc_names" array.
	 * Add the new service to the end.
	 */
	for (i = 0; i < nsvcs; i++)
		service_name[i] = ibnex.ibnex_comm_svc_names[i];
	service_name[i] = kmem_alloc(strlen(service) + 1, KM_SLEEP);
	(void) snprintf(service_name[i], 5, "%s", service);

	/* Replace existing pointer to Port services w/ newly allocated one */
	if (ibnex.ibnex_comm_svc_names) {
		kmem_free(ibnex.ibnex_comm_svc_names, nsvcs * sizeof (char *));
	}
	ibnex.ibnex_num_comm_svcs++;
	ibnex.ibnex_comm_svc_names = service_name;
}

/*
 * ibnex_hcasvc_conf_entry_add()
 *	Add a new service to the ibnex data base of HCA_SVC communication
 *	services.
 */
static void
ibnex_hcasvc_conf_entry_add(char *service)
{
	int	i, nsvcs;
	char	**service_name;

	ASSERT(MUTEX_HELD(&ibnex.ibnex_mutex));
	nsvcs = ibnex.ibnex_nhcasvc_comm_svcs;

	/* Allocate space for new "ibnex.ibnex_nvppa_comm_svcs + 1" */
	service_name = kmem_alloc((nsvcs + 1) * sizeof (char *), KM_SLEEP);
	/*
	 * Copy over the existing "ibnex.ibnex_hcasvc_comm_svc_names"
	 * array. Add the new service at the end.
	 */
	for (i = 0; i < nsvcs; i++)
		service_name[i] = ibnex.ibnex_hcasvc_comm_svc_names[i];
	service_name[i] = kmem_alloc(strlen(service) + 1, KM_SLEEP);
	(void) snprintf(service_name[i], 5, "%s", service);

	/*
	 * Replace existing pointer to HCA_SVC services w/ newly
	 * allocated one
	 */
	if (ibnex.ibnex_hcasvc_comm_svc_names) {
		kmem_free(ibnex.ibnex_hcasvc_comm_svc_names, nsvcs *
		    sizeof (char *));
	}
	ibnex.ibnex_nhcasvc_comm_svcs++;
	ibnex.ibnex_hcasvc_comm_svc_names = service_name;
}


/*
 * ibnex_vppa_conf_entry_delete()
 *	Delete an existing service entry from ibnex data base of
 *	VPPA communication services.
 */
static int
ibnex_vppa_conf_entry_delete(char *msg, char *service)
{
	int			i, j, nsvcs;
	int			len;
	int			match_ndx;
	char			**service_name;
	boolean_t		found = B_FALSE;
	ibnex_node_data_t	*node_datap = ibnex.ibnex_port_node_head;

	IBTF_DPRINTF_L4("ibnex", "\tvppa_conf_entry_delete: %s", service);

	ASSERT(MUTEX_HELD(&ibnex.ibnex_mutex));
	nsvcs = ibnex.ibnex_nvppa_comm_svcs;

	/* find matching index */
	for (i = 0; i < nsvcs; i++) {
		if (strncmp(ibnex.ibnex_vppa_comm_svc_names[i], service,
		    strlen(service)))
			continue;
		found = B_TRUE;
		match_ndx = i;
		break;
	}

	/* check for valid "nsvcs" */
	if (found == B_FALSE || nsvcs == 0) {
		IBTF_DPRINTF_L2("ibnex", "%s: invalid vppa services %x",
		    msg, nsvcs);
		return (EIO);
	}

	/* Check if service is in use; return failure if so */
	for (; node_datap; node_datap = node_datap->node_next) {
		if ((node_datap->node_data.port_node.port_commsvc_idx == i) &&
		    node_datap->node_type == IBNEX_VPPA_COMMSVC_NODE &&
		    node_datap->node_dip) {
			IBTF_DPRINTF_L2("ibnex", "%s: service %s is in use",
			    msg, service);
			return (EIO);
		}
	}

	/* if nsvcs == 1, bailout early */
	if (nsvcs == 1) {
		/* free up that single entry */
		len = strlen(ibnex.ibnex_vppa_comm_svc_names[0]) + 1;
		kmem_free(ibnex.ibnex_vppa_comm_svc_names[0], len);
		kmem_free(ibnex.ibnex_vppa_comm_svc_names, sizeof (char *));
		ibnex.ibnex_vppa_comm_svc_names = NULL;
		ibnex.ibnex_nvppa_comm_svcs = 0;
		return (0);
	}

	/* Allocate space for new "ibnex.ibnex_nvppa_comm_svcs - 1" */
	service_name = kmem_alloc((nsvcs - 1) * sizeof (char *), KM_SLEEP);
	/*
	 * Copy over the existing "ibnex.ibnex_vppa_comm_svc_names"
	 * array. Do not copy over the matching service.
	 */
	for (i = 0, j = 0; i < nsvcs; i++) {
		if (i == match_ndx) {
			/* free up that entry */
			len = strlen(ibnex.ibnex_vppa_comm_svc_names[i]) + 1;
			kmem_free(ibnex.ibnex_vppa_comm_svc_names[i], len);
			continue;
		}
		service_name[j++] = ibnex.ibnex_vppa_comm_svc_names[i];
	}

	/* Replace existing pointer to VPPA services w/ newly adjusted one */
	if (ibnex.ibnex_vppa_comm_svc_names) {
		kmem_free(ibnex.ibnex_vppa_comm_svc_names, nsvcs *
		    sizeof (char *));
		ibnex.ibnex_nvppa_comm_svcs--;
		ibnex.ibnex_vppa_comm_svc_names = service_name;
	}
	return (0);
}


/*
 * ibnex_port_conf_entry_delete()
 *	Delete an existing service entry from ibnex data base of
 *	Port communication services.
 */
static int
ibnex_port_conf_entry_delete(char *msg, char *service)
{
	int			i, j, nsvcs;
	int			match_ndx;
	int			len;
	char			**service_name;
	boolean_t		found = B_FALSE;
	ibnex_node_data_t	*node_datap = ibnex.ibnex_port_node_head;

	IBTF_DPRINTF_L4("ibnex", "\tport_conf_entry_delete: %s", service);

	ASSERT(MUTEX_HELD(&ibnex.ibnex_mutex));
	nsvcs = ibnex.ibnex_num_comm_svcs;

	/* find matching index */
	for (i = 0; i < nsvcs; i++) {
		if (strncmp(ibnex.ibnex_comm_svc_names[i], service,
		    strlen(service)))
			continue;
		found = B_TRUE;
		match_ndx = i;
		break;
	}

	/* check for valid "nsvcs" */
	if (found == B_FALSE || nsvcs == 0) {
		IBTF_DPRINTF_L2("ibnex", "%s: invalid services %x", msg, nsvcs);
		return (EIO);
	}

	/* Check if service is in use; return failure if so */
	for (; node_datap; node_datap = node_datap->node_next) {
		if ((node_datap->node_data.port_node.port_commsvc_idx == i) &&
		    node_datap->node_type == IBNEX_PORT_COMMSVC_NODE &&
		    node_datap->node_dip)
			return (EIO);
	}

	/* if nsvcs == 1, bailout early */
	if (nsvcs == 1) {
		/* free up that single entry */
		len = strlen(ibnex.ibnex_comm_svc_names[0]) + 1;
		kmem_free(ibnex.ibnex_comm_svc_names[0], len);
		kmem_free(ibnex.ibnex_comm_svc_names, sizeof (char *));
		ibnex.ibnex_comm_svc_names = NULL;
		ibnex.ibnex_num_comm_svcs = 0;
		return (0);
	}

	/* Allocate space for new "ibnex.ibnex_num_comm_svcs - 1" */
	service_name = kmem_alloc((nsvcs - 1) * sizeof (char *), KM_SLEEP);
	/*
	 * Copy over the existing "ibnex.ibnex_comm_svc_names" array.
	 * Skip the matching service.
	 */
	for (i = 0, j = 0; i < nsvcs; i++) {
		if (i == match_ndx) {
			/* free up that entry */
			len = strlen(ibnex.ibnex_comm_svc_names[i]) + 1;
			kmem_free(ibnex.ibnex_comm_svc_names[i], len);
			continue;
		}
		service_name[j++] = ibnex.ibnex_comm_svc_names[i];
	}

	/* Replace existing pointer to Port services w/ newly adjusted one */
	if (ibnex.ibnex_comm_svc_names) {
		kmem_free(ibnex.ibnex_comm_svc_names, nsvcs * sizeof (char *));
		ibnex.ibnex_num_comm_svcs--;
		ibnex.ibnex_comm_svc_names = service_name;
	}
	return (0);
}

/*
 * ibnex_hcasvc_conf_entry_delete()
 *	Delete an existing service entry from ibnex data base of
 *	HCA_SVC communication services.
 */
static int
ibnex_hcasvc_conf_entry_delete(char *msg, char *service)
{
	int			i, j, nsvcs;
	int			len;
	int			match_ndx;
	char			**service_name;
	boolean_t		found = B_FALSE;
	ibnex_node_data_t	*node_datap = ibnex.ibnex_port_node_head;

	IBTF_DPRINTF_L4("ibnex", "\thcasvc_conf_entry_delete: %s", service);

	ASSERT(MUTEX_HELD(&ibnex.ibnex_mutex));
	nsvcs = ibnex.ibnex_nhcasvc_comm_svcs;

	/* find matching index */
	for (i = 0; i < nsvcs; i++) {
		if (strncmp(ibnex.ibnex_hcasvc_comm_svc_names[i], service,
		    strlen(service)))
			continue;
		found = B_TRUE;
		match_ndx = i;
		break;
	}

	/* check for valid "nsvcs" */
	if (found == B_FALSE || nsvcs == 0) {
		IBTF_DPRINTF_L2("ibnex", "%s: invalid hca_svc services %x",
		    msg, nsvcs);
		return (EIO);
	}

	/* Check if service is in use; return failure if so */
	for (; node_datap; node_datap = node_datap->node_next) {
		if ((node_datap->node_data.port_node.port_commsvc_idx == i) &&
		    node_datap->node_type == IBNEX_HCASVC_COMMSVC_NODE &&
		    node_datap->node_dip) {
			IBTF_DPRINTF_L2("ibnex", "%s: service %s is in use",
			    msg, service);
			return (EIO);
		}
	}

	/* if nsvcs == 1, bailout early */
	if (nsvcs == 1) {
		/* free up that single entry */
		len = strlen(ibnex.ibnex_hcasvc_comm_svc_names[0]) + 1;
		kmem_free(ibnex.ibnex_hcasvc_comm_svc_names[0], len);
		kmem_free(ibnex.ibnex_hcasvc_comm_svc_names, sizeof (char *));
		ibnex.ibnex_hcasvc_comm_svc_names = NULL;
		ibnex.ibnex_nhcasvc_comm_svcs = 0;
		return (0);
	}

	/* Allocate space for new "ibnex.ibnex_nhcasvc_comm_svcs - 1" */
	service_name = kmem_alloc((nsvcs - 1) * sizeof (char *), KM_SLEEP);
	/*
	 * Copy over the existing "ibnex.ibnex_hcasvc_comm_svc_names"
	 * array. Do not copy over the matching service.
	 */
	for (i = 0, j = 0; i < nsvcs; i++) {
		if (i == match_ndx) {
			/* free up that entry */
			len = strlen(ibnex.ibnex_hcasvc_comm_svc_names[i]) + 1;
			kmem_free(ibnex.ibnex_hcasvc_comm_svc_names[i], len);
			continue;
		}
		service_name[j++] = ibnex.ibnex_hcasvc_comm_svc_names[i];
	}

	/* Replace existing pointer to VPPA services w/ newly adjusted one */
	if (ibnex.ibnex_hcasvc_comm_svc_names) {
		kmem_free(ibnex.ibnex_hcasvc_comm_svc_names, nsvcs *
		    sizeof (char *));
		ibnex.ibnex_nhcasvc_comm_svcs--;
		ibnex.ibnex_hcasvc_comm_svc_names = service_name;
	}
	return (0);
}


/*
 * ibnex_ioc_fininode()
 *	Un-initialize a child device node for IOC device node
 *	Returns IBNEX_SUCCESS/IBNEX_FAILURE
 */
static ibnex_rval_t
ibnex_ioc_fininode(dev_info_t *dip, ibnex_ioc_node_t *ioc_nodep)
{
	int	rval = MDI_SUCCESS;

	ASSERT(MUTEX_HELD(&ibnex.ibnex_mutex));
	IBTF_DPRINTF_L4("ibnex", "\tioc_fininode");

	/*
	 * For a dis-connected IOC,
	 *	Free the ioc_profile &&
	 *	decrement ibnex_num_disconnect_iocs
	 */
	if (ioc_nodep->ioc_ngids == 0 && ioc_nodep->ioc_profile) {
		IBTF_DPRINTF_L4("ibnex", "\tioc_fininode: unconfigure "
		    "disconnected IOC: GUID %lX", ioc_nodep->ioc_guid);
		ibnex.ibnex_num_disconnect_iocs--;
		kmem_free(ioc_nodep->ioc_profile,
		    sizeof (ib_dm_ioc_ctrl_profile_t));
		ioc_nodep->ioc_profile = NULL;
	}

	mutex_exit(&ibnex.ibnex_mutex);
	ASSERT(i_ddi_node_state(dip) >= DS_BOUND);

	IBTF_DPRINTF_L4("ibnex", "\tioc_fininode: offlining the IOC");
	rval = ibnex_offline_childdip(dip);

	if (rval != MDI_SUCCESS) {
		rval = NDI_FAILURE;
		IBTF_DPRINTF_L2("ibnex", "\toffline failed for IOC "
		    "dip %p with 0x%x", dip, rval);
	}

	mutex_enter(&ibnex.ibnex_mutex);
	return (rval == MDI_SUCCESS ? IBNEX_SUCCESS : IBNEX_OFFLINE_FAILED);
}


int
ibnex_offline_childdip(dev_info_t *dip)
{
	int		rval = MDI_SUCCESS;
	mdi_pathinfo_t	*path = NULL, *temp;

	IBTF_DPRINTF_L4("ibnex", "\toffline_childdip; begin");
	if (dip == NULL) {
		IBTF_DPRINTF_L2("ibnex", "\toffline_childdip; NULL dip");
		return (MDI_FAILURE);
	}

	for (path = mdi_get_next_phci_path(dip, path); path; ) {
		IBTF_DPRINTF_L4("ibnex", "\toffline_childdip: "
		    "offling path %p", path);
		rval = mdi_pi_offline(path, NDI_UNCONFIG);
		if (rval != MDI_SUCCESS) {
			IBTF_DPRINTF_L2("ibnex", "\toffline_childdip: "
			    "mdi_pi_offline failed %p", dip);
			break;
		}
		temp = path;
		path = mdi_get_next_phci_path(dip, path);
		(void) mdi_pi_free(temp, 0);
	}
	return (rval);
}


/*
 * ibnex_commsvc_fininode()
 *
 * Un-initialize a child device node for HCA port / node GUID
 * for a communication service.
 *	Returns IBNEX_SUCCESS/IBNEX_FAILURE
 */
static ibnex_rval_t
ibnex_commsvc_fininode(dev_info_t *dip)
{
	int	rval = NDI_SUCCESS;

	ASSERT(MUTEX_HELD(&ibnex.ibnex_mutex));
	IBTF_DPRINTF_L4("ibnex", "\tcommsvc_fininode");

	mutex_exit(&ibnex.ibnex_mutex);
	if (i_ddi_node_state(dip) < DS_BOUND) {
		/*
		 * if the child hasn't been bound yet, we can
		 * just free the dip. This path is currently
		 * untested.
		 */
		(void) ddi_remove_child(dip, 0);
		IBTF_DPRINTF_L4("ibnex",
		    "\tcommsvc_fininode: ddi_remove_child");
	} else {
		IBTF_DPRINTF_L4("ibnex", "\tcommsvc_fininode: offlining the "
		    "Commsvc node");

		rval = ndi_devi_offline(dip, NDI_DEVI_REMOVE | NDI_UNCONFIG);
		if (rval != NDI_SUCCESS)
			IBTF_DPRINTF_L2("ibnex", "\toffline failed for Commsvc "
			    "dip %p with 0x%x", dip, rval);
	}
	mutex_enter(&ibnex.ibnex_mutex);
	return (rval == NDI_SUCCESS ? IBNEX_SUCCESS : IBNEX_OFFLINE_FAILED);
}


/*
 * ibnex_pseudo_fininode()
 *	Un-initialize a child pseudo device node
 *	Returns IBNEX_SUCCESS/IBNEX_FAILURE
 */
static ibnex_rval_t
ibnex_pseudo_fininode(dev_info_t *dip)
{
	int	rval = MDI_SUCCESS;

	ASSERT(MUTEX_HELD(&ibnex.ibnex_mutex));
	IBTF_DPRINTF_L4("ibnex", "\tpseudo_fininode: dip = %p", dip);

	mutex_exit(&ibnex.ibnex_mutex);
	ASSERT(i_ddi_node_state(dip) >= DS_BOUND);

	IBTF_DPRINTF_L4("ibnex", "\tpseudo_fininode: offlining the "
	    "pseudo device");
	rval = ibnex_offline_childdip(dip);
	if (rval != MDI_SUCCESS) {
		rval = NDI_FAILURE;
		IBTF_DPRINTF_L2("ibnex", "\tpseudo offline failed for "
		    "dip %p with 0x%x", dip, rval);
	}

	mutex_enter(&ibnex.ibnex_mutex);
	return (rval == MDI_SUCCESS ? IBNEX_SUCCESS : IBNEX_OFFLINE_FAILED);
}
