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

#include	"reloc.h"


#ifdef	KOBJ_DEBUG
static const char *	rels[] = {
		"R_PPC_NONE            ",
		"R_PPC_ADDR32          ",
		"R_PPC_ADDR24          ",
		"R_PPC_ADDR16          ",
		"R_PPC_ADDR16_LO       ",
		"R_PPC_ADDR16_HI       ",
		"R_PPC_ADDR16_HA       ",
		"R_PPC_ADDR14          ",
		"R_PPC_ADDR14_BRTAKEN  ",
		"R_PPC_ADDR14_BRNTAKEN ",
		"R_PPC_REL24           ",
		"R_PPC_REL14           ",
		"R_PPC_REL14_BRTAKEN   ",
		"R_PPC_REL14_BRNTAKEN  ",
		"R_PPC_GOT16           ",
		"R_PPC_GOT16_LO        ",
		"R_PPC_GOT16_HI        ",
		"R_PPC_GOT16_HA        ",
		"R_PPC_PLTREL24        ",
		"R_PPC_COPY            ",
		"R_PPC_GLOB_DAT        ",
		"R_PPC_JMP_SLOT        ",
		"R_PPC_RELATIVE        ",
		"R_PPC_LOCAL24PC       ",
		"R_PPC_UADDR32         ",
		"R_PPC_UADDR16         ",
		"R_PPC_REL32           ",
		"R_PPC_PLT32           ",
		"R_PPC_PLTREL32        ",
		"R_PPC_PLT16_LO        ",
		"R_PPC_PLT16_HI        ",
		"R_PPC_PLT16_HA        ",
		"R_PPC_SDAREL16        ",
		"R_PPC_SECTOFF         ",
		"R_PPC_SECTOFF_LO      ",
		"R_PPC_SECTOFF_HI      ",
		"R_PPC_SECTOFF_HA      ",
		"R_PPC_ADDR30          ",
};
#endif


/*
 * This is a 'stub' of the orignal version defined in liblddbg.so
 * This stub just returns the 'int string' of the relocation in question
 * instead of converting it to it's full syntax.
 */
const char *
conv_reloc_PPC_type_str(Word rtype)
{
#ifdef	KOBJ_DEBUG
	if (rtype < R_PPC_NUM)
		return (rels[rtype]);
	else {
#endif
		static char 	strbuf[32];
		int		ndx = 31;
		strbuf[ndx--] = '\0';
		do {
			strbuf[ndx--] = '0' + (rtype % 10);
			rtype = rtype / 10;
		} while ((ndx >= (int)0) && (rtype > (Word)0));
		return (&strbuf[ndx + 1]);
#ifdef	KOBJ_DEBUG
	}
#endif
}
