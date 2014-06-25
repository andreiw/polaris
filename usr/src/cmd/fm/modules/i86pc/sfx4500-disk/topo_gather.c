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

#pragma ident	"@(#)topo_gather.c	1.3	06/06/14 SMI"

/*
 * Gathers properties exported by libtopo and uses them to construct diskmon
 * data structures, which hold the configuration information for the
 * DE.
 */

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <pthread.h>
#include <libnvpair.h>
#include <config_admin.h>
#include <sys/fm/protocol.h>
#include <fm/libtopo.h>

#include "sata.h"
#include "sfx4500_props.h"
#include "sfx4500-disk.h"
#include "topo_gather.h"

#define	TOPO_PGROUP_IO		"io"	/* duplicated from did_props.h */
#define	MAX_CONF_MSG_LEN	256

static nvlist_t *g_topo2diskmon = NULL;

/*
 * The following function template is required for nvlists that were
 * create with no flags (so there can be multiple identical name or name-value
 * pairs).  The function defined below returns the first match for the name
 * provided.
 */
#define	NONUNIQUE_NVLIST_FN(suffix, type, atype) \
static int								\
nonunique_nvlist_lookup_##suffix(nvlist_t *nvlp, const char *n, atype *rpp) \
{									\
	nvpair_t *nvp = NULL;						\
	while ((nvp = nvlist_next_nvpair(nvlp, nvp)) != NULL) {		\
		if (nvpair_type(nvp) != type)				\
			continue;					\
		if (strcmp(nvpair_name(nvp), n) == 0)			\
			return (nvpair_value_##suffix(nvp, rpp));	\
	}								\
	return (ENOENT);						\
}

NONUNIQUE_NVLIST_FN(string, DATA_TYPE_STRING, char *)

static diskmon_t *
dm_fmristring_to_diskmon(char *str)
{
	diskmon_t *p = NULL;
	uint64_t u64val;
	char ch;
	char *lastsl = strrchr(str, '/');

	ch = *lastsl;
	*lastsl = 0;

	if (nvlist_lookup_uint64(g_topo2diskmon, str, &u64val) == 0) {

		p = (diskmon_t *)(uintptr_t)u64val;
	}

	*lastsl = ch;

	return (p);
}

diskmon_t *
dm_fmri_to_diskmon(fmd_hdl_t *hdl, nvlist_t *fmri)
{
	topo_hdl_t *thdl;
	nvlist_t *dupfmri;
	diskmon_t *diskp;
	char *buf;
	int err;

	if (nvlist_dup(fmri, &dupfmri, 0) != 0)
		return (NULL);

	(void) nvlist_remove(dupfmri, FM_FMRI_HC_REVISION, DATA_TYPE_STRING);
	(void) nvlist_remove(dupfmri, FM_FMRI_HC_SERIAL_ID, DATA_TYPE_STRING);
	(void) nvlist_remove(dupfmri, FM_FMRI_HC_PART, DATA_TYPE_STRING);

	thdl = fmd_hdl_topology(hdl, TOPO_VERSION);
	if (thdl == NULL || topo_fmri_nvl2str(thdl, dupfmri, &buf, &err) != 0) {
		nvlist_free(dupfmri);
		return (NULL);
	}

	diskp = dm_fmristring_to_diskmon(buf);

	nvlist_free(dupfmri);
	topo_hdl_strfree(thdl, buf);

	return (diskp);
}

static nvlist_t *
find_sfx4500_private_pgroup(topo_hdl_t *thp, tnode_t *node)
{
	nvlist_t *list_of_lists, *nvlp, *dupnvlp;
	nvlist_t *sfx4500_pgrp = NULL;
	nvpair_t *nvp = NULL;
	char *pgroup_name;

	/*
	 * topo_prop_get_all() returns an nvlist that contains other
	 * nvlists (some of which are property groups).  Since the private
	 * property group we need will be among the list of property
	 * groups returned (hopefully), we need to walk the list of nvlists
	 * in the topo node's properties to find the property groups, then
	 * check inside each embedded nvlist to see if it's the pgroup we're
	 * looking for.
	 */
	if ((list_of_lists = topo_prop_get_all(thp, node)) != NULL) {
		/*
		 * Go through the list of nvlists, looking for the
		 * property group we need.
		 */
		while ((nvp = nvlist_next_nvpair(list_of_lists, nvp)) != NULL) {

			if (nvpair_type(nvp) != DATA_TYPE_NVLIST ||
			    strcmp(nvpair_name(nvp), TOPO_PROP_GROUP) != 0 ||
			    nvpair_value_nvlist(nvp, &nvlp) != 0)
				continue;

			dm_assert(nvlp != NULL);
			pgroup_name = NULL;

			if (nonunique_nvlist_lookup_string(nvlp,
			    TOPO_PROP_GROUP_NAME, &pgroup_name) != 0 ||
			    strcmp(pgroup_name, SUN_FIRE_X4500_PROPERTIES) != 0)
				continue;
			else {
				/*
				 * Duplicate the nvlist so that when the
				 * master nvlist is freed (below), we will
				 * still refer to allocated memory.
				 */
				if (nvlist_dup(nvlp, &dupnvlp, 0) == 0)
					sfx4500_pgrp = dupnvlp;
				else
					sfx4500_pgrp = NULL;
				break;
			}
		}

		nvlist_free(list_of_lists);
	}

	return (sfx4500_pgrp);
}

/*
 * Look up the FMRI corresponding to the node in the global
 * hash table and return the pointer stored (if any).  Save the
 * FMRI string in *str if str is non-NULL.
 */
static void *
fmri2ptr(topo_hdl_t *thp, tnode_t *node, char **str, int *err)
{
	nvlist_t	*fmri = NULL;
	char		*cstr = NULL;
	uint64_t	u64val;
	void		*p = NULL;

	if (topo_node_resource(node, &fmri, err) != 0)
		return (NULL);

	if (topo_fmri_nvl2str(thp, fmri, &cstr, err) != 0) {
		nvlist_free(fmri);
		return (NULL);
	}

	if (nvlist_lookup_uint64(g_topo2diskmon, cstr, &u64val) == 0) {

		p = (void *)(uintptr_t)u64val;
	}

	nvlist_free(fmri);
	if (str != NULL)
		*str = dstrdup(cstr);
	topo_hdl_strfree(thp, cstr);
	return (p);
}

static void
transform_model_string(char *manuf, char *model, char **finalstring,
    int *finalstringbuflen)
{
	int buflen = 0;
	int i = 0;
	int j = 0;
	char *buf;
	char *finalmodelstring;

	if (manuf != NULL)
		buflen += strlen(manuf);
	if (model != NULL)
		buflen += strlen(model);
	buflen += 2;

	buf = dmalloc(buflen);
	finalmodelstring = dmalloc(buflen);
	if (manuf != NULL && model != NULL)
		(void) snprintf(buf, buflen, "%s-%s", manuf, model);
	else
		(void) snprintf(buf, buflen, "%s",
		    manuf == NULL ? model : manuf);

	do {
		if (isspace(buf[i]))
			finalmodelstring[j++] = '-';
		else
			finalmodelstring[j++] = buf[i];

		while (buf[i] != 0 && isspace(buf[i]))
			i++;
	} while (buf[i++] != 0);

	finalmodelstring[j] = 0;

	dfree(buf, buflen);
	*finalstring = finalmodelstring;
	*finalstringbuflen = buflen;
}

static int
topo_add_disk(topo_hdl_t *thp, tnode_t *node, diskmon_t *target_diskp)
{
	nvlist_t	*fmri = NULL;
	nvlist_t	*asru_fmri;
	nvlist_t	*fru_fmri;
	char		*devpath = NULL;
	char		*capacity = NULL;
	char		*firmrev = NULL;
	char		*serial = NULL;
	char		*manuf = NULL;
	char		*model = NULL;
	char		*cstr = NULL;
	char		*buf;
	char		*label;
	char		*p;
	uint64_t	ptr = 0;
	int		buflen;
	int		err;
	int		orig_cstr_len;
	dm_fru_t	*frup;
	diskmon_t	*diskp;

	/*
	 * Match this node to a disk in the configuration by looking at
	 * our parent's fmri (and do that by getting our FMRI and chopping
	 * off the last part).
	 */
	if (topo_node_resource(node, &fmri, &err) != 0) {
		log_msg(MM_TOPO, "topo_add_disk: Could not generate FMRI for "
		    "node %p!\n", (void *)node);
		return (-1);
	}

	if (topo_fmri_nvl2str(thp, fmri, &cstr, &err) != 0) {
		log_msg(MM_TOPO, "topo_add_disk: Could not create string for "
		    "node %p's FMRI!\n", (void *)node);
		nvlist_free(fmri);
		return (-1);
	}

	nvlist_free(fmri);

	/*
	 * Chop off all but last path (since there's no way to get
	 * the node's parent in the libtopo API).
	 */
	orig_cstr_len = strlen(cstr) + 1;
	p = strrchr(cstr, '/');
	dm_assert(p != NULL);
	*p = 0;
	if (nvlist_lookup_uint64(g_topo2diskmon, cstr, &ptr) != 0) {
		log_msg(MM_TOPO, "No diskmon for parent of node %p.\n", node);
		topo_hdl_free(thp, cstr, orig_cstr_len);
		/* Skip this disk: */
		return (0);
	}

	topo_hdl_free(thp, cstr, orig_cstr_len);

	diskp = (diskmon_t *)(uintptr_t)ptr;

	/* If we were called upon to update a particular disk, do it */
	if (target_diskp != NULL && diskp != target_diskp) {
		return (0);
	}

	/*
	 * Update the diskmon's ASRU and FRU with our information (this
	 * information is cached in the diskmon so we don't have to do a
	 * time-consuming topo traversal when we get an ereport).
	 */
	if (topo_prop_get_fmri(node, TOPO_PGROUP_PROTOCOL, TOPO_PROP_ASRU,
	    &asru_fmri, &err) == 0) {
		diskmon_add_asru(diskp, asru_fmri);
		nvlist_free(asru_fmri);
	}
	if (topo_prop_get_fmri(node, TOPO_PGROUP_PROTOCOL, TOPO_PROP_FRU,
	    &fru_fmri, &err) == 0) {
		diskmon_add_fru(diskp, fru_fmri);
		nvlist_free(fru_fmri);
	}
	if (topo_node_resource(node, &fmri, &err) == 0) {
		diskmon_add_disk_fmri(diskp, fmri);
		nvlist_free(fmri);
	}

	/*
	 * Update the diskmon's location field with the disk's label
	 */
	if (diskp->location)
		dstrfree(diskp->location);
	if (topo_node_label(node, &label, &err) == 0) {
		diskp->location = dstrdup(label);
		topo_hdl_strfree(thp, label);
	} else
		diskp->location = dstrdup("unknown location");

	/*
	 * Check for a device path property (if the disk is configured,
	 * it will be present) and add it to the diskmon's properties)
	 */
	if (topo_prop_get_string(node, TOPO_PGROUP_IO, TOPO_IO_DEV_PATH,
	    &devpath, &err) == 0) {
		char devp[PATH_MAX];
		/*
		 * Consumers of the DISK_PROP_DEVPATH property expect a raw
		 * minor device node
		 */
		(void) snprintf(devp, PATH_MAX, "%s:q,raw", devpath);
		(void) nvlist_add_string(diskp->props, DISK_PROP_DEVPATH,
		    devp);
		topo_hdl_strfree(thp, devpath);
	}

	/*
	 * Add the logical disk node, if it exists
	 */
	if (topo_prop_get_string(node, TOPO_STORAGE_PGROUP,
	    TOPO_STORAGE_LOGICAL_DISK_NAME, &devpath, &err) == 0) {
		(void) nvlist_add_string(diskp->props, DISK_PROP_LOGNAME,
		    devpath);
		topo_hdl_strfree(thp, devpath);
	}

	/*
	 * Add the FRU information (if present in the node) to the diskmon's
	 * fru data structure:
	 */
	(void) topo_prop_get_string(node, TOPO_STORAGE_PGROUP,
	    TOPO_STORAGE_MODEL, &model, &err);

	(void) topo_prop_get_string(node, TOPO_STORAGE_PGROUP,
	    TOPO_STORAGE_MANUFACTURER, &manuf, &err);

	(void) topo_prop_get_string(node, TOPO_STORAGE_PGROUP,
	    TOPO_STORAGE_SERIAL_NUM, &serial, &err);

	(void) topo_prop_get_string(node, TOPO_STORAGE_PGROUP,
	    TOPO_STORAGE_FIRMWARE_REV, &firmrev, &err);

	(void) topo_prop_get_string(node, TOPO_STORAGE_PGROUP,
	    TOPO_STORAGE_CAPACITY, &capacity, &err);

	frup = new_dmfru(manuf, model, firmrev, serial,
	    capacity == NULL ? 0 : strtoull(capacity, 0, 0));

	/*
	 * Update the disk's resource FMRI with the
	 * SunService-required members:
	 * FM_FMRI_HC_SERIAL_ID, FM_FMRI_HC_PART, and
	 * FM_FMRI_HC_REVISION
	 */
	(void) nvlist_add_string(diskp->disk_res_fmri,
	    FM_FMRI_HC_SERIAL_ID, serial);

	transform_model_string(manuf, model, &buf, &buflen);

	(void) nvlist_add_string(diskp->disk_res_fmri,
	    FM_FMRI_HC_PART, buf);

	/*
	 * Add the serial number to the ASRU so that when the resource
	 * is marked faulty in the fmd resource cache, the hc scheme
	 * plugin can detect when the disk is no longer installed (and so,
	 * can clear the faulty state automatically across fmd restarts).
	 *
	 * The serial number is only updated when a disk comes online
	 * because that's when the disk node exists in the topo tree.
	 * It's ok to keep a stale value in the ASRU when the disk is removed
	 * because it's only used as part of fault creation when the disk
	 * is configured (online), at which point it will be updated with
	 * the (current) serial number of the disk inserted.
	 */
	(void) nvlist_add_string(diskp->asru_fmri,
	    FM_FMRI_HC_SERIAL_ID, serial);

	dfree(buf, buflen);

	(void) nvlist_add_string(diskp->disk_res_fmri,
	    FM_FMRI_HC_REVISION, firmrev);

	if (model) {
		topo_hdl_strfree(thp, model);
	}

	if (manuf) {
		topo_hdl_strfree(thp, manuf);
	}

	if (serial) {
		topo_hdl_strfree(thp, serial);
	}

	if (firmrev) {
		topo_hdl_strfree(thp, firmrev);
	}

	if (capacity) {
		topo_hdl_strfree(thp, capacity);
	}

	/* Add the fru information to the diskmon: */
	dm_assert(pthread_mutex_lock(&diskp->fru_mutex) == 0);
	dm_assert(diskp->frup == NULL);
	diskp->frup = frup;
	dm_assert(pthread_mutex_unlock(&diskp->fru_mutex) == 0);

	return (0);
}

static int
indicator_breakup(char *identifier, ind_state_t *state, char **name)
{
	if (identifier[0] != '+' && identifier[0] != '-') {
		log_msg(MM_CONF, "Invalid indicator name `%s'\n", identifier);
		return (-1);
	}

	*state = (identifier[0] == '+') ? INDICATOR_ON : INDICATOR_OFF;
	*name = &identifier[1];
	return (0);
}

static int
topoprop_indicator_add(indicator_t **indp, char *ind_name, char *ind_action)
{
	/* The Indicator name is of the form: "[+-][A-Za-z][A-Za-z0-9]+" */
	indicator_t *newindp;
	ind_state_t state;
	char *name;

	if (indicator_breakup(ind_name, &state, &name) != 0)
		return (-1);
	newindp = new_indicator(state, name, ind_action);

	link_indicator(indp, newindp);

	return (0);
}

static hotplug_state_t
str2dmstate(char *str)
{
	if (strcasecmp("configured", str) == 0) {
		return (HPS_CONFIGURED);
	} else if (strcasecmp("unconfigured", str) == 0) {
		return (HPS_UNCONFIGURED);
	} else if (strcasecmp("absent", str) == 0) {
		return (HPS_ABSENT);
	} else if (strcasecmp("present", str) == 0) {
		return (HPS_PRESENT);
	} else
		return (HPS_UNKNOWN);
}

static int
topoprop_indrule_add(indrule_t **indrp, char *sts, char *acts)
{
	ind_action_t		*indactp = NULL;
	ind_state_t		state;
	char			*name, *lasts, *p;
	int			stateslen = strlen(sts) + 1;
	int			actionslen = strlen(acts) + 1;
	char			*states = dstrdup(sts);
	char			*actions = dstrdup(acts);
	state_transition_t	strans;
	boolean_t		failed = B_FALSE;
	conf_err_t		err;
	char			msgbuf[MAX_CONF_MSG_LEN];

	/* The state string is of the form "{STATE}>{STATE}" */
	p = strchr(states, '>');
	dm_assert(p != NULL);
	*p = 0;
	strans.begin = str2dmstate(states);
	*p = '>';
	strans.end = str2dmstate(p + 1);

	if (strans.begin == HPS_UNKNOWN || strans.end == HPS_UNKNOWN) {
		log_msg(MM_CONF, "Invalid states property `%s'\n", sts);
		failed = B_TRUE;
	} else if ((err = check_state_transition(strans.begin, strans.end))
	    != E_NO_ERROR) {
		conf_error_msg(err, msgbuf, MAX_CONF_MSG_LEN, &strans);
		log_msg(MM_CONF, "%s: Not adding disk to list!\n", msgbuf);
		failed = B_TRUE;
	}

	/* Actions are of the form "{ACTION}[&{ACTION}]" */
	if (!failed && (p = strtok_r(actions, "&", &lasts)) != NULL) {
		/* At least 2 tokens */
		do {
			if (indicator_breakup(p, &state, &name) != 0) {
				failed = B_TRUE;
				break;
			}

			link_indaction(&indactp, new_indaction(state, name));

		} while ((p = strtok_r(NULL, "&", &lasts)) != NULL);
	} else if (!failed) {
		/* One token */
		if (indicator_breakup(actions, &state, &name) != 0)
			return (-1);
		indactp = new_indaction(state, name);
	}

	dfree(states, stateslen);
	dfree(actions, actionslen);

	if (!failed && (err = check_indactions(indactp)) != E_NO_ERROR) {
		conf_error_msg(err, msgbuf, MAX_CONF_MSG_LEN, NULL);
		log_msg(MM_CONF, "%s: Not adding disk to list!\n", msgbuf);
		failed = B_TRUE;
	}

	if (failed) {
		indaction_free(indactp);
		return (-1);
	} else
		link_indrule(indrp, new_indrule(&strans, indactp));
	return (0);
}


static int
topo_add_sata_port(topo_hdl_t *thp, tnode_t *node, diskmon_t *target_diskp)
{
	nvlist_t	*nvlp = find_sfx4500_private_pgroup(thp, node);
	nvlist_t	*prop_nvlp;
	nvpair_t	*nvp = NULL;
	char		*prop_name, *prop_value;
#define	PNAME_MAX 128
	char		pname[PNAME_MAX];
	char		msgbuf[MAX_CONF_MSG_LEN];
	char		*indicator_name, *indicator_action;
	char		*indrule_states, *indrule_actions;
	int		err = 0, i;
	conf_err_t	conferr;
	boolean_t	conf_failure = B_FALSE;
	char		*physid = NULL;
	char		*label;
	nvlist_t	*diskprops = NULL;
	char		*cstr = NULL;
	indicator_t	*indp = NULL;
	indrule_t	*indrp = NULL;
	void		*p;
	diskmon_t	*diskp;
	void		*ptr;

	/* No private properties -- just ignore the port */
	if (nvlp == NULL)
		return (0);

	/*
	 * Look for a diskmon based on this node's FMRI string.
	 * Once a diskmon has been created, it's not re-created.  This is
	 * essential for the times when the tree-walk is called after a
	 * disk is inserted (or removed) -- in that case, the disk node
	 * handler simply updates the FRU information in the diskmon.
	 */
	if ((p = fmri2ptr(thp, node, &cstr, &err)) != NULL) {

		diskp = (diskmon_t *)p;

		/*
		 * Delete the FRU information from the diskmon.  If a disk
		 * is connected, its FRU information will be refreshed by
		 * the disk node code.
		 */
		if (diskp->frup && (target_diskp == NULL ||
		    diskp == target_diskp)) {
			dm_assert(pthread_mutex_lock(&diskp->fru_mutex) == 0);
			dmfru_free(diskp->frup);
			diskp->frup = NULL;
			dm_assert(pthread_mutex_unlock(&diskp->fru_mutex) == 0);
		}

		dstrfree(cstr);
		nvlist_free(nvlp);
		return (0);
	}

	/*
	 * Determine the physical path to the attachment point
	 */
	if (topo_prop_get_string(node, TOPO_PGROUP_IO,
	    TOPO_IO_AP_PATH, &physid, &err) != 0) {

		/* physid cannot have been allocated */
		if (cstr)
			dstrfree(cstr);
		nvlist_free(nvlp);
		return (-1);
	}

	/*
	 * Process the properties.  If we encounter a property that
	 * is not an indicator name, action, or rule, add it to the
	 * disk's props list.
	 */

	/* Process indicators */
	i = 0;
	indicator_name = NULL;
	indicator_action = NULL;
	do {
		if (indicator_name != NULL && indicator_action != NULL) {

			if (topoprop_indicator_add(&indp, indicator_name,
			    indicator_action) != 0) {

				conf_failure = B_TRUE;
			}

			topo_hdl_strfree(thp, indicator_name);
			topo_hdl_strfree(thp, indicator_action);
		}

		(void) snprintf(pname, PNAME_MAX, SATA_IND_NAME "-%d", i);
		if (topo_prop_get_string(node, SUN_FIRE_X4500_PROPERTIES,
		    pname, &indicator_name, &err) != 0)
			break;

		(void) snprintf(pname, PNAME_MAX, SATA_IND_ACTION "-%d", i);
		if (topo_prop_get_string(node, SUN_FIRE_X4500_PROPERTIES,
		    pname, &indicator_action, &err) != 0)
			break;

		i++;
	} while (!conf_failure && indicator_name != NULL &&
	    indicator_action != NULL);

	if (!conf_failure && indp != NULL &&
	    (conferr = check_inds(indp)) != E_NO_ERROR) {
		conf_error_msg(conferr, msgbuf, MAX_CONF_MSG_LEN, NULL);
		log_msg(MM_CONF, "%s: Not adding disk to list\n", msgbuf);
		conf_failure = B_TRUE;
	}

	/* Process state rules and indicator actions */
	i = 0;
	indrule_states = NULL;
	indrule_actions = NULL;
	do {
		if (indrule_states != NULL && indrule_actions != NULL) {

			if (topoprop_indrule_add(&indrp, indrule_states,
			    indrule_actions) != 0) {

				conf_failure = B_TRUE;
			}

			topo_hdl_strfree(thp, indrule_states);
			topo_hdl_strfree(thp, indrule_actions);
		}

		(void) snprintf(pname, PNAME_MAX, SATA_INDRULE_STATES "-%d", i);
		if (topo_prop_get_string(node, SUN_FIRE_X4500_PROPERTIES,
		    pname, &indrule_states, &err) != 0)
			break;

		(void) snprintf(pname, PNAME_MAX, SATA_INDRULE_ACTIONS "-%d",
		    i);
		if (topo_prop_get_string(node, SUN_FIRE_X4500_PROPERTIES,
		    pname, &indrule_actions, &err) != 0)
			break;

		i++;
	} while (!conf_failure && indrule_states != NULL &&
	    indrule_actions != NULL);

	if (!conf_failure && indrp != NULL && indp != NULL &&
	    ((conferr = check_indrules(indrp, (state_transition_t **)&ptr))
	    != E_NO_ERROR ||
	    (conferr = check_consistent_ind_indrules(indp, indrp,
	    (ind_action_t **)&ptr)) != E_NO_ERROR)) {

		conf_error_msg(conferr, msgbuf, MAX_CONF_MSG_LEN, ptr);
		log_msg(MM_CONF, "%s: Not adding disk to list\n", msgbuf);
		conf_failure = B_TRUE;

	}

	/*
	 * Now collect miscellaneous properties.
	 * Each property is stored as an embedded nvlist named
	 * TOPO_PROP_VAL.  The property name is stored in the value for
	 * key=TOPO_PROP_VAL_NAME and the property's value is
	 * stored in the value for key=TOPO_PROP_VAL_VAL.  This is all
	 * necessary so we can subtractively decode the properties that
	 * we do not directly handle (so that these properties are added to
	 * the per-disk properties nvlist), increasing flexibility.
	 */
	(void) nvlist_alloc(&diskprops, NV_UNIQUE_NAME, 0);
	while ((nvp = nvlist_next_nvpair(nvlp, nvp)) != NULL) {
		/* Only care about embedded nvlists named TOPO_PROP_VAL */
		if (nvpair_type(nvp) != DATA_TYPE_NVLIST ||
		    strcmp(nvpair_name(nvp), TOPO_PROP_VAL) != 0 ||
		    nvpair_value_nvlist(nvp, &prop_nvlp) != 0)
			continue;

		if (nonunique_nvlist_lookup_string(prop_nvlp,
		    TOPO_PROP_VAL_NAME, &prop_name) != 0)
			continue;

		/* Filter out indicator properties */
		if (strstr(prop_name, SATA_IND_NAME) != NULL ||
		    strstr(prop_name, SATA_IND_ACTION) != NULL ||
		    strstr(prop_name, SATA_INDRULE_STATES) != NULL ||
		    strstr(prop_name, SATA_INDRULE_ACTIONS) != NULL)
			continue;

		if (nonunique_nvlist_lookup_string(prop_nvlp, TOPO_PROP_VAL_VAL,
		    &prop_value) != 0)
			continue;

		/* Add the property to the disk's prop list: */
		if (nvlist_add_string(diskprops, prop_name, prop_value) != 0)
			log_msg(MM_TOPO,
			    "Could not add disk property `%s' with "
			    "value `%s'\n", prop_name, prop_value);
	}

	nvlist_free(nvlp);

	if (cstr != NULL) {
		namevalpr_t nvpr;
		nvlist_t *dmap_nvl;

		nvpr.name = DISK_AP_PROP_APID;
		nvpr.value = strncmp(physid, "/devices", 8) == 0 ?
		    (physid + 8) : physid;

		/*
		 * Set the diskmon's location to the value in this port's label.
		 * If there's a disk plugged in, the location will be updated
		 * to be the disk label (e.g. HD_ID_00).  Until a disk is
		 * inserted, though, there won't be a disk libtopo node
		 * created.
		 */

		/* Pass physid without the leading "/devices": */
		dmap_nvl = namevalpr_to_nvlist(&nvpr);

		diskp = new_diskmon(dmap_nvl, indp, indrp, diskprops);

		if (topo_node_label(node, &label, &err) == 0) {
			diskp->location = dstrdup(label);
			topo_hdl_strfree(thp, label);
		} else
			diskp->location = dstrdup("unknown location");

		if (!conf_failure && diskp != NULL) {
			/* Add this diskmon to the disk list */
			cfgdata_add_diskmon(config_data, diskp);
			if (nvlist_add_uint64(g_topo2diskmon, cstr,
			    (uint64_t)(uintptr_t)diskp) != 0) {
				log_msg(MM_TOPO,
				    "Could not add pointer to nvlist "
				    "for `%s'!\n", cstr);
			}
		} else if (diskp != NULL) {
			diskmon_free(diskp);
		} else {
			if (dmap_nvl)
				nvlist_free(dmap_nvl);
			if (indp)
				ind_free(indp);
			if (indrp)
				indrule_free(indrp);
			if (diskprops)
				nvlist_free(diskprops);
		}

		dstrfree(cstr);
	}


	topo_hdl_strfree(thp, physid);
	return (0);
}

/*ARGSUSED*/
static int
gather_topo_cfg(topo_hdl_t *thp, tnode_t *node, void *arg)
{
	char *nodename = topo_node_name(node);
	if (strcmp(SATA_DISK, nodename) == 0)
		return (topo_add_disk(thp, node, (diskmon_t *)arg)
		    ? TOPO_WALK_ERR : TOPO_WALK_NEXT);
	else if (strcmp(SATA_PORT, nodename) == 0)
		return (topo_add_sata_port(thp, node, (diskmon_t *)arg)
		    ? TOPO_WALK_ERR : TOPO_WALK_NEXT);

	return (TOPO_WALK_NEXT);
}


int
update_configuration_from_topo(diskmon_t *diskp)
{
	int err;
	topo_hdl_t *thp;
	topo_walk_t *twp;
	char *uuid;

	if ((thp = topo_open(TOPO_VERSION, NULL, &err)) == NULL) {

		return (TOPO_OPEN_ERROR);
	}

	if ((uuid = topo_snap_hold(thp, NULL, &err)) == NULL) {

		topo_close(thp);
		return (TOPO_SNAP_ERROR);
	}


	if ((twp = topo_walk_init(thp, FM_FMRI_SCHEME_HC, gather_topo_cfg,
	    diskp, &err)) == NULL) {

		topo_snap_release(thp);
		topo_hdl_strfree(thp, uuid);
		topo_close(thp);

		return (err ? TOPO_WALK_INIT_ERROR : TOPO_SUCCESS);
	}

	topo_hdl_strfree(thp, uuid);

	if (topo_walk_step(twp, TOPO_WALK_CHILD) == TOPO_WALK_ERR) {

		topo_walk_fini(twp);
		topo_snap_release(thp);
		topo_close(thp);

		return (TOPO_WALK_ERROR);
	}

	topo_walk_fini(twp);
	topo_snap_release(thp);
	topo_close(thp);

	return (TOPO_SUCCESS);
}

int
init_configuration_from_topo(void)
{
	return (nvlist_alloc(&g_topo2diskmon, NV_UNIQUE_NAME, 0));
}

void
fini_configuration_from_topo(void)
{
	if (g_topo2diskmon) {
		nvlist_free(g_topo2diskmon);
	}
}
