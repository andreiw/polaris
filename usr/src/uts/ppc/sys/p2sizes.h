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


#ifndef _P2SIZES_H
#define	_P2SIZES_H

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Symbolic names for powers of 2.
 * Used for common sizes, just to make them more human-readable.
 * For example, page sizes.
 *
 * There ought not to be any hard-coded page sizes,
 * but it is an imperfect world, so if we commit the sin
 * of hard-coding a number, at least we try to make it
 * more human-readable.
 *
 */

#define	B1K   0x0400
#define	B2K   0x0800
#define	B4K   0x1000
#define	B8K   0x2000
#define	B16K  0x4000
#define	B32K  0x8000
#define	B64K  0x00010000
#define	B128K 0x00020000
#define	B256K 0x00040000
#define	B512K 0x00080000
#define	B1M   0x00100000
#define	B2M   0x00200000
#define	B4M   0x00400000
#define	B8M   0x00800000
#define	B16M  0x01000000
#define	B32M  0x02000000
#define	B64M  0x04000000
#define	B128M 0x08000000
#define	B256M 0x10000000
#define	B512M 0x20000000
#define	B1G   0x40000000
#define	B2G   0x80000000
#define	B4G   0x000100000000
#define	B8G   0x000200000000
#define	B16G  0x000400000000
#define	B32G  0x000800000000
#define	B64G  0x001000000000
#define	B128G 0x002000000000
#define	B256G 0x004000000000
#define	B512G 0x008000000000
#define	B1T   0x010000000000
#define	B2T   0x020000000000
#define	B4T   0x040000000000
#define	B8T   0x080000000000
#define	B16T  0x100000000000
#define	B32T  0x200000000000
#define	B64T  0x400000000000
#define	B128T 0x800000000000
#define	B256T 0x0001000000000000
#define	B512T 0x0002000000000000
#define	B1P   0x0004000000000000
#define	B2P   0x0008000000000000
#define	B4P   0x0010000000000000
#define	B8P   0x0020000000000000
#define	B16P  0x0040000000000000
#define	B32P  0x0080000000000000
#define	B64P  0x0100000000000000
#define	B128P 0x0200000000000000
#define	B256P 0x0400000000000000
#define	B512P 0x0800000000000000
#define	B1E   0x1000000000000000
#define	B2E   0x2000000000000000
#define	B4E   0x4000000000000000
#define	B8E   0x8000000000000000

#ifdef __cplusplus
}
#endif

#endif /* _P2SIZES_H */
