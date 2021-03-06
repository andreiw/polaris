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

#pragma ident	"@(#)sgen_link.c	1.4	05/06/08 SMI"

#include <devfsadm.h>
#include <stdio.h>
#include <strings.h>
#include <stdlib.h>
#include <limits.h>
#include <ctype.h>

#define	SGEN_LINK_RE	"^scsi/.+/c[0-9]+t[0-9A-F]+d[0-9]+$"
#define	SGEN_DIR	"scsi"
#define	SGEN_CLASS	"generic-scsi"

static int sgen_callback(di_minor_t minor, di_node_t node);
static char *find_ctrlr(di_node_t node, di_minor_t minor);


static devfsadm_create_t sgen_create_cbt[] = {
	{ SGEN_CLASS, "ddi_generic:scsi", NULL,
	    TYPE_EXACT | CREATE_DEFER, ILEVEL_0, sgen_callback
	}
};

DEVFSADM_CREATE_INIT_V0(sgen_create_cbt);

/*
 * HOT auto cleanup of sgen links not desired.
 */
static devfsadm_remove_t sgen_remove_cbt[] = {
	{ SGEN_CLASS, SGEN_LINK_RE, RM_POST,
		ILEVEL_0, devfsadm_rm_all
	}
};

DEVFSADM_REMOVE_INIT_V0(sgen_remove_cbt);

static int
sgen_callback(di_minor_t minor, di_node_t node)
{
	char *baddr, *cnum, *tstr;
	char lpath[PATH_MAX], buf[PATH_MAX];
	uchar_t *wwn;

	if ((cnum = find_ctrlr(node, minor)) == NULL)
		goto done;

	if (di_prop_lookup_strings(DDI_DEV_T_ANY, node,
	    "client-guid", (char **)&wwn) > 0) {
		/*
		 * MPXIO-enabled devices; lun is always 0.
		 */
		if (strlcpy((char *)buf, (char *)wwn, sizeof (buf)) >=
		    sizeof (buf))
			goto done;

		for (tstr = buf; *tstr != '\0'; tstr++) {
			*tstr = toupper(*tstr);
		}
		if (snprintf(lpath, sizeof (lpath), "%s/%s/c%st%sd0", SGEN_DIR,
		    di_minor_name(minor), cnum, buf) >= sizeof (lpath))
			goto done;

	} else if (di_prop_lookup_bytes(DDI_DEV_T_ANY, node,
	    "port-wwn", &wwn) == 8) {
		/*
		 * "normal" fibre channel devices
		 */
		int lun, *lunp, count;
		if (di_prop_lookup_ints(DDI_DEV_T_ANY, node, "lun", &lunp) > 0)
			lun = *lunp;
		else
			lun = 0;

		for (count = 0, tstr = buf; count < 8; count++, tstr += 2)
			(void) sprintf(tstr, "%02X", wwn[count]);

		*tstr = '\0';
		if (snprintf(lpath, sizeof (lpath), "%s/%s/c%st%sd%d", SGEN_DIR,
		    di_minor_name(minor), cnum, buf, lun) >= sizeof (lpath))
			goto done;
	} else {
		/*
		 * Parallel SCSI devices
		 */
		uint_t targ, lun;

		if ((baddr = di_bus_addr(node)) == NULL)
			goto done;

		if (sscanf(baddr, "%X,%X", &targ, &lun) != 2)
			goto done;

		if (snprintf(lpath, sizeof (lpath), "%s/%s/c%st%dd%d", SGEN_DIR,
		    di_minor_name(minor), cnum, targ, lun) >= sizeof (lpath))
			goto done;
	}

	(void) devfsadm_mklink(lpath, node, minor, 0);
done:
	free(cnum);
	return (DEVFSADM_CONTINUE);
}

/* index of enumeration rule applicable to this module */
#define	RULE_INDEX	2

static char *
find_ctrlr(di_node_t node, di_minor_t minor)
{
	char path[PATH_MAX + 1];
	char *devfspath;
	char *buf, *mn;

	devfsadm_enumerate_t rules[3] = {
	    {"^r?dsk$/^c([0-9]+)", 1, MATCH_PARENT},
	    {"^cfg$/^c([0-9]+)$", 1, MATCH_ADDR},
	    {"^scsi$/^.+$/^c([0-9]+)", 1, MATCH_PARENT}
	};

	mn = di_minor_name(minor);

	if ((devfspath = di_devfs_path(node)) == NULL) {
		return (NULL);
	}
	(void) strcpy(path, devfspath);
	(void) strcat(path, ":");
	(void) strcat(path, mn);
	di_devfs_path_free(devfspath);

	/*
	 * Use controller (parent) component of device path
	 */
	if (disk_enumerate_int(path, RULE_INDEX, &buf, rules, 3) ==
	    DEVFSADM_MULTIPLE) {

		/*
		 * We failed because there are multiple logical controller
		 * numbers for a single physical controller.  If we use node
		 * name also for DEVICE paths in the match it should fix this
		 * and only find one logical controller. (See 4045879).
		 * NOTE: Rules for controllers are not changed, as there is
		 * no unique controller number for them in this case.
		 *
		 * MATCH_UNCACHED flag is private to the "disks" and "sgen"
		 * modules. NOT to be used by other modules.
		 */
		rules[0].flags = MATCH_NODE | MATCH_UNCACHED; /* disks */
		rules[2].flags = MATCH_NODE | MATCH_UNCACHED; /* generic scsi */
		if (devfsadm_enumerate_int(path, RULE_INDEX, &buf, rules, 3)) {
			return (NULL);
		}
	}

	return (buf);
}
