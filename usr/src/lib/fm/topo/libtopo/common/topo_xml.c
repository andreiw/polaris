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

#pragma ident	"@(#)topo_xml.c	1.2	06/02/11 SMI"

#include <libxml/parser.h>
#include <libxml/xinclude.h>
#include <sys/fm/protocol.h>
#include <assert.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <fm/libtopo.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <topo_mod.h>
#include <topo_subr.h>
#include <topo_alloc.h>
#include <topo_parse.h>
#include <topo_error.h>

const char * const Children = "children";
const char * const Dependents = "dependents";
const char * const FMRI = "fmri";
const char * const Grouping = "grouping";
const char * const Immutable = "immutable";
const char * const Instance = "instance";
const char * const Int32 = "int32";
const char * const Int64 = "int64";
const char * const Name = "name";
const char * const Path = "path";
const char * const Range = "range";
const char * const Scheme = "scheme";
const char * const Siblings = "siblings";
const char * const String = "string";
const char * const Topology = "topology";
const char * const Type = "type";
const char * const UInt32 = "uint32";
const char * const UInt64 = "uint64";
const char * const Value = "value";
const char * const Verify = "verify";
const char * const Version = "version";

const char * const Enum_meth = "enum-method";
const char * const Propgrp = "propgroup";
const char * const Propval = "propval";

const char * const Node = "node";
const char * const Hc = "hc";

const char * const True = "true";
const char * const False = "false";

const char * const Namestab = "name-stability";
const char * const Datastab = "data-stability";

const char * const Evolving = "Evolving";
const char * const External = "External";
const char * const Internal = "Internal";
const char * const Obsolete = "Obsolete";
const char * const Private = "Private";
const char * const Stable = "Stable";
const char * const Standard = "Standard";
const char * const Unstable = "Unstable";

static tf_rdata_t *topo_xml_walk(topo_mod_t *,
    tf_info_t *, xmlNodePtr, tnode_t *);

static void
txml_dump(int g, xmlNodePtr p)
{
	if (p && p->name) {
		topo_dprintf(TOPO_DBG_MOD, "%d %s\n", g, p->name);

		for (p = p->xmlChildrenNode; p != NULL; p = p->next)
			txml_dump(g + 1, p);
	}
}

int
xmlattr_to_stab(topo_mod_t *mp, xmlNodePtr n, topo_stability_t *rs)
{
	xmlChar *str;
	int rv = 0;

	if (n == NULL) {
		/* If there is no Stability defined, we default to private */
		*rs = TOPO_STABILITY_PRIVATE;
		return (0);
	}
	if ((str = xmlGetProp(n, (xmlChar *)Value)) == NULL)
		return (topo_mod_seterrno(mp, ETOPO_PRSR_NOATTR));
	if (xmlStrcmp(str, (xmlChar *)Internal) == 0) {
		*rs = TOPO_STABILITY_INTERNAL;
	} else if (xmlStrcmp(str, (xmlChar *)Private) == 0) {
		*rs = TOPO_STABILITY_PRIVATE;
	} else if (xmlStrcmp(str, (xmlChar *)Obsolete) == 0) {
		*rs = TOPO_STABILITY_OBSOLETE;
	} else if (xmlStrcmp(str, (xmlChar *)External) == 0) {
		*rs = TOPO_STABILITY_EXTERNAL;
	} else if (xmlStrcmp(str, (xmlChar *)Unstable) == 0) {
		*rs = TOPO_STABILITY_UNSTABLE;
	} else if (xmlStrcmp(str, (xmlChar *)Evolving) == 0) {
		*rs = TOPO_STABILITY_EVOLVING;
	} else if (xmlStrcmp(str, (xmlChar *)Stable) == 0) {
		*rs = TOPO_STABILITY_STABLE;
	} else if (xmlStrcmp(str, (xmlChar *)Standard) == 0) {
		*rs = TOPO_STABILITY_STANDARD;
	} else {
		xmlFree(str);
		return (topo_mod_seterrno(mp, ETOPO_PRSR_BADSTAB));
	}
	xmlFree(str);
	return (rv);
}

int
xmlattr_to_int(topo_mod_t *mp,
    xmlNodePtr n, const char *propname, uint64_t *value)
{
	xmlChar *str;
	xmlChar *estr;

	topo_mod_dprintf(mp, "attribute to int\n");
	if ((str = xmlGetProp(n, (xmlChar *)propname)) == NULL)
		return (topo_mod_seterrno(mp, ETOPO_PRSR_NOATTR));
	*value = strtoull((char *)str, (char **)&estr, 10);
	if (estr == str) {
		/* no conversion was done */
		xmlFree(str);
		return (topo_mod_seterrno(mp, ETOPO_PRSR_BADNUM));
	}
	xmlFree(str);
	return (0);
}

static int
xmlattr_to_fmri(topo_mod_t *mp,
    xmlNodePtr xn, const char *propname, nvlist_t **rnvl)
{
	int err;
	xmlChar *str;

	topo_mod_dprintf(mp, "attribute to int\n");
	if ((str = xmlGetProp(xn, (xmlChar *)propname)) == NULL)
		return (topo_mod_seterrno(mp, ETOPO_PRSR_NOATTR));
	if (topo_fmri_str2nvl(topo_mod_handle(mp), (const char *)str, rnvl,
	    &err) < 0)
		return (-1);
	xmlFree(str);
	return (0);
}

static topo_type_t
xmlattr_to_type(topo_mod_t *mp, xmlNodePtr xn)
{
	topo_type_t rv;
	xmlChar *str;
	if ((str = xmlGetProp(xn, (xmlChar *)Type)) == NULL) {
		topo_mod_dprintf(mp, "Property missing type");
		(void) topo_mod_seterrno(mp, ETOPO_PRSR_NOATTR);
		return (TOPO_TYPE_INVALID);
	}
	if (xmlStrcmp(str, (xmlChar *)Int32) == 0) {
		rv = TOPO_TYPE_INT32;
	} else if (xmlStrcmp(str, (xmlChar *)UInt32) == 0) {
		rv = TOPO_TYPE_UINT32;
	} else if (xmlStrcmp(str, (xmlChar *)Int64) == 0) {
		rv = TOPO_TYPE_INT64;
	} else if (xmlStrcmp(str, (xmlChar *)UInt64) == 0) {
		rv = TOPO_TYPE_UINT64;
	} else if (xmlStrcmp(str, (xmlChar *)FMRI) == 0) {
		rv = TOPO_TYPE_FMRI;
	} else if (xmlStrcmp(str, (xmlChar *)String) == 0) {
		rv = TOPO_TYPE_STRING;
	} else {
		xmlFree(str);
		topo_mod_dprintf(mp, "Unrecognized type attribute.\n");
		(void) topo_mod_seterrno(mp, ETOPO_PRSR_BADTYPE);
		return (TOPO_TYPE_INVALID);
	}
	xmlFree(str);
	return (rv);
}

static int
xmlprop_xlate(topo_mod_t *mp, xmlNodePtr xn, nvlist_t *nvl)
{
	topo_type_t ptype;
	xmlChar *str;
	nvlist_t *fmri;
	uint64_t ui;
	int e;

	if ((str = xmlGetProp(xn, (xmlChar *)Immutable)) != NULL) {
		if (xmlStrcmp(str, (xmlChar *)False) == 0)
			e = nvlist_add_boolean_value(nvl, INV_IMMUTE, B_FALSE);
		else
			e = nvlist_add_boolean_value(nvl, INV_IMMUTE, B_TRUE);
		xmlFree(str);
		if (e != 0)
			return (-1);
	}
	/* FMXXX stability of the property value */
	if ((ptype = xmlattr_to_type(mp, xn)) == TOPO_TYPE_INVALID)
		return (-1);
	e = nvlist_add_int32(nvl, INV_PVALTYPE, ptype);
	if (e != 0)
		return (-1);
	switch (ptype) {
	case TOPO_TYPE_INT32:
		if (xmlattr_to_int(mp, xn, Value, &ui) < 0)
			return (-1);
		e = nvlist_add_int32(nvl, INV_PVAL, (int32_t)ui);
		break;
	case TOPO_TYPE_UINT32:
		if (xmlattr_to_int(mp, xn, Value, &ui) < 0)
			return (-1);
		e = nvlist_add_uint32(nvl, INV_PVAL, (uint32_t)ui);
		break;
	case TOPO_TYPE_INT64:
		if (xmlattr_to_int(mp, xn, Value, &ui) < 0)
			return (-1);
		e = nvlist_add_int64(nvl, INV_PVAL, (int64_t)ui);
		break;
	case TOPO_TYPE_UINT64:
		if (xmlattr_to_int(mp, xn, Value, &ui) < 0)
			return (-1);
		e = nvlist_add_uint64(nvl, INV_PVAL, ui);
		break;
	case TOPO_TYPE_FMRI:
		if (xmlattr_to_fmri(mp, xn, Value, &fmri) < 0)
			return (-1);
		e = nvlist_add_nvlist(nvl, INV_PVAL, fmri);
		nvlist_free(fmri);
		break;
	case TOPO_TYPE_STRING:
		if ((str = xmlGetProp(xn, (xmlChar *)Value)) == NULL)
			return (-1);
		e = nvlist_add_string(nvl, INV_PVAL, (char *)str);
		xmlFree(str);
		break;
	default:
		topo_mod_dprintf(mp, "Unrecognized type attribute.\n");
		return (topo_mod_seterrno(mp, ETOPO_PRSR_BADTYPE));
	}
	if (e != 0) {
		topo_mod_dprintf(mp, "Nvlist construction failed.\n");
		return (topo_mod_seterrno(mp, ETOPO_NOMEM));
	}
	return (0);
}

static int
dependent_create(topo_mod_t *mp,
    tf_info_t *xinfo, tf_pad_t *pad, xmlNodePtr dxn, tnode_t *ptn)
{
	tf_rdata_t *rp, *pp, *np;
	xmlChar *grptype;
	int sibs = 0;

	topo_mod_dprintf(mp, "dependent create\n");
	if ((grptype = xmlGetProp(dxn, (xmlChar *)Grouping)) == NULL) {
		topo_mod_dprintf(mp, "Dependents missing grouping attribute");
		return (topo_mod_seterrno(mp, ETOPO_PRSR_NOATTR));
	}

	pp = NULL;
	if (xmlStrcmp(grptype, (xmlChar *)Siblings) == 0) {
		rp = pad->tpad_sibs;
		sibs++;
	} else if (xmlStrcmp(grptype, (xmlChar *)Children) == 0) {
		rp = pad->tpad_child;
	} else {
		topo_mod_dprintf(mp,
		    "Dependents have bogus grouping attribute");
		xmlFree(grptype);
		return (topo_mod_seterrno(mp, ETOPO_PRSR_BADGRP));
	}
	xmlFree(grptype);
	/* Add processed dependents to the tail of the list */
	while (rp != NULL) {
		pp = rp;
		rp = rp->rd_next;
	}
	if ((np = topo_xml_walk(mp, xinfo, dxn, ptn)) == NULL) {
		topo_mod_dprintf(mp,
		    "error within dependent .xml topology: "
		    "%s\n", topo_strerror(topo_mod_errno(mp)));
		return (-1);
	}
	if (pp != NULL)
		pp->rd_next = np;
	else if (sibs == 1)
		pad->tpad_sibs = np;
	else
		pad->tpad_child = np;
	return (0);
}

static int
dependents_create(topo_mod_t *mp,
    tf_info_t *xinfo, tf_pad_t *pad, xmlNodePtr pxn, tnode_t *ptn)
{
	xmlNodePtr cn;

	topo_mod_dprintf(mp, "dependents create\n");
	for (cn = pxn->xmlChildrenNode; cn != NULL; cn = cn->next) {
		if (xmlStrcmp(cn->name, (xmlChar *)Dependents) == 0) {
			if (dependent_create(mp, xinfo, pad, cn, ptn) < 0)
				return (-1);
		}
	}
	return (0);
}

static int
prop_create(topo_mod_t *mp,
    nvlist_t *pfmri, tnode_t *ptn, const char *gnm, const char *pnm,
    topo_type_t ptype, int flag)
{
	nvlist_t *fmri;
	uint32_t ui32;
	uint64_t ui64;
	int32_t i32;
	int64_t i64;
	char *str;
	int err, e;

	topo_mod_dprintf(mp, "prop create\n");
	switch (ptype) {
	case TOPO_TYPE_INT32:
		e = nvlist_lookup_int32(pfmri, INV_PVAL, &i32);
		break;
	case TOPO_TYPE_UINT32:
		e = nvlist_lookup_uint32(pfmri, INV_PVAL, &ui32);
		break;
	case TOPO_TYPE_INT64:
		e = nvlist_lookup_int64(pfmri, INV_PVAL, &i64);
		break;
	case TOPO_TYPE_UINT64:
		e = nvlist_lookup_uint64(pfmri, INV_PVAL, &ui64);
		break;
	case TOPO_TYPE_FMRI:
		e = nvlist_lookup_nvlist(pfmri, INV_PVAL, &fmri);
		break;
	case TOPO_TYPE_STRING:
		e = nvlist_lookup_string(pfmri, INV_PVAL, &str);
		break;
	default:
		e = ETOPO_PRSR_BADTYPE;
	}
	if (e != 0) {
		topo_mod_dprintf(mp, "prop value lookup failed.\n");
		return (topo_mod_seterrno(mp, e));
	}
	switch (ptype) {
	case TOPO_TYPE_INT32:
		e = topo_prop_set_int32(ptn, gnm, pnm, flag, i32, &err);
		break;
	case TOPO_TYPE_UINT32:
		e = topo_prop_set_uint32(ptn, gnm, pnm, flag, ui32, &err);
		break;
	case TOPO_TYPE_INT64:
		e = topo_prop_set_int64(ptn, gnm, pnm, flag, i64, &err);
		break;
	case TOPO_TYPE_UINT64:
		e = topo_prop_set_uint64(ptn, gnm, pnm, flag, ui64, &err);
		break;
	case TOPO_TYPE_FMRI:
		e = topo_prop_set_fmri(ptn, gnm, pnm, flag, fmri, &err);
		break;
	case TOPO_TYPE_STRING:
		e = topo_prop_set_string(ptn, gnm, pnm, flag, str, &err);
		break;
	}
	if (e != 0) {
		topo_mod_dprintf(mp, "prop set failed.\n");
		return (topo_mod_seterrno(mp, err));
	}
	return (0);
}

static int
props_create(topo_mod_t *mp,
    tnode_t *ptn, const char *gnm, nvlist_t **props, int nprops)
{
	topo_type_t ptype;
	boolean_t pim;
	char *pnm;
	int32_t i32;
	int flag;
	int pn;
	int e;

	topo_mod_dprintf(mp, "props create\n");
	for (pn = 0; pn < nprops; pn++) {
		e = nvlist_lookup_string(props[pn], INV_PNAME, &pnm);
		if (e != 0) {
			topo_mod_dprintf(mp,
			    "props create lookup (%s) failure: %s",
			    INV_PNAME, topo_strerror(e));
			return (topo_mod_seterrno(mp, ETOPO_PRSR_NVPROP));
		}
		e = nvlist_lookup_boolean_value(props[pn], INV_IMMUTE, &pim);
		if (e != 0) {
			topo_mod_dprintf(mp,
			    "props create lookup (%s) failure: %s",
			    INV_IMMUTE, topo_strerror(e));
			return (topo_mod_seterrno(mp, ETOPO_PRSR_NVPROP));
		}
		flag = (pim == B_TRUE) ?
		    TOPO_PROP_SET_ONCE : TOPO_PROP_SET_MULTIPLE;

		e = nvlist_lookup_int32(props[pn], INV_PVALTYPE, &i32);
		if (e != 0) {
			topo_mod_dprintf(mp,
			    "props create lookup (%s) failure: %s",
			    INV_PVALTYPE, topo_strerror(e));
			return (topo_mod_seterrno(mp, ETOPO_PRSR_NVPROP));
		}
		ptype = (topo_type_t)i32;
		if (prop_create(mp, props[pn], ptn, gnm, pnm, ptype, flag) < 0)
			return (-1);
	}
	return (0);
}

static int
pgroups_create(topo_mod_t *mp, tf_pad_t *pad, tnode_t *ptn)
{
	topo_stability_t gs;
	nvlist_t **props;
	char *gnm;
	uint32_t rnprops, nprops;
	uint32_t ui32;
	int pg;
	int e;

	topo_mod_dprintf(mp, "pgroups create\n");
	for (pg = 0; pg < pad->tpad_pgcnt; pg++) {
		e = nvlist_lookup_string(pad->tpad_pgs[pg],
		    INV_PGRP_NAME, &gnm);
		if (e != 0) {
			topo_mod_dprintf(mp, "pad lookup (%s) failed.\n",
			    INV_PGRP_NAME);
			return (topo_mod_seterrno(mp, ETOPO_PRSR_NVPROP));
		}
		e = nvlist_lookup_uint32(pad->tpad_pgs[pg],
		    INV_PGRP_STAB, &ui32);
		if (e != 0) {
			topo_mod_dprintf(mp, "pad lookup (%s) failed.\n",
			    INV_PGRP_STAB);
			return (topo_mod_seterrno(mp, ETOPO_PRSR_NVPROP));
		}
		gs = (topo_stability_t)ui32;
		if (topo_pgroup_create(ptn, gnm, gs, &e) != 0) {
			if (e != ETOPO_PROP_DEFD) {
				topo_mod_dprintf(mp,
				    "pgroups create failure: %s\n",
				    topo_strerror(e));
				return (-1);
			}
		}
		e = nvlist_lookup_uint32(pad->tpad_pgs[pg],
		    INV_PGRP_NPROP, &rnprops);
		e |= nvlist_lookup_nvlist_array(pad->tpad_pgs[pg],
		    INV_PGRP_ALLPROPS, &props, &nprops);
		if (rnprops != nprops) {
			topo_mod_dprintf(mp,
			    "warning: recorded number of props %d does not "
			    "match number of props recorded %d.\n",
			    rnprops, nprops);
		}
		if (props_create(mp, ptn, gnm, props, nprops) < 0)
			return (-1);
	}
	return (0);
}

static nvlist_t *
pval_record(topo_mod_t *mp, xmlNodePtr xn)
{
	nvlist_t *pnvl = NULL;
	xmlChar *pname;

	topo_mod_dprintf(mp, "pval record\n");
	if ((pname = xmlGetProp(xn, (xmlChar *)Name)) == NULL) {
		topo_mod_dprintf(mp, "propval lacks a name\n");
		(void) topo_mod_seterrno(mp, ETOPO_PRSR_NOATTR);
		return (NULL);
	}
	if (topo_mod_nvalloc(mp, &pnvl, NV_UNIQUE_NAME) < 0) {
		xmlFree(pname);
		return (NULL);
	}
	if (nvlist_add_string(pnvl, INV_PNAME, (char *)pname) < 0) {
		xmlFree(pname);
		nvlist_free(pnvl);
		return (NULL);
	}
	xmlFree(pname);
	/* FMXXX stability of the property name */

	if (xmlprop_xlate(mp, xn, pnvl) < 0) {
		nvlist_free(pnvl);
		return (NULL);
	}
	return (pnvl);
}

static int
pgroup_record(topo_mod_t *mp, xmlNodePtr pxn, tf_pad_t *rpad, int pi)
{
	topo_stability_t nmstab;
	xmlNodePtr sn = NULL;
	xmlNodePtr cn;
	xmlChar *name;
	nvlist_t **apl = NULL;
	nvlist_t *pgnvl = NULL;
	int pcnt = 0;
	int ai = 0;
	int e;

	topo_mod_dprintf(mp, "pgroup record\n");
	if ((name = xmlGetProp(pxn, (xmlChar *)Name)) == NULL) {
		topo_mod_dprintf(mp, "propgroup lacks a name\n");
		return (topo_mod_seterrno(mp, ETOPO_PRSR_NOATTR));
	}
	topo_mod_dprintf(mp, "pgroup %s\n", (char *)name);
	for (cn = pxn->xmlChildrenNode; cn != NULL; cn = cn->next) {
		if (xmlStrcmp(cn->name, (xmlChar *)Propval) == 0)
			pcnt++;
		else if (xmlStrcmp(cn->name, (xmlChar *)Namestab) == 0)
			sn = cn;
	}
	if (xmlattr_to_stab(mp, sn, &nmstab) < 0) {
		xmlFree(name);
		return (-1);
	}
	if (topo_mod_nvalloc(mp, &pgnvl, NV_UNIQUE_NAME) < 0) {
		xmlFree(name);
		return (-1);
	}

	e = nvlist_add_string(pgnvl, INV_PGRP_NAME, (char *)name);
	e |= nvlist_add_uint32(pgnvl, INV_PGRP_STAB, nmstab);
	e |= nvlist_add_uint32(pgnvl, INV_PGRP_NPROP, pcnt);
	if (e != 0 ||
	    (apl = topo_mod_zalloc(mp, pcnt * sizeof (nvlist_t *))) == NULL) {
		xmlFree(name);
		nvlist_free(pgnvl);
		return (-1);
	}
	for (cn = pxn->xmlChildrenNode; cn != NULL; cn = cn->next) {
		if (xmlStrcmp(cn->name, (xmlChar *)Propval) == 0) {
			if (ai < pcnt) {
				if ((apl[ai] = pval_record(mp, cn)) == NULL)
					break;
			}
			ai++;
		}
	}
	xmlFree(name);
	e |= (ai != pcnt);
	e |= nvlist_add_nvlist_array(pgnvl, INV_PGRP_ALLPROPS, apl, pcnt);
	for (ai = 0; ai < pcnt; ai++)
		if (apl[ai] != NULL)
			nvlist_free(apl[ai]);
	topo_mod_free(mp, apl, pcnt * sizeof (nvlist_t *));
	if (e != 0) {
		nvlist_free(pgnvl);
		return (-1);
	}
	rpad->tpad_pgs[pi] = pgnvl;
	return (0);
}

static int
pgroups_record(topo_mod_t *mp, xmlNodePtr pxn, tf_pad_t *rpad)
{
	xmlNodePtr cn;
	int pi = 0;

	topo_mod_dprintf(mp, "pgroups record\n");
	for (cn = pxn->xmlChildrenNode; cn != NULL; cn = cn->next) {
		if (xmlStrcmp(cn->name, (xmlChar *)Propgrp) == 0) {
			if (pgroup_record(mp, cn, rpad, pi++) < 0)
				return (-1);
		}
	}
	return (0);
}

/*
 * Process the property group and dependents xmlNode children of
 * parent xmlNode pxn.
 */
static int
pad_process(topo_mod_t *mp,
    tf_info_t *xinfo, xmlNodePtr pxn, tnode_t *ptn, tf_pad_t **rpad)
{
	xmlNodePtr cn;
	int pgcnt = 0;
	int dcnt = 0;

	topo_mod_dprintf(mp, "pad process beneath %s\n", topo_node_name(ptn));
	if (*rpad == NULL) {
		for (cn = pxn->xmlChildrenNode; cn != NULL; cn = cn->next) {
			if (xmlStrcmp(cn->name, (xmlChar *)Dependents) == 0)
				dcnt++;
			else if (xmlStrcmp(cn->name, (xmlChar *)Propgrp) == 0)
				pgcnt++;
		}
		if ((*rpad = tf_pad_new(mp, pgcnt, dcnt)) == NULL)
			return (-1);
		if (dcnt == 0 && pgcnt == 0)
			return (0);
		if (pgcnt > 0) {
			(*rpad)->tpad_pgs =
			    topo_mod_zalloc(mp, pgcnt * sizeof (nvlist_t *));
			if ((*rpad)->tpad_pgs == NULL) {
				tf_pad_free(mp, *rpad);
				return (-1);
			}
			if (pgroups_record(mp, pxn, *rpad) < 0) {
				tf_pad_free(mp, *rpad);
				return (-1);
			}
		}
	}

	if ((*rpad)->tpad_dcnt > 0)
		if (dependents_create(mp, xinfo, *rpad, pxn, ptn) < 0)
			return (-1);

	if ((*rpad)->tpad_pgcnt > 0)
		if (pgroups_create(mp, *rpad, ptn) < 0)
			return (-1);
	return (0);
}

static int
node_process(topo_mod_t *mp, xmlNodePtr nn, tf_rdata_t *rd)
{
	topo_instance_t inst;
	tf_idata_t *newi;
	tnode_t *ntn;
	uint64_t ui;
	int rv = -1;

	topo_mod_dprintf(mp, "node process %s\n", rd->rd_name);
	if (xmlattr_to_int(mp, nn, Instance, &ui) < 0)
		goto nodedone;
	inst = (topo_instance_t)ui;

	if (topo_mod_enumerate(rd->rd_mod,
	    rd->rd_pn, rd->rd_finfo->tf_scheme, rd->rd_name, inst, inst) < 0)
		goto nodedone;
	ntn = topo_node_lookup(rd->rd_pn, rd->rd_name, inst);
	if (ntn == NULL)
		goto nodedone;

	if ((newi = tf_idata_new(mp, inst, ntn)) == NULL) {
		topo_mod_dprintf(mp, "tf_idata_new failed.\n");
		goto nodedone;
	}
	if (tf_idata_insert(mp, &rd->rd_instances, newi) < 0) {
		topo_mod_dprintf(mp, "tf_idata_insert failed.\n");
		goto nodedone;
	}
	if (pad_process(mp, rd->rd_finfo, nn, ntn, &newi->ti_pad) < 0)
		goto nodedone;
	rv = 0;
nodedone:
	topo_mod_dprintf(mp, "done with node %s.\n", rd->rd_name);
	return (rv);
}

static tf_edata_t *
enum_attributes_process(topo_mod_t *mp, xmlNodePtr en)
{
	tf_edata_t *einfo;
	uint64_t ui;

	topo_mod_dprintf(mp, "enum attributes process\n");
	if ((einfo = topo_mod_zalloc(mp, sizeof (tf_edata_t))) == NULL) {
		(void) topo_mod_seterrno(mp, ETOPO_NOMEM);
		return (NULL);
	}
	einfo->te_name = (char *)xmlGetProp(en, (xmlChar *)Name);
	if (einfo->te_name == NULL) {
		topo_mod_dprintf(mp, "Enumerator name attribute missing.\n");
		(void) topo_mod_seterrno(mp, ETOPO_PRSR_NOATTR);
		goto enodedone;
	}
	einfo->te_path = (char *)xmlGetProp(en, (xmlChar *)Path);
	if (einfo->te_path == NULL) {
		topo_mod_dprintf(mp, "Enumerator path attribute missing.\n");
		(void) topo_mod_seterrno(mp, ETOPO_PRSR_NOATTR);
		goto enodedone;
	}
	if (xmlattr_to_int(mp, en, Version, &ui) < 0)
		goto enodedone;
	einfo->te_vers = (int)ui;
	/*
	 * FMXXX must deal with name-stability and apply-methods (which are
	 * child xmlNodes)
	 */
	return (einfo);

enodedone:
	if (einfo->te_name != NULL)
		xmlFree(einfo->te_name);
	if (einfo->te_path != NULL)
		xmlFree(einfo->te_path);
	return (NULL);
}

static int
enum_run(topo_mod_t *mp, tf_rdata_t *rd)
{
	int e = -1;

	/*
	 * first see if the module is already loaded
	 */
	rd->rd_mod = topo_mod_lookup(mp->tm_hdl, rd->rd_einfo->te_name);
	if (rd->rd_mod == NULL) {
		char *mostpath = topo_mod_alloc(mp, PATH_MAX);
		char *skip;
		int prepend = 0;

		if (mostpath == NULL)
			return (-1);
		skip = rd->rd_einfo->te_path;
		if (*skip == '%' && *(skip + 1) == 'r') {
			prepend = 1;
			skip += 2;
		}
		(void) snprintf(mostpath,
		    PATH_MAX, "%s%s/%s.so",
		    (prepend == 1) ? topo_mod_rootdir(mp) : "",
		    skip, rd->rd_einfo->te_name);
		topo_mod_dprintf(mp,
		    "enum_run, load %s.\n", mostpath);
		if ((rd->rd_mod = topo_mod_load(mp, mostpath)) == NULL) {
			topo_mod_dprintf(mp,
			    "mod_load of %s failed: %s.\n",
			    mostpath, topo_strerror(topo_mod_errno(mp)));
			topo_free(mostpath, PATH_MAX);
			return (e);
		}
		topo_free(mostpath, PATH_MAX);
	}
	/*
	 * We're live, so let's enumerate.
	 */
	topo_mod_dprintf(mp, "enumerate request. (%s)\n",
	    rd->rd_einfo->te_name);
	e = topo_mod_enumerate(rd->rd_mod, rd->rd_pn, rd->rd_einfo->te_name,
	    rd->rd_name, rd->rd_min, rd->rd_max);
	topo_mod_dprintf(mp, "back from enumeration. %d\n", e);
	if (e != 0) {
		topo_mod_dprintf(mp, "Enumeration failed (%s)\n",
		    topo_strerror(topo_mod_errno(mp)));
		return (topo_mod_seterrno(mp, EMOD_PARTIAL_ENUM));
	}
	return (e);
}

int
topo_xml_range_process(topo_mod_t *mp, xmlNodePtr rn, tf_rdata_t *rd)
{
	/*
	 * The range may have several children xmlNodes, that may
	 * represent the enumeration method, property groups,
	 * dependents or nodes.
	 */
	xmlNodePtr cn;
	tnode_t *ct;
	int e;

	topo_mod_dprintf(mp, "process %s range beneath %s\n",
	    rd->rd_name, topo_node_name(rd->rd_pn));
	e = topo_node_range_create(mp,
	    rd->rd_pn, rd->rd_name, rd->rd_min, rd->rd_max);
	if (e != 0) {
		topo_mod_dprintf(mp, "Range create failed due to %s.\n",
		    topo_strerror(topo_mod_errno(mp)));
		return (-1);
	}
	for (cn = rn->xmlChildrenNode; cn != NULL; cn = cn->next)
		if (xmlStrcmp(cn->name, (xmlChar *)Enum_meth) == 0)
			break;

	if (cn != NULL) {
		if ((rd->rd_einfo = enum_attributes_process(mp, cn)) == NULL)
			return (-1);
		if (enum_run(mp, rd) < 0) {
			topo_mod_dprintf(mp, "Enumeration failed.\n");
			return (-1);
		}
	}

	/* Now look for nodes, i.e., hard instances */
	for (cn = rn->xmlChildrenNode; cn != NULL; cn = cn->next) {
		if (xmlStrcmp(cn->name, (xmlChar *)Node) == 0)
			if (node_process(mp, cn, rd) < 0) {
				topo_mod_dprintf(mp,
				    "node processing failed: %s.\n",
				    topo_strerror(topo_mod_errno(mp)));
				return (topo_mod_seterrno(mp,
				    EMOD_PARTIAL_ENUM));
			}
	}

	/* Property groups and Dependencies */
	ct = topo_child_first(rd->rd_pn);
	while (ct != NULL) {
		/* Only care about instances within the range */
		if (strcmp(topo_node_name(ct), rd->rd_name) != 0) {
			ct = topo_child_next(rd->rd_pn, ct);
			continue;
		}
		if (pad_process(mp, rd->rd_finfo, rn, ct, &rd->rd_pad) < 0)
			return (-1);
		ct = topo_child_next(rd->rd_pn, ct);
	}
	topo_mod_dprintf(mp, "end range process %s\n",
	    rd->rd_name);
	return (0);
}

static tf_rdata_t *
topo_xml_walk(topo_mod_t *mp,
    tf_info_t *xinfo, xmlNodePtr croot, tnode_t *troot)
{
	xmlNodePtr curr;
	tf_rdata_t *rr, *pr, *rdp;

	/*
	 * What we're interested in are children xmlNodes of croot tagged
	 * as 'ranges', these define topology nodes may exist, and need
	 * to be verified.
	 */
	topo_mod_dprintf(mp, "topo_xml_walk\n");
	rr = pr = NULL;
	for (curr = croot->xmlChildrenNode; curr != NULL; curr = curr->next) {
		if (curr->name == NULL) {
			topo_mod_dprintf(mp, "Ignoring nameless xmlnode.\n");
			continue;
		}
		if (xmlStrcmp(curr->name, (xmlChar *)Range) != 0) {
			topo_mod_dprintf(mp,
			    "Ignoring non-range %s.\n", curr->name);
			continue;
		}
		if ((rdp = tf_rdata_new(mp, xinfo, curr, troot)) == NULL) {
			tf_rdata_free(mp, rr);
			return (NULL);
		}
		if (pr == NULL) {
			rr = pr = rdp;
		} else {
			pr->rd_next = rdp;
			pr = rdp;
		}
		rr->rd_cnt++;
	}
	return (rr);
}

/*
 *  Convert parsed xml topology description into topology nodes
 */
int
topo_xml_enum(topo_mod_t *tmp, tf_info_t *xinfo, tnode_t *troot)
{
	xmlNodePtr xroot;

	if ((xroot = xmlDocGetRootElement(xinfo->tf_xdoc)) == NULL) {
		topo_mod_dprintf(tmp, "Couldn't get root xmlNode.\n");
		return (-1);
	}
	if ((xinfo->tf_rd = topo_xml_walk(tmp, xinfo, xroot, troot)) == NULL) {
		topo_mod_dprintf(tmp,
		    "error within .xml topology: %s\n",
		    topo_strerror(topo_mod_errno(tmp)));
		return (-1);
	}
	return (0);
}

/*
 * Load an XML tree from filename and read it into a DOM parse tree.
 */
static tf_info_t *
txml_file_parse(topo_mod_t *tmp,
    int fd, const char *filenm, const char *escheme)
{
	xmlValidCtxtPtr vcp;
	xmlNodePtr cursor;
	xmlDocPtr document;
	xmlDtdPtr dtd = NULL;
	xmlChar *scheme = NULL;
	char *dtdpath = NULL;
	int readflags = 0;
	tf_info_t *r;
	int e;

	/*
	 * Since topologies can XInclude other topologies, and libxml2
	 * doesn't do DTD-based validation with XInclude, by default
	 * we don't validate topology files.  One can force
	 * validation, though, by creating a TOPOXML_VALIDATE
	 * environment variable and creating a TOPO_DTD environment
	 * variable with the path to the DTD against which to validate.
	 */
	if (getenv("TOPOXML_VALIDATE") != NULL) {
		dtdpath = getenv("TOPO_DTD");
		if (dtdpath != NULL)
			xmlLoadExtDtdDefaultValue = 0;
	}

	/*
	 * Splat warnings and errors related to parsing the topology
	 * file if the TOPOXML_PERROR environment variable exists.
	 */
	if (getenv("TOPOXML_PERROR") == NULL)
		readflags = XML_PARSE_NOERROR | XML_PARSE_NOWARNING;

	if ((document = xmlReadFd(fd, filenm, NULL, readflags)) == NULL) {
		topo_mod_dprintf(tmp, "couldn't parse document.\n");
		return (NULL);
	}

	/*
	 * Verify that this is a document type we understand.
	 */
	if ((dtd = xmlGetIntSubset(document)) == NULL) {
		topo_mod_dprintf(tmp, "document has no DTD.\n");
		return (NULL);
	}

	if (strcmp((const char *)dtd->SystemID, TOPO_DTD_PATH) == -1) {
		topo_mod_dprintf(tmp,
		    "document DTD unknown; bad topology file?\n");
		return (NULL);
	}

	if ((cursor = xmlDocGetRootElement(document)) == NULL) {
		topo_mod_dprintf(tmp, "document is empty.\n");
		xmlFreeDoc(document);
		return (NULL);
	}

	/*
	 * Make sure we're looking at a topology description in the
	 * expected scheme.
	 */
	if (xmlStrcmp(cursor->name, (xmlChar *)Topology) != 0) {
		topo_mod_dprintf(tmp,
		    "document is not a topology description.\n");
		xmlFreeDoc(document);
		return (NULL);
	}
	if ((scheme = xmlGetProp(cursor, (xmlChar *)Scheme)) == NULL) {
		topo_mod_dprintf(tmp, "topology lacks a scheme.\n");
		(void) topo_mod_seterrno(tmp, ETOPO_PRSR_NOATTR);
		xmlFreeDoc(document);
		return (NULL);
	}
	if (xmlStrcmp(scheme, (xmlChar *)escheme) != 0) {
		topo_mod_dprintf(tmp,
		    "topology in unrecognized scheme, %s, expecting %s\n",
		    scheme, escheme);
		(void) topo_mod_seterrno(tmp, ETOPO_PRSR_BADSCH);
		xmlFree(scheme);
		xmlFreeDoc(document);
		return (NULL);
	}

	if (dtdpath != NULL) {
		dtd = xmlParseDTD(NULL, (xmlChar *)dtdpath);
		if (dtd == NULL) {
			topo_mod_dprintf(tmp,
			    "Could not parse DTD \"%s\".\n",
			    dtdpath);
			return (NULL);
		}

		if (document->extSubset != NULL)
			xmlFreeDtd(document->extSubset);

		document->extSubset = dtd;
	}

	if (xmlXIncludeProcessFlags(document, XML_PARSE_XINCLUDE) == -1) {;
		topo_mod_dprintf(tmp,
		    "couldn't handle XInclude statements in document\n");
		return (NULL);
	}

	if ((vcp = xmlNewValidCtxt()) == NULL) {
		xmlFree(scheme);
		scheme = NULL;
		return (NULL);
	}
	vcp->warning = xmlParserValidityWarning;
	vcp->error = xmlParserValidityError;

	e = xmlValidateDocument(vcp, document);

	xmlFreeValidCtxt(vcp);

	if (e == 0) {
		topo_mod_dprintf(tmp, "Document is not valid.\n");
		xmlFreeDoc(document);
		return (NULL);
	}

	if ((r = tf_info_new(tmp, filenm, document, scheme)) == NULL)
		return (NULL);

	/* txml_dump(0, cursor); */

	xmlFree(scheme);
	scheme = NULL;
	return (r);
}

static int
txml_file_open(topo_mod_t *mp, const char *filename)
{
	int rfd;
	if ((rfd = open(filename, O_RDONLY)) < 0)
		return (topo_mod_seterrno(mp, ETOPO_PRSR_NOENT));
	return (rfd);
}

tf_info_t *
topo_xml_read(topo_mod_t *tmp, const char *path, const char *escheme)
{
	int fd;
	tf_info_t *tip;

	if ((fd = txml_file_open(tmp, path)) < 0) {
		return (NULL);
	}
	tip = txml_file_parse(tmp, fd, path, escheme);
	(void) close(fd);
	return (tip);
}
