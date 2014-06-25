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
 * All rights reserved.  Use is subject to license terms.
 */

#ifndef _SYS_ELF_PPC64_H
#define	_SYS_ELF_PPC64_H

#pragma ident	"%Z%%M%	%I%	%E%"	/* "@(#)elf_ppc64.h" */

#ifdef	__cplusplus
extern "C" {
#endif

#define	R_PPC64_NONE			0	/* relocation types */
#define	R_PPC64_ADDR32			1
#define	R_PPC64_ADDR24			2
#define	R_PPC64_ADDR16			3
#define	R_PPC64_ADDR16_LO		4
#define	R_PPC64_ADDR16_HI		5
#define	R_PPC64_ADDR16_HA		6
#define	R_PPC64_ADDR14			7
#define	R_PPC64_ADDR14_BRTAKEN		8
#define	R_PPC64_ADDR14_BRNTAKEN		9
#define	R_PPC64_REL24			10
#define	R_PPC64_REL14			11
#define	R_PPC64_REL14_BRTAKEN		12
#define	R_PPC64_REL14_BRNTAKEN		13
#define	R_PPC64_GOT16			14
#define	R_PPC64_GOT16_LO		15
#define	R_PPC64_GOT16_HI		16
#define	R_PPC64_GOT16_HA		17
#define	R_PPC64_COPY			19
#define	R_PPC64_GLOB_DAT		20
#define	R_PPC64_JMP_SLOT		21
#define	R_PPC64_RELATIVE		22
#define	R_PPC64_UADDR32			24
#define	R_PPC64_UADDR16			25
#define	R_PPC64_REL32			26
#define	R_PPC64_PLT32			27
#define	R_PPC64_PLTREL32		28
#define	R_PPC64_PLT16_LO		29
#define	R_PPC64_PLT16_HI		30
#define	R_PPC64_PLT16_HA		31
#define	R_PPC64_SECTOFF			33
#define	R_PPC64_SECTOFF_LO		34
#define	R_PPC64_SECTOFF_HI		35
#define	R_PPC64_SECTOFF_HA		36
#define	R_PPC64_ADDR30			37
#define	R_PPC64_ADDR64			38
#define	R_PPC64_ADDR16_HIGHER		39
#define	R_PPC64_ADDR16_HIGHERA		40
#define	R_PPC64_ADDR16_HIGHEST		41
#define	R_PPC64_ADDR16_HIGHESTA		42
#define	R_PPC64_UADDR64			43
#define	R_PPC64_REL64			44
#define	R_PPC64_PLT64			45
#define	R_PPC64_PLTREL64		46
#define	R_PPC64_TOC16			47
#define	R_PPC64_TOC16_LO		48
#define	R_PPC64_TOC16_HI		49
#define	R_PPC64_TOC16_HA		50
#define	R_PPC64_TOC			51
#define	R_PPC64_PLTGOT16		52
#define	R_PPC64_PLTGOT16_LO		53
#define	R_PPC64_PLTGOT16_HI		54
#define	R_PPC64_PLTGOT16_HA		55
#define	R_PPC64_ADDR16_DS		56
#define	R_PPC64_ADDR16_LO_DS		57
#define	R_PPC64_GOT16_DS		58
#define	R_PPC64_GOT16_LO_DS		59
#define	R_PPC64_PLT16_LO_DS		60
#define	R_PPC64_SECTOFF_DS		61
#define	R_PPC64_SECTOFF_LO_DS		62
#define	R_PPC64_TOC16_DS		63
#define	R_PPC64_TOC16_LO_DS		64
#define	R_PPC64_PLTGOT16_DS		65
#define	R_PPC64_PLTGOT16_LO_DS		66
#define	R_PPC64_TLS			67
#define	R_PPC64_DTPMOD64		68
#define	R_PPC64_TPREL16			69
#define	R_PPC64_TPREL16_LO		70
#define	R_PPC64_TPREL16_HI		71
#define	R_PPC64_TPREL16_HA		72
#define	R_PPC64_TPREL64			73
#define	R_PPC64_DTPREL16		74
#define	R_PPC64_DTPREL16_LO		75
#define	R_PPC64_DTPREL16_HI		76
#define	R_PPC64_DTPREL16_HA		77
#define	R_PPC64_DTPREL64		78
#define	R_PPC64_GOT_TLSGD16		79
#define	R_PPC64_GOT_TLSGD16_LO		80
#define	R_PPC64_GOT_TLSGD16_HI		81
#define	R_PPC64_GOT_TLSGD16_HA		82
#define	R_PPC64_GOT_TLSLD16		83
#define	R_PPC64_GOT_TLSLD16_LO		84
#define	R_PPC64_GOT_TLSLD16_HI		85
#define	R_PPC64_GOT_TLSLD16_HA		86
#define	R_PPC64_GOT_TPREL16_DS		87
#define	R_PPC64_GOT_TPREL16_LO_DS	88
#define	R_PPC64_GOT_TPREL16_HI		89
#define	R_PPC64_GOT_TPREL16_HA		90
#define	R_PPC64_GOT_DTPREL16_DS		91
#define	R_PPC64_GOT_DTPREL16_LO_DS	92
#define	R_PPC64_GOT_DTPREL16_HI		93
#define	R_PPC64_GOT_DTPREL16_HA		94
#define	R_PPC64_TPREL16_DS		95
#define	R_PPC64_TPREL16_LO_DS		96
#define	R_PPC64_TPREL16_HIGHER		97
#define	R_PPC64_TPREL16_HIGHERA		98
#define	R_PPC64_TPREL16_HIGHEST		99
#define	R_PPC64_TPREL16_HIGHESTA	100
#define	R_PPC64_DTPREL16_DS		101
#define	R_PPC64_DTPREL16_LO_DS		102
#define	R_PPC64_DTPREL16_HIGHER		103
#define	R_PPC64_DTPREL16_HIGHERA	104
#define	R_PPC64_DTPREL16_HIGHEST	105
#define	R_PPC64_DTPREL16_HIGHESTA	106
#define	R_PPC64_NUM			107

#define ELF_PPC64_MAXPGSZ	0x10000 /* maximum page size */


#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_ELF_PPC64_H */
