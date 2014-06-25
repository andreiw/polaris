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
 * Copyright 2006 Sun Microsystems Laboratories.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#include <sys/types.h>
#include <sys/cpuvar.h>		// defines cpu_t, needed for pass2
#include <sys/spr.h>
#include <sys/pvr.h>
#include <sys/ppc_instr.h>
#include <sys/ppc_subr.h>
#include <sys/hid0.h>
#include <sys/cpu_feature.h>
#include <sys/cmn_err.h>	// Import snprintf()
#include <sys/chip.h>

#define	ELEMENTS(array) ((sizeof (array)) / (sizeof ((array)[0])))

uint_t l2cache_sets;
uint_t l2cache_linesz;
uint_t l2cache_assoc;
uint_t l2cache_size;

/*
 * cpu_features_hw is set depending solely on what features the hardware
 * is capable of, according to cpuid_pass1().
 * cpu_features would normally be the same as cpu_features_hw, but some
 * features can be disabled, as far as the kernel is concerned.
 */
uint_t cpu_features_hw = 0;
uint_t cpu_features = 0;
uint_t hid0_writable = 1;
uint_t enable_high_bats = 0;
uint_t hid0;
uint_t hid1;

/*
 * Interpretation of PVR.
 * See MPC7450UM, page 2-12, table 2-2.
 *
 */

static void init_mcp745x(uint_t);

/*
 * Procossor version classes.
 */
#define	PVC_UNKNOWN 0
#define	PVC_MPC745X 1
#define	PVC_IBM750  0
#define	PVC_MPC603X 0
#define	PVC_MPC7400 0

struct pvr {
	ushort_t	pvr_rev_start;
	ushort_t	pvr_rev_end;
	ushort_t	pvr_class;
	ushort_t	pvr_cap;	/* capabilities for this version */
	ushort_t	pvr_l2cache_sets;
	ushort_t	pvr_l2cache_linesz;
	ushort_t	pvr_l2cache_assoc;
	ushort_t	pvr_l2cache_known;
	const char	*pvr_model;
};

struct pvr_extent {
	ushort_t	pvrx_lo_version;
	ushort_t	pvrx_hi_version;
	struct pvr	*pvrx_array;
};

static struct pvr pvr_array_8000[] = {
	/* CSTYLED */
	/* srev    erev    class     cap  sets lsz as  ?  model */
	{ 0x0200, 0xffff, PVC_MPC745X, 0,  512, 64, 8, 1, "MPC7451" },
	{ 0x0100,  0x3ff, PVC_MPC745X, 0,  512, 64, 8, 1, "MPC7455" },
	{ 0x0100,  0x1ff, PVC_MPC745X, 0, 1024, 64, 8, 1, "MPC7457" },
	{ 0x0100,  0x1ff, PVC_MPC745X, 0, 1024, 64, 8, 1, "MPC7447A" },
	{ 0x0100,  0x1ff, PVC_UNKNOWN, 0, 2048, 64, 8, 1, "MPC7448" },
};

static struct pvr pvr_array_0008[] = {
	/* CSTYLED */
	/* srev    erev    class     cap  sets lsz as  ?  model */
	{  0x100,  0x4ff, PVC_MPC603X, 0,  512, 64, 8, 0, "MPC603e" },
	{ 0x1000, 0x1fff, PVC_MPC603X, 0,  512, 64, 8, 0, "MPC603r" },
	{ 0x2000, 0x3fff, PVC_IBM750,  0,  512, 64, 8, 0, "IBM750"  },
	{  0x100, 0x3fff, PVC_UNKNOWN, 0,  512, 64, 8, 0, "MPC7448" },
};


static struct pvr pvr_array_000C[] = {
	/* CSTYLED */
	/* srev    erev    class     cap  sets lsz as  ?  model */
	{ 0x0100, 0x04ff, PVC_MPC7400, 0,  512, 64, 8, 0, "MPC7400" },
};

static struct pvr_extent pvr_table[] = {
	{ 0x0006, 0x0008, &pvr_array_0008[0] },
	{ 0x000C, 0x000C, &pvr_array_000C[0] },
	{ 0x8000, 0x8004, &pvr_array_8000[0] },
};

/*
 * Define which CPU version we are using.
 *
 * Information about known processor versions are kept in tables.
 * Each entry has a range of revision numbers, CPU capabilities,
 * cache geometry, and a descriptive string (model name).
 * The top-level table is not just a collection of entries;
 * rather it is an array of descriptors which refer to extents.
 *
 * An extent is a densely packed series of version numbers,
 * suitable for lookup by direct index.  Each extent is an array
 * of entries describing individual version numbers.  The top-level
 * table is sorted by version number range, in ascending order,
 * and must be maintained that way, in order for searches to succeed.
 *
 * Cache geometry:
 * See MPC7450UM, chapter 3: L1, L2 and L3 Cache Operation,
 * section 3.6.1, L2 Cache Organization:
 *   page 3-49, Figure 3-17, MPC7450
 *   page 3-50, Figure 3-18, MPC7447 and MPC7457
 *   page 3-50, Figure 3-19, MPC7448
 *
 */

uint_t
cpuid_pass1(void)
{
	// XXX extern void *cifp;
	struct pvr *pvr_ent;
	uint_t pvr_val, pir_val, version, revision;
	uint_t i;

	pvr_val = mfspr(SPR_PVR);
	version = PVR_GET_VERSION(pvr_val);
	revision = PVR_GET_REVISION(pvr_val);
	pvr_ent = NULL;
	for (i = 0; i < ELEMENTS(pvr_table); ++i) {
		struct pvr_extent *pvr_ext = &pvr_table[i];
		uint_t lo = pvr_ext->pvrx_lo_version;
		uint_t hi = pvr_ext->pvrx_hi_version;
		if (version >= lo && version <= hi) {
			pvr_ent = pvr_ext->pvrx_array + (version - lo);
			break;
		}
		if (lo > version)
			break;
	}

	prom_printf("Processor Version Reg (PVR) = %x "
		"(version=%x, revision=%x)\n",
		pvr_val, version, revision);

	if (pvr_ent == NULL) {
		prom_printf("Unexpected CPU\n");
		return (0);
	}

	prom_printf("Found Valid CPU: %s\n", pvr_ent->pvr_model);
	if (!(revision >= pvr_ent->pvr_rev_start &&
		revision <= pvr_ent->pvr_rev_end)) {
		prom_printf("WARNING: revision level is not in range %x..%x\n",
			pvr_ent->pvr_rev_start, pvr_ent->pvr_rev_end);
	}

	l2cache_sets = pvr_ent->pvr_l2cache_sets;
	l2cache_linesz = pvr_ent->pvr_l2cache_linesz;
	l2cache_assoc = pvr_ent->pvr_l2cache_assoc;
	l2cache_size = l2cache_sets * l2cache_assoc * l2cache_linesz;
	if (pvr_ent->pvr_l2cache_known == 0) {
		prom_printf("WARNING: "
			"Cache geometry is not known for model %s\n",
			pvr_ent->pvr_model);
		prom_printf("The values chosen are not reliable.\n");
	}
	prom_printf("Cache geometry: sets=%u, linesz=%u, assoc=%u, size=%u\n",
		l2cache_sets, l2cache_linesz, l2cache_assoc, l2cache_size);

	switch (pvr_ent->pvr_class) {
	case PVC_MPC745X:
		init_mcp745x(pvr_ent->pvr_cap);
		break;
	default:
		/*
		 * Processor version is of an unknown class.
		 * Let us not assume that this is fatal.
		 * Just don't try to enable any optional features,
		 * and hope that this processor works with the
		 * least-features behavior of Solaris/PPC.
		 */
		break;
	}

	return (pvr_val);
}

void
cpuid_pass2(cpu_t *cpu)
{
}

/*
 * HID0[HBEN]		Enable additional BAT register numbers.
 * HID0[XBSEN]		Enable extended BAT block sizes.
 * HID0[TBEN]		Enable time base.
 * HID0[XAEN]		Enable extended addressing.
 *
 * Note: HBEN is also known as HIGH_BAT_EN.
 *
 * See: MPC7450UM, Rev. 5, 2005/01, page 2-18..2-22.
 *
 * Note: We would enable extended addressing here, for machines with
 * physical memory greater than 4GBytes, but all BATs and TLBs must
 * be invalidated, and the kernel, boot and firmware rely on BAT mappings.
 * We will think about that, later.
 */

static void
init_mcp745x(uint_t capabilities)
{
	char fmtbuf[256];
	char *bfmt = "%b";
	uint_t hid0_old, hid0_new, hid0_chg;

	cpu_features_hw = CPU_FEATURE_HBEN | CPU_FEATURE_XBSEN;
	if (enable_high_bats)
		cpu_features = cpu_features_hw;
	else
		cpu_features = cpu_features_hw & ~CPU_FEATURE_XBSEN;
	hid0 = mfspr(SPR_HID0);
	hid1 = mfspr(SPR_HID1);
	hid0_old = hid0;
	hid0_new = hid0_old | HID0_TBEN_FLD_MASK;
	if (enable_high_bats)
		hid0_new |=  HID0_HBEN_FLD_MASK | HID0_XBSEN_FLD_MASK;
	hid0_chg = hid0_new & ~hid0_old;
	snprintf(fmtbuf, sizeof (fmtbuf), bfmt, hid0_old, HID0_BFMT);
	prom_printf("HID0_old=0x%x\n", hid0_old);
	prom_printf("        =%s\n", fmtbuf);
	snprintf(fmtbuf, sizeof (fmtbuf), bfmt, hid0_new, HID0_BFMT);
	prom_printf("HID0_new=0x%x\n", hid0_new);
	prom_printf("        =%s\n", fmtbuf);
	snprintf(fmtbuf, sizeof (fmtbuf), bfmt, hid0_chg, HID0_BFMT);
	prom_printf("HID0_chg=0x%x\n", hid0_chg);
	prom_printf("        =%s\n", fmtbuf);

	if (hid0_writable) {
		hid0 = hid0_new;
		ppc_sync();
		mtspr(SPR_HID0, hid0);
		isync();
		/* context_sync(); */
	} else {
		prom_printf("HID0 has not been modified.\n");
		cpu_features = 0;
	}
}

chipid_t
chip_plat_get_chipid(cpu_t *cpu)
{
#if defined(__x86)
	ASSERT(cpuid_checkpass(cpu, 1));

	if (cpuid_is_cmt(cpu))
		return (cpu->cpu_m.mcpu_cpi->cpi_chipid);
	return (cpu->cpu_id);
#else
	return (0);
#endif
}

id_t
chip_plat_get_coreid(cpu_t *cpu)
{
#if defined(__x86)
	ASSERT(cpuid_checkpass(cpu, 1));
	return (cpu->cpu_m.mcpu_cpi->cpi_coreid);
#else
	return (0);
#endif
}
