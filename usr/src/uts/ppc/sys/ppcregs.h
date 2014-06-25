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
 * Copyright 2005 Cyril Plisko <cyril.plisko@mountall.com>
 */

/*
 * PowerPC registers definition
 */
#ifndef _SYS_PPCREGS_H
#define _SYS_PPCREGS_H

#pragma ident	"XXX OpenSolaris/PPC"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * PowerPC Machine State Register (MSR) 32 bit architecture
 */
#define PPC_MSR_LE	0x00000001UL	/* Little-endian mode enable */
#define PPC_MSR_RI	0x00000002UL	/* Recoverable exception */
#define PPC_MSR_DR	0x00000010UL	/* Data address translation */
#define PPC_MSR_IR	0x00000020UL	/* Instruction address translation */
#define PPC_MSR_IP	0x00000040UL	/* Exception prefix */
#define PPC_MSR_FE1	0x00000100UL	/* Floating-point exception mode 1 */
#define PPC_MSR_BE	0x00000200UL	/* Branch trace enable */
#define PPC_MSR_SE	0x00000400UL	/* Single step trace enable */
#define PPC_MSR_FE0	0x00000800UL	/* Floating-point exception mode 0 */
#define PPC_MSR_ME	0x00001000UL	/* Machine check enable */
#define PPC_MSR_FP	0x00002000UL	/* Floating-point available */
#define PPC_MSR_PR	0x00004000UL	/* Privilege level */
#define PPC_MSR_EE	0x00008000UL	/* External interrupt enable */
#define PPC_MSR_ILE	0x00010000UL	/* Exception little-endian mode */
#define PPC_MSR_POW	0x00040000UL	/* Power Management enable
					   0 Power management disabled
						(normal operation mode)
					   1 Power management enabled
						(reduced power mode )
					 */

#define FMT_PPC_MSR	\
	"\020"		\
	"\023pow\021ile\020ee\017pr\016fp\015me\014fe0"	\
	"\013se\012be\011fe1\007ip\006ir\005dr\002ri\001le"

#ifdef __cplusplus
}
#endif

#endif /* _SYS_PPCREGS_H */
