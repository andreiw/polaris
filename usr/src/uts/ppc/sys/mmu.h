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
 * Copyright (c) 2006 by Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef	_SYS_MMU_H
#define	_SYS_MMU_H

#pragma ident   "%Z%%M% %I%     %E% CP"

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef _ASM
#define	good_addr(a)  (1) 		/* Can map all addresses */
#endif /* !_ASM */

/*
 * Definitions for the PowerPC MMU.
 *
 * The definitions are valid for 32bit implementation of PowerPC architecture.
 */

#if defined(_KERNEL) && !defined(_ASM)
/*
 * Table Search Descriptor Register 1 (SDR1).
 */
#ifdef _BIT_FIELDS_LTOH
struct sdr1 {
	unsigned sdr1_htabmask:9;
	unsigned sdr1_reserved:7;
	unsigned sdr1_htaborg:16;
};
#endif

#ifdef _BIT_FIELDS_HTOL
struct sdr1 {
	unsigned sdr1_htaborg:16;
	unsigned sdr1_reserved:7;
	unsigned sdr1_htabmask:9;
};
#endif

/*
 * Segment Register definition for memory segments.
 */
#ifdef _BIT_FIELDS_LTOH
struct segreg {
	unsigned segreg_vsid:24;	/* virtual segment id */
	unsigned segreg_reserved:5;
	unsigned segreg_ku:1;		/* key bit for user access */
	unsigned segreg_ks:1;		/* key bit for supervisor access */
	unsigned segreg_t:1;
};
#endif

#ifdef _BIT_FIELDS_HTOL
struct segreg {
	unsigned segreg_t:1;
	unsigned segreg_ks:1;		/* key bit for supervisor access */
	unsigned segreg_ku:1;		/* key bit for user access */
	unsigned segreg_reserved:5;
	unsigned segreg_vsid:24;	/* virtual segment id */
};
#endif

typedef union sreg {
	struct	segreg sr;
	u_int	sreg_uint;
} sreg_t;

/*
 * Low level mmu-specific functions
 */
int	valid_va_range(/* basep, lenp, minlen, dir */);

#endif /* defined(_KERNEL) && !defined(_ASM) */

/* bit masks for sdr1 */
#define	SDR1_HTABMASK		0x000001FF
#define	SDR1_HTABORG		0xFFFF0000

#define	SDR1_HTABORG_SHIFT	16

/* bit masks for segment register */
#define	SR_VSID	0x00FFFFFF
#define	SR_KU	0x20000000
#define	SR_KS	0x40000000
#define	SR_T	0x80000000

/* macro to create memory segment register value */
#define	MAKE_SR(v)	(((u_int)v & SR_VSID) | SR_KU)

#ifndef _ASM
/* functions in mmu.c */
extern void mmu_setpte(/* caddr_t base, struct pte pte */);
extern void mmu_getpte(/* caddr_t base, struct pte *ppte */);
extern void mmu_getkpte(/* caddr_t base, struct pte *ppte */);

/*
 * Cache geometry variables.
 */
extern int dcache_size;		/* size of data cache, in bytes */
extern int dcache_blocksize;	/* size of data cache block, in bytes */
extern int icache_size;		/* size of instruction cache, in bytes */
extern int icache_blocksize;	/* size of instruction cache block, in bytes */
extern int unified_cache;	/* non-zero if shared instruction/data cache */

#endif /* !_ASM */

#ifdef	__cplusplus
}
#endif

#endif /* !_SYS_MMU_H */
