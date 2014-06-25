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

/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"%Z%%M%	%I%	%E% SMI"

/* LINTLIBRARY */

#if	defined(_KERNEL)
#include	"reloc.h"
#else
#include	"sgs.h"
#include	"machdep.h"
#include	"libld.h"
#include	"reloc.h"
#include	"conv.h"
#include	"msg.h"
#endif

/* 
 * This was copied from 2.6/uts/common/krtld/reloc.h where they should be, 
 * temporary for fixing eprintf error and eprintf should be later replaced with 
 * the macros defined in reloc.h
 *
 * This converts the sgs eprintf() routine into the _printf()
 * as used by krtld.
 */
#define eprintf         _kobj_printf
#define ERR_FATAL       ops

/* import MSG_REL_LOOSEBITS macros from 2.6 for an error*/
#define MSG_REL_LOOSEBITS       "relocation error: %s: file %s: symbol %s:" \
                                " value 0x%x looses %d bits at offset 0x%x"

/*  
 * Copied from 2.6/uts/common/krtld/reloc.h to go thorough the build, temporarily
 */
#define FLG_RE_RELPC            0x00000004
#define FLG_RE_HA               0x00000100  /* HIGH ADJUST (ppc) */
#define FLG_RE_BRTAK            0x00000200  /* BRTAKEN (ppc) */
#define FLG_RE_BRNTAK           0x00000400  /* BRNTAKEN (ppc) */
#define FLG_RE_SDAREL           0x00000800  /* SDABASE REL (ppc) */
#define FLG_RE_SECTREL          0x00001000  /* SECTOFF relative */
/* end souring in from 2.6 */

/*
 * This table represents the current relocations that do_reloc() is able to
 * process.  The relocations below that are marked SPECIAL are relocations that
 * take special processing and shouldn't actually ever be passed to do_reloc().
 */
const Rel_entry	reloc_table[R_PPC_NUM] = {
/* R_PPC_NONE */	{0, 0, 0, 0, 0},
/* R_PPC_ADDR32 */	{FLG_RE_NOTREL, 4, 0, 0, 0},
/* R_PPC_ADDR24 */	{FLG_RE_NOTREL | FLG_RE_VERIFY, 4, 2, 24, 2},
/* R_PPC_ADDR16 */	{FLG_RE_NOTREL | FLG_RE_VERIFY, 2, 0, 0, 0},
/* R_PPC_ADDR16_LO */	{FLG_RE_NOTREL, 2, 0, 16, 0},
/* R_PPC_ADDR16_HI */	{FLG_RE_NOTREL, 2, 16, 16, 0},
/* R_PPC_ADDR16_HA */	{FLG_RE_NOTREL | FLG_RE_HA, 2, 16, 16, 0},
/* R_PPC_ADDR14 */	{FLG_RE_NOTREL | FLG_RE_VERIFY, 4, 2, 14, 2},
/* R_PPC_ADDR14_BRTAKEN */
			{FLG_RE_NOTREL | FLG_RE_VERIFY |
				FLG_RE_BRTAK, 4, 2, 14, 2},
/* R_PPC_ADDR14_BRNTAKEN */
			{FLG_RE_NOTREL | FLG_RE_VERIFY |
				FLG_RE_BRNTAK, 4, 2, 14, 2},
/* R_PPC_REL24 */	{FLG_RE_PCREL | FLG_RE_VERIFY | FLG_RE_SIGN,
				4, 2, 24, 2},
/* R_PPC_REL14 */	{FLG_RE_PCREL | FLG_RE_VERIFY | FLG_RE_SIGN,
				4, 2, 14, 2},
/* R_PPC_REL14_BRTAKEN */
			{FLG_RE_PCREL | FLG_RE_VERIFY | FLG_RE_SIGN |
				FLG_RE_BRTAK, 4, 2, 14, 2},
/* R_PPC_REL14_BRNTAKEN */
			{FLG_RE_PCREL | FLG_RE_VERIFY | FLG_RE_SIGN |
				FLG_RE_BRNTAK, 4, 2, 14, 2},
/* R_PPC_GOT16 */	{FLG_RE_GOTREL | FLG_RE_VERIFY | FLG_RE_SIGN,
				2, 0, 0, 0},
/* R_PPC_GOT16_LO */	{FLG_RE_GOTREL, 2, 0, 16, 0},
/* R_PPC_GOT16_HI */	{FLG_RE_GOTREL, 2, 16, 16, 0},
/* R_PPC_GOT16_HA */	{FLG_RE_GOTREL | FLG_RE_HA, 2, 16, 16, 0},
/* R_PPC_PLTREL24 */	{FLG_RE_PLTREL | FLG_RE_PCREL |
				FLG_RE_VERIFY | FLG_RE_SIGN, 4, 2, 24, 2},
/* R_PPC_COPY */	{0, 0, 0, 0, 0},			/* SPECIAL */
/* R_PPC_GLOBDAT */	{FLG_RE_NOTREL, 4, 0, 0, 0},
/* R_PPC_JMP_SLOT */	{0, 0, 0, 0, 0},			/* SPECIAL */
/* R_PPC_RELATIVE */	{FLG_RE_NOTREL, 4, 0, 0, 0},
/* R_PPC_LOCAL24PC */	{FLG_RE_RELPC | FLG_RE_PCREL | FLG_RE_VERIFY |
				FLG_RE_SIGN,
				4, 2, 24, 2},
/* R_PPC_UADDR32 */	{FLG_RE_NOTREL | FLG_RE_UNALIGN, 4, 0, 0, 0},
/* R_PPC_UADDR16 */	{FLG_RE_NOTREL | FLG_RE_UNALIGN, 2, 0, 0, 0},
/* R_PPC_REL32 */	{FLG_RE_PCREL, 4, 0, 0, 0},
/* R_PPC_PLT32 */	{FLG_RE_PLTREL, 4, 0, 0, 0},
/* R_PPC_PLTREL32 */	{FLG_RE_PLTREL | FLG_RE_PCREL, 4, 0, 0, 0},
/* R_PPC_PLT16_LO */	{FLG_RE_PLTREL, 2, 0, 16, 0},
/* R_PPC_PLT16_HI */	{FLG_RE_PLTREL, 2, 16, 16, 0},
/* R_PPC_PLT16_HA */	{FLG_RE_PLTREL | FLG_RE_HA, 2, 16, 16, 0},
/* R_PPC_SDAREL16 */	{FLG_RE_SDAREL | FLG_RE_VERIFY, 2, 0, 0, 0},
/* R_PPC_SECTOFF */	{FLG_RE_SECTREL | FLG_RE_VERIFY, 2, 0, 0, 0},
/* R_PPC_SECTOFF_LO */	{FLG_RE_SECTREL, 2, 0, 16, 0},
/* R_PPC_SECTOFF_HI */	{FLG_RE_SECTREL, 2, 16, 16, 0},
/* R_PPC_SECTOFF_HA */	{FLG_RE_SECTREL | FLG_RE_HA, 2, 16, 16, 0},
/* R_PPC_ADDR30 */	{FLG_RE_PCREL | FLG_RE_SIGN, 4, 2, 30, 2}
};


/*
 * Write a single relocated value to its reference location.
 * We assume we wish to add the relocation amount, value, to the
 * the value of the address already present in the instruction, as
 * specified by the pair (isect, off), the reference location.
 *
 * NAME				VALUE	FIELD		CALCULATION
 *
 * R_PPC_NONE			0	none		none
 * R_PPC_ADDR32			1	word32		S + A
 * R_PPC_ADDR24			2	low24		(S + A) >> 2
 * R_PPC_ADDR16			3	half16		S + A
 * R_PPC_ADDR16_LO		4	half16		#lo(S + A)
 * R_PPC_ADDR16_HI		5	half16		#hi(S + A)
 * R_PPC_ADDR16_HA		6	half16		#ha(S + A)
 * R_PPC_ADDR14			7	low14		(S + A) >> 2
 * R_PPC_ADDR14_BRTAKEN		8	low14		(S + A) >> 2
 * R_PPC_ADDR14_BRNTAKEN	9	low14		(S + A) >> 2
 * R_PPC_REL24			10	low24		(S + A - P) >> 2
 * R_PPC_REL14			11	low14		(S + A - P) >> 2
 * R_PPC_REL14_BRTAKEN		12	low14		(S + A - P) >> 2
 * R_PPC_REL14_BRNTAKEN		13	low14		(S + A - P) >> 2
 * R_PPC_GOT16			14	half16		G + A
 * R_PPC_GOT16_LO		15	half16		#lo(G + A)
 * R_PPC_GOT16_HI		16	half16		#hi(G + A)
 * R_PPC_GOT16_HA		17	half16		#ha(G + A)
 * R_PPC_PLTREL24		18	low24		(L + A - P) >> 2
 * R_PPC_COPY			19	none		none
 * R_PPC_GLOB_DAT		20	word32		S + A
 * R_PPC_JMP_SLOT		21	none		see below
 * R_PPC_RELATIVE		22	word32		B + A
 * R_PPC_LOCAL24PC		23	low24		see below
 * R_PPC_UADDR32		24	word32		S + A
 * R_PPC_UADDR16		25	half16		S + A
 * R_PPC_REL32			26	word32		S + A - P
 * R_PPC_PLT32			27	word32		L + A
 * R_PPC_PLTREL32		28	word32		L + A - P
 * R_PPC_PLT16_LO		29	half16		#lo(L + A)
 * R_PPC_PLT16_HI		30	half16		#hi(L + A)
 * R_PPC_PLT16_HA		31	half16		#ha(L + A)
 * R_PPC_SDAREL16		32	half16		S + A - _SDA_BASE_
 * R_PPC_SECTOFF		33	half16		R + A
 * R_PPC_SECTOFF_LO		34	half16		#lo(R + A)
 * R_PPC_SECTOFF_HI		35	half16		#hi(R + A)
 * R_PPC_SECTOFF_HA		36	half16		#ha(R + A)
 * R_PPC_ADDR30			37	word30		(S + A - P) >> 2
 *
 *	This is Figure 4-3: Relocation Types from the Draft Copy of
 * the ABI, Printed on 7/25/94.
 *
 *	The field column specifies how much of the data
 * at the reference address is to be used. The data are assumed to be
 * right-justified with the least significant bit at the right.
 *	In the case of plt24 addresses, the reference address is
 * assumed to be that of a 6-word PLT entry. The address is the right-
 * most 24 bits of the third word.
 *
 * The calculations in the CALCULATION column are assumed to have
 * been performed in routine reloc_sec before calling this function
 * except for the addition of the addresses in the instructions.
 *
 * The following notations are used in the calculations fields:
 *
 * A	Represents the addend used to compute the value of the
 *	relocatable field.
 *
 * B	Represents the base address at which a shared object has
 *	been loaded into memory during execution.  Generally, a shared
 *	object file is built with a 0 base virtual address, but
 *	the execution address will be different.  See PROGRAM HEADER
 *	in the System V ABI for more information about the base address.
 *
 * G	Represents the offset into the global offset table at which the
 *	address of the relocation entry's symbol will reside during
 *	execution.  See Coding Examples in Chapter 3 and Global Offset
 *	Table in Chapter 5 for more information.
 *
 * L	Represents the sections offset or address of the procedure
 *	linkage table entry for a symbol.  A procedure linkage table
 *	entry redirects a function call to the proper destination.
 *	The link editor builds the initial procedure linkage
 *	table, and the dynamic linker modifies the entries during
 *	execution.  See Procedure Linkage Table in Chapter 5 for
 *	more information.
 *
 * P	Represents the place (section offset or address) of the storage
 *	unit being relocated (computed using r_offset).
 *
 * R	Represents the offset of the symbol within the section in which
 *	the symbol is defined (its section-relative address).
 *
 * S	Represents the value of the symbol whose index resides in
 *	the relocation entry.
 */
int
do_reloc(uchar_t rtype, uchar_t * off, Xword * value, const char * sym,
	const char * file, void * lml)
{
	long			uvalue = 0;
	Xword			basevalue, sigbit_mask, resshift;
	Xword			corevalue = *value;
	unsigned char		bshift;
	int			field_size, re_flags;
	const Rel_entry *	rep;

	rep = &reloc_table[rtype];
	bshift = rep->re_bshift;
	field_size = rep->re_fsize;
	re_flags = rep->re_flags;
	sigbit_mask = S_MASK(rep->re_sigbits);
	resshift = rep->re_resshift;

	if (field_size == 0) {
		eprintf(ERR_FATAL, MSG_INTL(MSG_REL_UNIMPL), file,
		    (sym ? sym : MSG_INTL(MSG_STR_UNKNOWN)), rtype);
		return (0);
	}

	if (re_flags & FLG_RE_UNALIGN) {
		int		i;
		unsigned char *	dest = (unsigned char *)&basevalue;

		basevalue = 0;
		for (i = field_size - 1; i > 0; i--)
			dest[i] = off[i];
	} else {
		if (((field_size == 2) && ((Xword)off & 0x1)) ||
		    ((field_size == 4) && ((Xword)off & 0x3))) {
			eprintf(ERR_FATAL, MSG_INTL(MSG_REL_NONALIGN),
			    conv_reloc_PPC_type_str(rtype), file,
			    (sym ? sym : MSG_INTL(MSG_STR_UNKNOWN)), off);
			return (0);
		}
		switch (field_size) {
		case 1:
			basevalue = (Xword)*((unsigned char *)off);
			break;
		case 2:
			/* LINTED */
			basevalue = (Xword)*((Half *)off);
			break;
		case 4:
			/* LINTED */
			basevalue = (Xword)*((Xword *)off);
			break;
		default:
			eprintf(ERR_FATAL, MSG_INTL(MSG_REL_UNNOBITS),
			    conv_reloc_PPC_type_str(rtype), file,
			    (sym ? sym : MSG_INTL(MSG_STR_UNKNOWN)),
			    rep->re_fsize * 8);
			return (0);
		}
	}

	if (sigbit_mask) {
		if (resshift) {
			uvalue = (basevalue >> resshift) & sigbit_mask;
			basevalue &= ~(sigbit_mask << resshift);
		} else {
			uvalue = sigbit_mask & basevalue;
			basevalue &= ~sigbit_mask;
		}
	} else
		uvalue = basevalue;

	if (bshift)
		uvalue <<= bshift;

	uvalue += *value;

	if (bshift) {
		/*
		 * This is to check that we are not attempting to
		 * jump to a non-4 byte aligned address.
		 */
		if ((bshift == 2) && (uvalue & 0x3)) {
			eprintf(ERR_FATAL, MSG_INTL(MSG_REL_LOSEBITS),
			    conv_reloc_PPC_type_str(rtype), file,
			    (sym ? sym : MSG_INTL(MSG_STR_UNKNOWN)),
			    uvalue, 2, off);
			return (0);
		}
		if (re_flags & FLG_RE_HA) {
			uvalue = (uvalue >> 16) + ((uvalue & 0x8000) ? 1 : 0);
			corevalue = (corevalue >> 16) +
				((corevalue & 0x8000) ? 1 : 0);
		} else {
			uvalue >>= bshift;
			corevalue >>= bshift;
		}
	}

	if ((re_flags & FLG_RE_VERIFY) && sigbit_mask) {
		if (((re_flags & FLG_RE_SIGN) &&
		    (S_INRANGE(uvalue, rep->re_sigbits - 1) == 0)) ||
		    (!(re_flags & FLG_RE_SIGN) &&
		    ((sigbit_mask & uvalue) != uvalue))) {
			eprintf(ERR_FATAL, MSG_INTL(MSG_REL_NOFIT),
			    conv_reloc_PPC_type_str(rtype), file,
			    (sym ? sym : MSG_INTL(MSG_STR_UNKNOWN)), uvalue);
			return (0);
		}
	}

	if (sigbit_mask) {
		int disp = uvalue;
		uvalue &= sigbit_mask;
		if (resshift)
			uvalue = basevalue | uvalue << resshift;
		else {
			/*
			 * Combine value back with the original word
			 */
			uvalue |= basevalue;
		}

		/*
		 * If either of the BRNTAKEN or BRTAKEN flags are set
		 * then we must either set or clear bit 10, respectivly
		 */
		if (re_flags & FLG_RE_BRTAK) {
			if (disp > 0)
				uvalue |= 0x00200000;
			else
				uvalue &= ~0x00200000;
		} else if (re_flags & FLG_RE_BRNTAK) {
			if (disp < 0)
				uvalue &= ~0x00200000;
			else
				uvalue |= 0x00200000;
		}
	}
	*value = corevalue;

	if (re_flags & FLG_RE_UNALIGN) {
		int		i;
		unsigned char *	src = (unsigned char *)&uvalue;

		for (i = field_size - 1; i > 0; i--)
			off[i] = src[i];
	} else {
		switch (rep->re_fsize) {
		case 1:
			*((unsigned char *)off) = (unsigned char)uvalue;
			break;
		case 2:
			/* LINTED */
			*((Half *)off) = (Half)uvalue;
			break;
		case 4:
			/* LINTED */
			*((unsigned long *)off) = uvalue;
			break;
		}
	}
	return (1);
}
