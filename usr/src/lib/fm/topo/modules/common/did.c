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

#pragma ident	"@(#)did.c	1.5	06/05/09 SMI"

/*
 * did.c
 *	The acronym did means "Dev-Info-Data".  Many properties and
 *	characteristics of topology nodes are, with a bit of coaxing
 *	derived from devinfo nodes.  These routines do some of the
 *	derivation and also encapsulate the discoveries in did_t
 *	structures that get associated with topology nodes as their
 *	"private" data.
 */
#include <alloca.h>
#include <assert.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <libtopo.h>
#include <libnvpair.h>
#include <libdevinfo.h>
#include <sys/pcie.h>

#include "topo_mod.h"
#include "hostbridge.h"
#include "pcibus.h"
#include "did_impl.h"
#include "did_props.h"

static void slotnm_destroy(slotnm_t *);

static slotnm_t *
slotnm_create(topo_mod_t *mp, int dev, char *str)
{
	slotnm_t *p;

	if ((p = topo_mod_alloc(mp, sizeof (slotnm_t))) == NULL)
		return (NULL);
	p->snm_mod = mp;
	p->snm_next = NULL;
	p->snm_dev = dev;
	p->snm_label = topo_mod_strdup(mp, str);
	if (p->snm_label == NULL) {
		slotnm_destroy(p);
		return (NULL);
	}
	return (p);
}

static void
slotnm_destroy(slotnm_t *p)
{
	if (p == NULL)
		return;
	slotnm_destroy(p->snm_next);
	if (p->snm_label != NULL)
		topo_mod_strfree(p->snm_mod, p->snm_label);
	topo_mod_free(p->snm_mod, p, sizeof (slotnm_t));
}

static int
slotnm_cp(did_t *from, did_t *to, int *nslots)
{
	slotnm_t *nxt, *new;
	slotnm_t *last = NULL;

	*nslots = 0;
	for (nxt = from->dp_slotnames; nxt != NULL; nxt = nxt->snm_next) {
		new = slotnm_create(to->dp_mod, nxt->snm_dev, nxt->snm_label);
		if (new == NULL) {
			if (to->dp_slotnames != NULL)
				slotnm_destroy(to->dp_slotnames);
			to->dp_slotnames = NULL;
			*nslots = 0;
			return (-1);
		}
		if (last == NULL) {
			to->dp_slotnames = last = new;
		} else {
			last->snm_next = new;
			last = new;
		}
		(*nslots)++;
	}
	if (*nslots > 0)
		topo_mod_dprintf(to->dp_mod,
		    "%p inherits %d slot label(s) from %p.\n",
		    to, *nslots, from);
	return (0);
}

static int
di_physlotinfo_get(topo_mod_t *mp, di_node_t src, uint_t excap,
    int *slotnum, char **slotnm, di_prom_handle_t promtree)
{
	char *slotbuf;
	int sz;
	uchar_t *buf;

	*slotnum = -1;
	(void) di_uintprop_get(src, DI_PHYSPROP, (uint_t *)slotnum,
	    promtree);
	/*
	 * If no physical slot number property was found, then the
	 * capabilities register may indicate the pci-express device
	 * implements a slot, and we should record which slot.
	 */
	if (*slotnum == -1 && (excap & PCIE_PCIECAP_SLOT_IMPL) != 0) {
		uint_t slotcap;
		int e;
		e = di_uintprop_get(src, "pcie-slotcap-reg", &slotcap,
		    promtree);
		if (e == 0)
			*slotnum = slotcap >> PCIE_SLOTCAP_PHY_SLOT_NUM_SHIFT;
	}
	if (*slotnum == -1)
		return (0);

	/*
	 * For PCI-Express, there is only one downstream device, so check for
	 * a slot-names property, and if it exists, ignore the slotmask value
	 * and use the string as the label.
	 */
	if (di_bytes_get(src, DI_SLOTPROP, &sz, &buf, promtree) == 0 &&
	    sz > 4) {
		slotbuf = (char *)&buf[4];
	} else {
		/*
		 * Make generic description string "SLOT <num>", allow up to
		 * 10 digits for number
		 */
		slotbuf = alloca(16);
		(void) snprintf(slotbuf, 16, "SLOT %d", *slotnum);
	}
	if ((*slotnm = topo_mod_strdup(mp, slotbuf)) == NULL)
		return (-1);

	return (0);
}

static int
di_slotinfo_get(topo_mod_t *mp, di_node_t src, int *nslots, slotnm_t **slots,
    di_prom_handle_t promtree)
{
	slotnm_t *lastslot = NULL;
	slotnm_t *newslot;
	uchar_t *slotbuf;
	uint_t slotmap = 0;
	char *slotname;
	int andbit;
	int sz = -1;

	*slots = NULL;
	*nslots = 0;
	if (di_bytes_get(src, DI_SLOTPROP, &sz, &slotbuf, promtree) < 0)
		return (0);
	if (sz < sizeof (uint_t))
		return (0);
	bcopy(slotbuf, &slotmap, sizeof (uint_t));
	if (slotmap == 0)
		return (0);

	slotname = (char *)&slotbuf[4];
	for (andbit = 0; andbit < 32; andbit++) {
		if (slotmap & (1 << andbit)) {
			char *s = slotname;
			slotname += strlen(s) + 1;
			if ((newslot = slotnm_create(mp, andbit, s)) == NULL) {
				slotnm_destroy(*slots);
				*slots = NULL;
				*nslots = 0;
				return (-1);
			}
			if (lastslot == NULL)
				*slots = lastslot = newslot;
			else {
				lastslot->snm_next = newslot;
				lastslot = newslot;
			}
			(*nslots)++;
		}
	}
	return (0);
}

int
did_physslot(did_t *did)
{
	assert(did != NULL);
	return (did->dp_physlot);
}


did_hash_t *
did_hash(did_t *did)
{
	assert(did != NULL);
	return (did->dp_hash);
}

did_t *
did_create(did_hash_t *dhash, di_node_t src,
    int ibrd, int ibrdge, int irc, int ibus, di_prom_handle_t promtree)
{
	topo_mod_t *mp;
	did_t *np;
	did_t *pd;
	uint_t code;
	uint_t reg;

	mp = dhash->dph_mod;
	if ((pd = did_hash_lookup(dhash, src)) != NULL) {
		topo_mod_dprintf(mp, "Attempt to create existing did_t.\n");
		assert(ibus == TRUST_BDF || (pd->dp_bus == ibus));
		return (pd);
	}

	if ((np = topo_mod_zalloc(mp, sizeof (did_t))) == NULL)
		return (NULL);
	np->dp_mod = mp;
	np->dp_src = src;
	np->dp_hash = dhash;

	/*
	 * We must have a reg prop and from it we extract the bus #,
	 * device #, and function #.
	 */
	if (di_uintprop_get(src, DI_REGPROP, &reg, promtree) < 0) {
		topo_mod_free(mp, np, sizeof (did_t));
		return (NULL);
	}
	np->dp_board = ibrd;
	np->dp_bridge = ibrdge;
	np->dp_rc = irc;
	if (ibus == TRUST_BDF)
		np->dp_bus = PCI_REG_BUS_G(reg);
	else
		np->dp_bus = ibus;
	np->dp_dev = PCI_REG_DEV_G(reg);
	np->dp_fn = PCI_REG_FUNC_G(reg);
	np->dp_bdf = (PCI_REG_BUS_G(reg) << 8) | (PCI_REG_DEV_G(reg) << 3) |
	    PCI_REG_FUNC_G(reg);
	/*
	 * There *may* be a class code we can capture.  If there wasn't
	 * one, capture that fact by setting the class value to -1.
	 */
	if (di_uintprop_get(src, DI_CCPROP, &code, promtree) == 0) {
		np->dp_class = GETCLASS(code);
		np->dp_subclass = GETSUBCLASS(code);
	} else {
		np->dp_class = -1;
	}
	/*
	 * There *may* be a PCI-express capabilities register we can capture.
	 * If there wasn't one, the capabilities will be the out-of-bounds
	 * value of zero.
	 */
	(void) di_uintprop_get(src, "pcie-capid-reg", &np->dp_excap,
	    promtree);
	/*
	 * There *may* be a physical slot number property we can capture.
	 */
	if (di_physlotinfo_get(mp,
	    src, np->dp_excap, &np->dp_physlot, &np->dp_physlot_label,
	    promtree) < 0) {
		topo_mod_free(mp, np, sizeof (did_t));
		return (NULL);
	}
	/*
	 * There *may* be PCI slot info we can capture
	 */
	if (di_slotinfo_get(mp, src, &np->dp_nslots, &np->dp_slotnames,
	    promtree) < 0) {
		if (np->dp_physlot_label != NULL)
			topo_mod_strfree(mp, np->dp_physlot_label);
		topo_mod_free(mp, np, sizeof (did_t));
		return (NULL);
	}
	did_hash_insert(dhash, src, np);
	did_hold(np);
	return (np);
}

did_t *
did_link_get(did_t *dp)
{
	assert(dp != NULL);
	return (dp->dp_link);
}

did_t *
did_chain_get(did_t *dp)
{
	assert(dp != NULL);
	return (dp->dp_chain);
}

void
did_link_set(tnode_t *head, did_t *tail)
{
	did_t *hd, *pd;

	assert(head != NULL);
	pd = hd = topo_node_private(head);
	assert(hd != NULL);
	while ((hd = did_link_get(hd)) != NULL)
		pd = hd;
	pd->dp_link = tail;
	tail->dp_link = NULL;
}

void
did_did_link_set(did_t *from, did_t *to)
{
	assert(from != NULL && to != NULL);
	from->dp_link = to;
}

void
did_did_chain_set(did_t *from, did_t *to)
{
	assert(from != NULL && to != NULL);
	from->dp_chain = to;
}

void
did_destroy(did_t *dp)
{
	assert(dp != NULL);

	/*
	 * did_destroy() is called only from did_hash_destroy() when
	 * all references to the did_t have been released.  We can
	 * safely destroy the did_t.  If at some later time, more
	 * fine-grained reference count control is desired, this
	 * code will need to change
	 */

	if (dp->dp_physlot_label != NULL)
		topo_mod_strfree(dp->dp_mod, dp->dp_physlot_label);
	slotnm_destroy(dp->dp_slotnames);
	topo_mod_free(dp->dp_mod, dp, sizeof (did_t));
}

void
did_hold(did_t *dp)
{
	assert(dp != NULL);
	dp->dp_refcnt++;
}

void
did_rele(did_t *dp)
{
	assert(dp != NULL);
	assert(dp->dp_refcnt > 0);
	dp->dp_refcnt--;
}

di_node_t
did_dinode(did_t *dp)
{
	assert(dp != NULL);
	assert(dp->dp_src != NULL);
	return (dp->dp_src);
}

topo_mod_t *
did_mod(did_t *dp)
{
	assert(dp != NULL);
	return (dp->dp_mod);
}

void
did_markrc(did_t *dp)
{
	assert(dp != NULL);
	dp->dp_excap |= PCIE_PCIECAP_DEV_TYPE_ROOT;
}

void
did_BDF(did_t *dp, int *bus, int *dev, int *fn)
{
	assert(dp != NULL);
	if (bus != NULL)
		*bus = dp->dp_bus;
	if (dev != NULL)
		*dev = dp->dp_dev;
	if (fn != NULL)
		*fn = dp->dp_fn;
}

int
did_board(did_t *did)
{
	assert(did != NULL);
	return (did->dp_board);
}

int
did_bridge(did_t *did)
{
	assert(did != NULL);
	return (did->dp_bridge);
}

int
did_rc(did_t *did)
{
	assert(did != NULL);
	return (did->dp_rc);
}

static int
did_numlabels(did_t *dp)
{
	assert(dp != NULL);
	return (dp->dp_nslots);
}

int
did_excap(did_t *dp)
{
	assert(dp != NULL);
	return ((int)dp->dp_excap);
}

int
did_bdf(did_t *dp)
{
	assert(dp != NULL);
	return ((int)dp->dp_bdf);
}

const char *
did_label(did_t *dp, int dev)
{
	slotnm_t *slot;

	assert(dp != NULL);
	if (dp->dp_physlot_label != NULL)
		return (dp->dp_physlot_label);
	for (slot = dp->dp_slotnames; slot != NULL; slot = slot->snm_next)
		if (slot->snm_dev == dev)
			break;
	if (slot != NULL)
		return (slot->snm_label);
	return (NULL);
}

did_t *
did_find(did_hash_t *dhash, di_node_t dn)
{
	return (did_hash_lookup(dhash, dn));
}

int
pci_BDF_get(did_hash_t *dhash, di_node_t dn, int *bus, int *dev, int *fn)
{
	did_t *dp;

	if ((dp = did_find(dhash, dn)) == NULL)
		return (-1);
	*bus = dp->dp_bus;
	*dev = dp->dp_dev;
	*fn = dp->dp_fn;
	did_rele(dp);
	return (0);
}

int
pci_classcode_get(did_hash_t *dhash,
    di_node_t dn, uint_t *class, uint_t *sub)
{
	did_t *dp;

	if ((dp = did_find(dhash, dn)) == NULL)
		return (-1);
	if (dp->dp_class < 0) {
		did_rele(dp);
		return (-1);
	}
	*class = dp->dp_class;
	*sub = dp->dp_subclass;
	did_rele(dp);
	return (0);
}

int
pciex_cap_get(did_hash_t *dhash, di_node_t dn)
{
	did_t *dp;

	if ((dp = did_find(dhash, dn)) == NULL)
		return (-1);
	did_rele(dp);
	return (dp->dp_excap);
}

int
did_inherit(did_t *pdp, did_t *dp)
{
	/*
	 * If the child already has a label, we're done.
	 */
	assert(dp != NULL);
	if (did_numlabels(dp) > 0)
		return (0);

	assert(pdp != NULL);

	/*
	 * If the child and parent are the same, we're done.
	 */
	if (dp == pdp)
		return (0);

	if (pdp->dp_physlot_label != NULL) {
		topo_mod_dprintf(dp->dp_mod,
		    "%p inherits physlot label from %p.\n", dp, pdp);
		dp->dp_physlot_label =
		    topo_mod_strdup(dp->dp_mod, pdp->dp_physlot_label);
		if (dp->dp_physlot_label == NULL)
			return (-1);
	}
	if (slotnm_cp(pdp, dp, &dp->dp_nslots) < 0)
		return (-1);
	return (0);
}
