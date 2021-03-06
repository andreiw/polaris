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
#pragma ident	"@(#)relocate.c	1.15	06/05/09 SMI"

/*
 * String conversion routine for relocation types.
 */
#include	<stdio.h>
#include	"_conv.h"

/*
 * Generic front-end that determines machine specific relocations.
 */
const char *
conv_reloc_type(Half mach, Word type, int fmt_flags)
{
	static char	string[CONV_INV_STRSIZE];

	switch (mach) {
	case EM_386:
		return (conv_reloc_386_type(type, fmt_flags));

	case EM_SPARC:
	case EM_SPARC32PLUS:
	case EM_SPARCV9:
		return (conv_reloc_SPARC_type(type, fmt_flags));

	case EM_AMD64:
		return (conv_reloc_amd64_type(type, fmt_flags));
	}

	/* If didn't match a machine type, use integer value */
	return (conv_invalid_val(string, CONV_INV_STRSIZE, type, fmt_flags));
}
