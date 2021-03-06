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

#pragma ident	"@(#)pcibus.c	1.5	06/05/09 SMI"

#include <sys/fm/protocol.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <alloca.h>
#include <sys/param.h>
#include <sys/pci.h>
#include <sys/pcie.h>
#include <libdevinfo.h>
#include <libnvpair.h>
#include <fm/topo_mod.h>
#include <pthread.h>

#include "hostbridge.h"
#include "pcibus.h"
#include "did.h"
#include "did_props.h"
#include "util.h"

extern txprop_t Bus_common_props[];
extern txprop_t Dev_common_props[];
extern txprop_t Fn_common_props[];
extern int Bus_propcnt;
extern int Dev_propcnt;
extern int Fn_propcnt;

extern int pcifn_enum(topo_mod_t *, tnode_t *);

static void pci_release(topo_mod_t *, tnode_t *);
static int pci_enum(topo_mod_t *, tnode_t *, const char *, topo_instance_t,
    topo_instance_t, void *);
static int pci_contains(topo_mod_t *, tnode_t *, topo_version_t, nvlist_t *,
    nvlist_t **);
static int pci_present(topo_mod_t *, tnode_t *, topo_version_t, nvlist_t *,
    nvlist_t **);
static int pci_label(topo_mod_t *, tnode_t *, topo_version_t, nvlist_t *,
    nvlist_t **);

const topo_modinfo_t Pci_info =
	{ PCI_BUS, PCI_ENUMR_VERS, pci_enum, pci_release };

const topo_method_t Pci_methods[] = {
	{ "pci_contains", "pci element contains other element", PCI_ENUMR_VERS,
	    TOPO_STABILITY_INTERNAL, pci_contains },
	{ "pci_present", "pci element currently present", PCI_ENUMR_VERS,
	    TOPO_STABILITY_INTERNAL, pci_present },
	{ TOPO_METH_LABEL, TOPO_METH_LABEL_DESC,
	    TOPO_METH_LABEL_VERSION, TOPO_STABILITY_INTERNAL, pci_label },
	{ NULL }
};

void
_topo_init(topo_mod_t *modhdl)
{
	/*
	 * Turn on module debugging output
	 */
	if (getenv("TOPOPCIDBG") != NULL)
		topo_mod_setdebug(modhdl, TOPO_DBG_ALL);
	topo_mod_dprintf(modhdl, "initializing pcibus builtin\n");

	topo_mod_register(modhdl, &Pci_info, NULL);
	topo_mod_dprintf(modhdl, "PCI Enumr initd\n");
}

void
_topo_fini(topo_mod_t *modhdl)
{
	topo_mod_unregister(modhdl);
}

/*ARGSUSED*/
static int
pci_contains(topo_mod_t *mp, tnode_t *node, topo_version_t version,
    nvlist_t *in, nvlist_t **out)
{
	return (0);
}

/*ARGSUSED*/
static int
pci_present(topo_mod_t *mp, tnode_t *node, topo_version_t version,
    nvlist_t *in, nvlist_t **out)
{
	return (0);
}

static int
pci_label(topo_mod_t *mp, tnode_t *node, topo_version_t version,
    nvlist_t *in, nvlist_t **out)
{
	if (version > TOPO_METH_LABEL_VERSION)
		return (topo_mod_seterrno(mp, EMOD_VER_NEW));
	return (platform_pci_label(node, in, out, mp));
}

static tnode_t *
pci_tnode_create(tnode_t *parent,
    const char *name, topo_instance_t i, void *priv, topo_mod_t *mod)
{
	tnode_t *ntn;

	if ((ntn = tnode_create(mod, parent, name, i, priv)) == NULL)
		return (NULL);
	if (topo_method_register(mod, ntn, Pci_methods) < 0) {
		topo_mod_dprintf(mod, "topo_method_register failed: %s\n",
		    topo_strerror(topo_mod_errno(mod)));
		topo_node_unbind(ntn);
		return (NULL);
	}
	return (ntn);
}

/*ARGSUSED*/
static int
hostbridge_asdevice(tnode_t *bus, did_hash_t *didhash,
    di_prom_handle_t promtree, topo_mod_t *mod)
{
	di_node_t di;
	tnode_t *dev32;

	di = topo_node_private(bus);
	assert(di != DI_NODE_NIL);

	if ((dev32 = pcidev_declare(bus, di, 32, didhash, promtree, mod))
	    == NULL)
		return (-1);
	if (pcifn_declare(dev32, di, 0, didhash, promtree, mod) == NULL)
		return (-1);
	return (0);
}

tnode_t *
pciexfn_declare(tnode_t *parent, di_node_t dn, topo_instance_t i,
    did_hash_t *didhash, di_prom_handle_t promtree, topo_mod_t *mod)
{
	did_t *pd;
	tnode_t *ntn;

	if ((pd = did_find(didhash, dn)) == NULL)
		return (NULL);
	if ((ntn = pci_tnode_create(parent, PCIEX_FUNCTION, i, dn, mod))
	    == NULL)
		return (NULL);
	if (did_props_set(ntn, pd, Fn_common_props, Fn_propcnt,
	    promtree) < 0) {
		topo_node_unbind(ntn);
		return (NULL);
	}
	/*
	 * We may find pci-express buses or plain-pci buses beneath a function
	 */
	if (child_range_add(mod, ntn, PCIEX_BUS, 0, MAX_HB_BUSES) < 0) {
		topo_node_range_destroy(ntn, PCIEX_BUS);
		return (NULL);
	}
	if (child_range_add(mod, ntn, PCI_BUS, 0, MAX_HB_BUSES) < 0) {
		topo_node_range_destroy(ntn, PCI_BUS);
		return (NULL);
	}
	return (ntn);
}

tnode_t *
pciexdev_declare(tnode_t *parent, di_node_t dn, topo_instance_t i,
    did_hash_t *didhash, di_prom_handle_t promtree, topo_mod_t *mod)
{
	did_t *pd;
	tnode_t *ntn;

	if ((pd = did_find(didhash, dn)) == NULL)
		return (NULL);
	if ((ntn = pci_tnode_create(parent, PCIEX_DEVICE, i, dn, mod)) == NULL)
		return (NULL);
	if (did_props_set(ntn, pd, Dev_common_props, Dev_propcnt,
	    promtree) < 0) {
		topo_node_unbind(ntn);
		return (NULL);
	}
	/*
	 * We can expect to find pci-express functions beneath the device
	 */
	if (child_range_add(mod,
	    ntn, PCIEX_FUNCTION, 0, MAX_PCIDEV_FNS) < 0) {
		topo_node_range_destroy(ntn, PCIEX_FUNCTION);
		return (NULL);
	}
	return (ntn);
}

tnode_t *
pciexbus_declare(tnode_t *parent, di_node_t dn, topo_instance_t i,
    did_hash_t *didhash, di_prom_handle_t promtree, topo_mod_t *mod)
{
	did_t *pd;
	tnode_t *ntn;

	if ((pd = did_find(didhash, dn)) == NULL)
		return (NULL);
	if ((ntn = pci_tnode_create(parent, PCIEX_BUS, i, dn, mod)) == NULL)
		return (NULL);
	if (did_props_set(ntn, pd, Bus_common_props, Bus_propcnt,
	    promtree) < 0) {
		topo_node_range_destroy(ntn, PCI_DEVICE);
		topo_node_unbind(ntn);
		return (NULL);
	}
	/*
	 * We can expect to find pci-express devices beneath the bus
	 */
	if (child_range_add(mod,
	    ntn, PCIEX_DEVICE, 0, MAX_PCIBUS_DEVS) < 0) {
		topo_node_range_destroy(ntn, PCIEX_DEVICE);
		return (NULL);
	}
	return (ntn);
}

tnode_t *
pcifn_declare(tnode_t *parent, di_node_t dn, topo_instance_t i,
    did_hash_t *didhash, di_prom_handle_t promtree, topo_mod_t *mod)
{
	did_t *pd;
	tnode_t *ntn;

	if ((pd = did_find(didhash, dn)) == NULL)
		return (NULL);
	if ((ntn = pci_tnode_create(parent, PCI_FUNCTION, i, dn, mod)) == NULL)
		return (NULL);
	if (did_props_set(ntn, pd, Fn_common_props, Fn_propcnt,
	    promtree) < 0) {
		topo_node_unbind(ntn);
		return (NULL);
	}
	/*
	 * We may find pci buses beneath a function
	 */
	if (child_range_add(mod, ntn, PCI_BUS, 0, MAX_HB_BUSES) < 0) {
		topo_node_unbind(ntn);
		return (NULL);
	}
	return (ntn);
}

tnode_t *
pcidev_declare(tnode_t *parent, di_node_t dn, topo_instance_t i,
    did_hash_t *didhash, di_prom_handle_t promtree, topo_mod_t *mod)
{
	di_node_t pdn;
	did_t *pd;
	tnode_t *ntn;
	did_t *ppd;

	if ((pdn = topo_node_private(parent)) == DI_NODE_NIL)
		return (NULL);
	if ((ppd = did_find(didhash, pdn)) == NULL)
		return (NULL);
	if ((pd = did_find(didhash, dn)) == NULL)
		return (NULL);
	if ((ntn = pci_tnode_create(parent, PCI_DEVICE, i, dn, mod)) == NULL)
		return (NULL);
	/*
	 * If our devinfo node is lacking certain information of its
	 * own, we may need/want to inherit the information available
	 * from our parent node's private data.
	 */
	did_inherit(ppd, pd);
	if (did_props_set(ntn, pd, Dev_common_props, Dev_propcnt,
	    promtree) < 0) {
		topo_node_unbind(ntn);
		return (NULL);
	}
	/*
	 * We can expect to find pci functions beneath the device
	 */
	if (child_range_add(mod, ntn, PCI_FUNCTION, 0, MAX_PCIDEV_FNS) < 0) {
		topo_node_range_destroy(ntn, PCI_FUNCTION);
		topo_node_unbind(ntn);
		return (NULL);
	}
	return (ntn);
}

tnode_t *
pcibus_declare(tnode_t *parent, di_node_t dn, topo_instance_t i,
    did_hash_t *didhash, di_prom_handle_t promtree, topo_mod_t *mod)
{
	did_t *pd;
	tnode_t *ntn;
	int hbchild = 0;

	if ((pd = did_find(didhash, dn)) == NULL)
		return (NULL);
	if ((ntn = pci_tnode_create(parent, PCI_BUS, i, dn, mod)) == NULL)
		return (NULL);
	/*
	 * If our devinfo node is lacking certain information of its
	 * own, and our parent topology node is a hostbridge, we may
	 * need/want to inherit information available in the
	 * hostbridge node's private data.
	 */
	if (strcmp(topo_node_name(parent), HOSTBRIDGE) == 0)
		hbchild = 1;
	if (did_props_set(ntn, pd, Bus_common_props, Bus_propcnt,
	    promtree) < 0) {
		topo_node_unbind(ntn);
		return (NULL);
	}
	/*
	 * We can expect to find pci devices beneath the bus
	 */
	if (child_range_add(mod, ntn, PCI_DEVICE, 0, MAX_PCIBUS_DEVS) < 0) {
		topo_node_unbind(ntn);
		return (NULL);
	}
	/*
	 * On each bus child of the hostbridge, we represent the
	 * hostbridge as a device outside the range of legal device
	 * numbers.
	 */
	if (hbchild == 1) {
		if (hostbridge_asdevice(ntn, didhash, promtree, mod) < 0) {
			topo_node_range_destroy(ntn, PCI_DEVICE);
			topo_node_unbind(ntn);
			return (NULL);
		}
	}
	return (ntn);
}

static int
pci_bridge_declare(tnode_t *fn, di_node_t din, int board,
    int bridge, int rc, int depth, did_hash_t *didhash,
    di_prom_handle_t promtree, topo_mod_t *mod)
{
	int err, excap, extyp;

	excap = pciex_cap_get(didhash, din);
	extyp = excap & PCIE_PCIECAP_DEV_TYPE_MASK;
	if (excap <= 0 ||
	    extyp != PCIE_PCIECAP_DEV_TYPE_PCIE2PCI)
		err = pci_children_instantiate(fn,
		    din, board, bridge, rc, TRUST_BDF, depth + 1, didhash,
		    promtree, mod);
	else
		err = pci_children_instantiate(fn,
		    din, board, bridge, rc - TO_PCI, TRUST_BDF, depth + 1,
		    didhash, promtree, mod);
	return (err);
}

static int
declare_dev_and_fn(tnode_t *bus, tnode_t **dev, di_node_t din,
    int board, int bridge, int rc, int devno, int fnno, int depth,
    did_hash_t *didhash, di_prom_handle_t promtree, topo_mod_t *mod)
{
	tnode_t *fn;
	uint_t class, subclass;
	int err;

	if (*dev == NULL) {
		if (rc >= 0)
			*dev = pciexdev_declare(bus, din, devno, didhash,
			    promtree, mod);
		else
			*dev = pcidev_declare(bus, din, devno, didhash,
			    promtree, mod);
		if (*dev == NULL)
			return (-1);
	}
	if (rc >= 0)
		fn = pciexfn_declare(*dev, din, fnno, didhash, promtree, mod);
	else
		fn = pcifn_declare(*dev, din, fnno, didhash, promtree, mod);
	if (fn == NULL)
		return (-1);
	if (pci_classcode_get(didhash, din, &class, &subclass) < 0)
		return (-1);
	if (class == PCI_CLASS_BRIDGE && subclass == PCI_BRIDGE_PCI)
		err = pci_bridge_declare(fn, din, board, bridge, rc, depth,
		    didhash, promtree, mod);
	else
		err = pcifn_enum(mod, fn);
	if (err < 0)
		return (-1);
	else
		return (0);
}

int
pci_children_instantiate(tnode_t *parent, di_node_t pn,
    int board, int bridge, int rc, int bover, int depth, did_hash_t *didhash,
    di_prom_handle_t promtree, topo_mod_t *mod)
{
	did_t *pps[MAX_PCIBUS_DEVS][MAX_PCIDEV_FNS];
	did_t *bp = NULL;
	did_t *np;
	di_node_t sib;
	di_node_t din;
	tnode_t *bn = NULL;
	tnode_t *dn = NULL;
	int pb = -1;
	int b, d, f;

	for (d = 0; d < MAX_PCIBUS_DEVS; d++)
		for (f = 0; f < MAX_PCIDEV_FNS; f++)
			pps[d][f] = NULL;

	/* start at the parent's first sibling */
	sib = di_child_node(pn);
	while (sib != DI_NODE_NIL) {
		np = did_create(didhash, sib, board, bridge, rc, bover,
		    promtree);
		if (np == NULL)
			return (-1);
		did_BDF(np, &b, &d, &f);
		pps[d][f] = np;
		if (bp == NULL)
			bp = np;
		if (pb < 0)
			pb = ((bover == TRUST_BDF) ? b : bover);
		sib = di_sibling_node(sib);
	}
	if (pb < 0 && bover < 0)
		return (0);
	if (rc >= 0)
		bn = pciexbus_declare(parent, pn, ((pb < 0) ? bover : pb),
		    didhash, promtree, mod);
	else
		bn = pcibus_declare(parent, pn, ((pb < 0) ? bover : pb),
		    didhash, promtree, mod);
	if (bn == NULL)
		return (-1);
	if (pb < 0)
		return (0);

	for (d = 0; d < MAX_PCIBUS_DEVS; d++) {
		for (f = 0; f < MAX_PCIDEV_FNS; f++) {
			if (pps[d][f] == NULL)
				continue;
			din = did_dinode(pps[d][f]);
			if ((declare_dev_and_fn(bn,
			    &dn, din, board, bridge, rc, d, f, depth,
			    didhash, promtree, mod)) != 0)
				return (-1);
			did_rele(pps[d][f]);
		}
		dn = NULL;
	}
	return (0);
}

static int
pciexbus_enum(tnode_t *ptn,
    char *pnm, topo_instance_t min, topo_instance_t max,
    di_prom_handle_t promtree, topo_mod_t *mod)
{
	di_node_t pdn;
	int rc;
	int retval;
	did_hash_t *didhash;

	/*
	 * PCI-Express; root complex shares the hostbridge's instance
	 * number.  Parent node's private data is a simple di_node_t
	 * and we have to construct our own did hash and did_t.
	 */
	rc = topo_node_instance(ptn);

	if ((pdn = topo_node_private(ptn)) == DI_NODE_NIL) {
		topo_mod_dprintf(mod,
		    "Parent %s node missing private data.\n"
		    "Unable to proceed with %s enumeration.\n",
		    pnm, PCIEX_BUS);
		return (0);
	}
	if ((didhash = did_hash_init(mod)) == NULL ||
	    (did_create(didhash, pdn, 0, 0, rc, TRUST_BDF, promtree) == NULL))
		return (-1);	/* errno already set */
	retval = pci_children_instantiate(ptn,
	    pdn, 0, 0, rc, (min == max) ? min : TRUST_BDF, 0, didhash,
	    promtree, mod);
	did_hash_fini(didhash);
	return (retval);
}

static int
pcibus_enum(tnode_t *ptn, char *pnm, topo_instance_t min, topo_instance_t max,
    di_prom_handle_t promtree, topo_mod_t *mod)
{
	did_t *hbdid, *didp;
	int retval;
	did_hash_t *didhash;

	/*
	 * PCI Bus; Parent node's private data is a did_t.  We'll
	 * use the did hash established by the parent.
	 */
	if ((hbdid = topo_node_private(ptn)) == NULL) {
		topo_mod_dprintf(mod,
		    "Parent %s node missing private data.\n"
		    "Unable to proceed with %s enumeration.\n",
		    pnm, PCIEX_BUS);
		return (0);
	}
	didhash = did_hash(hbdid);

	/*
	 * If we're looking for a specific bus-instance, find the right
	 * did_t in the chain, otherwise, there should be only one did_t.
	 */
	if (min == max) {
		int b;
		didp = hbdid;
		while (didp != NULL) {
			did_BDF(didp, &b, NULL, NULL);
			if (b == min)
				break;
			didp = did_link_get(didp);
		}
		if (didp == NULL) {
			topo_mod_dprintf(mod,
			    "Parent %s node missing private data related\n"
			    "to %s instance %d.\n", pnm, PCI_BUS, min);
			return (0);
		}
	} else {
		assert(did_link_get(hbdid) == NULL);
		didp = hbdid;
	}
	retval = pci_children_instantiate(ptn, did_dinode(didp),
	    did_board(didp), did_bridge(didp), did_rc(didp),
	    (min == max) ? min : TRUST_BDF, 0, didhash, promtree, mod);
	return (retval);
}

/*ARGSUSED*/
static int
pci_enum(topo_mod_t *mp, tnode_t *ptn, const char *name,
    topo_instance_t min, topo_instance_t max, void *notused)
{
	char *pnm;
	int retval;
	di_prom_handle_t promtree;

	topo_mod_dprintf(mp, "Enumerating pci!\n");

	if ((promtree = di_prom_init()) == DI_PROM_HANDLE_NIL) {
		topo_mod_dprintf(mp,
		    "Pcibus enumerator: di_prom_handle_init failed.\n");
		return (-1);
	}

	pnm = topo_node_name(ptn);
	if (strcmp(pnm, HOSTBRIDGE) != 0 && strcmp(pnm, PCIEX_ROOT) != 0) {
		topo_mod_dprintf(mp,
		    "Currently can only enumerate a %s or %s directly\n",
		    PCI_BUS, PCIEX_BUS);
		topo_mod_dprintf(mp,
		    "descended from a %s or %s node.\n",
		    HOSTBRIDGE, PCIEX_ROOT);
		di_prom_fini(promtree);
		return (0);
	}

	if (strcmp(name, PCI_BUS) == 0) {
		retval = pcibus_enum(ptn, pnm, min, max, promtree, mp);
	} else if (strcmp(name, PCIEX_BUS) == 0) {
		retval = pciexbus_enum(ptn, pnm, min, max, promtree, mp);
	} else {
		topo_mod_dprintf(mp,
		    "Currently only know how to enumerate %s or %s not %s.\n",
		    PCI_BUS, PCIEX_BUS, name);
		di_prom_fini(promtree);
		return (0);
	}
	di_prom_fini(promtree);
	return (retval);
}

/*ARGSUSED*/
static void
pci_release(topo_mod_t *mp, tnode_t *node)
{
	topo_method_unregister_all(mp, node);

	/*
	 * node private data (did_t) for this node is destroyed in
	 * did_hash_destroy()
	 */

	topo_node_unbind(node);
}
