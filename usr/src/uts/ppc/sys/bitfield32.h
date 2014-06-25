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

#ifndef _BITFIELD32_H
#define	_BITFIELD32_H

#pragma ident	"@(#)prototype.h	1.14	06/02/08 SMI"
#pragma ident	"%Z%%M%	%I%	%E% SMI"

/*
 * Generic field accessor macros.
 *
 * Usage of terms in these macros:
 *
 *  val refers to an unshifted value, that is one that is directly usable
 *	for most calculations.
 *  fld refers to a value that is suitably shifted and masked so that it
 *	fits into its proper place as a bit field.
 *
 * fields are converted to values by extracting them.
 * values are converted to fields by depositing them.
 *
 * Bit positions are 0..31, from least-significant to most-significant.
 * Note that these macros use lo-to-high bit ordering, which is most
 * directly translatable into shift amounts.  This is in spite of the
 * fact that PowerPC documentation uses hi-to-lo bit numbering.
 * The notation used to produce .h files from .fd files avoids using
 * bit numbering altogether; fields are just listed in order, with
 * field lengths, and all fields including unnamed reserved fields
 * must be accounted for.  These accessor macros cover up the details
 * of bit numbering.
 *
 * VALMASK32:
 *   Given:
 *	A length, <len>
 *   Result:
 *	A mask suitable for masking off all but the low order <len> bits.
 *
 * FLDMASK32:
 *   Given:
 *	1) a bit position, <pos>, and
 *	2) a field length, <len>
 *   Result:
 *	A mask that is properly positioned and of proper length to mask
 *	off all bits except those occupying the field.
 *	The 1-s complement of this mask can be used to mask out the field
 *	itself, leaving everything else intact.
 *
 * VALTOFLD32:
 *   Given:
 *	1) a value,
 *	2) a bit position, and
 *	3) a length
 *   Result:
 *	A 32-bit word containing the given value properly positioned and
 *	masked for the specified bitfield.  The rest of the bits are zero.
 *
 * EXTRACT32:
 *   Given:
 *	1) a whole 32-bit word, possibly containing many fields,
 *	2) a bit position, and
 *	3) a length
 *   Result:
 *	The value of the specified field (position and length) extracted
 *	from the whole 32-bit word.
 *   Comment:
 *	On PowerPC, this should generate a single rlwinm instruction,
 *	unless extra debugging is enabled.
 *
 * DEPOSIT32:
 *   Given:
 *	1) a whole 32-bit word, possibly containing many fields,
 *	2) a value to be deposited into the specified field,
 *	3) a bit position, and
 *	4) a length
 *   Result:
 *	A 32-bit word with the given value deposited in place,
 *	and all other fields intact.
 *   Comment:
 *	On PowerPC, this should generate a single rlwimi instruction,
 *	unless extra debugging is enabled.  The term "deposit" comes from
 *	PA-RISC, IA-64, and other processors; PowerPC documentation
 *	uses the term "insert".
 *
 * Note: Although the term 'deposit' (or 'insert') may lead you
 * to believe that a word is actually being modified, none of these
 * macros modify any of their arguments; they merely yield values.
 * Any actual modification of a value would have to be done by
 * an explicit assignment.
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

#define	VALMASK32(len)		(((uint_t)1 << (len)) - 1)
#define	FLDMASK32(pos, len)	(VALMASK32(len) << (pos))

#if defined(XXX_DEBUG_BITFIELDS)

#define	VALTOFLD32(val, pos, len)	\
	((ASSERT(LINT_VARIABLE(val) <= VALMASK32(len))), \
	((((val) & VALMASK32(len)) << (pos))))

#else /* !defined(XXX_DEBUG_BITFIELDS) */

#define	VALTOFLD32(val, pos, len) \
	(((((val) & VALMASK32(len)) << (pos))))

#endif /* defined(XXX_DEBUG_BITFIELDS) */

#define	VALTOFLD32C(val, pos, len) \
	(((((val) & VALMASK32(len)) << (pos))))

#define	EXTRACT32(whole, pos, len) \
	((((whole)) >> ((pos))) & VALMASK32(len))

#define	DEPOSIT32(whole, val, pos, len) \
	(((whole) & ~FLDMASK32((pos), (len))) \
	| VALTOFLD32((val), (pos), (len)))


#ifdef __cplusplus
}
#endif

#endif /* _BITFIELD32_H */
