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


#pragma ident	"%Z%%M%	%I%	%E% CP"


/*
 * Global include file for all sgs PowerPC machine dependent macros, constants
 * and declarations.
 */
#ifndef	MACHDEP_DOT_H
#define	MACHDEP_DOT_H

#include	<link.h>
#include	"machelf.h"


/*
 * Elf header information.
 */
#define	M_MACH			EM_PPC
#define	M_MACHPLUS		M_MACH
#define	M_CLASS			ELFCLASS32
#if defined(_LITTLE_ENDIAN)
#define	M_DATA			ELFDATA2LSB
#elif defined(_BIG_ENDIAN)
#define	M_DATA			ELFDATA2MSB
#endif
#define	M_FLAGSPLUS_MASK	0
#define	M_FLAGSPLUS		0


/*
 * Page boundary Macros: truncate to previous page boundary and round to
 * next page boundary (refer to generic macros in ../sgs.h also).
 */
#define	M_PTRUNC(X)	((X) & ~(syspagsz - 1))
#define	M_PROUND(X)	(((X) + syspagsz - 1) & ~(syspagsz - 1))

/*
 * Segment boundary macros: truncate to previous segment boundary and round
 * to next page boundary.
 */
#ifndef	M_SEGSIZE
#define	M_SEGSIZE	ELF_PPC_MAXPGSZ
#endif
#define	M_STRUNC(X)	((X) & ~(M_SEGSIZE - 1))
#define	M_SROUND(X)	(((X) + M_SEGSIZE - 1) & ~(M_SEGSIZE - 1))


/*
 * Instruction encodings.
 */
#define	M_LISR11	0x3d600000	/* lis		%r11, symbol@ha	    */
#define	M_LISR12	0x3d800000	/* lis		%r12, symbol@ha	    */
#define	M_LAR11R11	0x396b0000	/* la		%r11, symbo@l(%r11) */
#define	M_LAR12R12	0x398c0000	/* la		%r12, symbo@l(%r12) */
#define	M_MTCTRR11	0x7d6903a6	/* mtctr	%r11		    */
#define	M_MTCTRR12	0x7d8903a6	/* mtctr	%r12		    */
#define	M_BCTR		0x4e800420	/* bctr				    */
#define	M_B		0x48000000	/* b		label		    */
#define	M_BL		0x48000001	/* bl		label		    */
#define	M_NOP		0x60000000	/* nop				    */
#define	M_LIR11		0x39600000	/* li		%r11,imm16	    */
#define	M_BLRL		0x4e800021	/* blrl				    */
#define	M_ORIR11R11	0x616b0000	/* ori		%r11, %r11, imm16   */
#define	M_ORIR12R12	0x618c0000	/* ori		%r12, %r12, imm16   */
#define	M_LWZR11R11	0x816b0000	/* lwz		%r11, symbo@l(%r11) */
#define	M_ADDISR11R11	0x3d6b0000	/* addis	%r11, %r11, imm16   */

#define	M_BIND_ADJ	4	/* adjustment for end of elf_rtbindr address */


/*
 * Plt and Got information; the first few .got and .plt entries are reserved
 *	PLT[0]	jump to dynamic linker
 *	PLT[3]  jump to far routine
 *	GOT[0]	address of _DYNAMIC
 *	GOT[-1] address of a blrl instruction
 */
#define	M_PLT_XRTLD	0	/* plt index for jump to rtld */
#define	M_PLT_XCALL	3	/* plt index for far calls */
#define	M_PLT_XNumber	9	/* # of reserved plt entries */
#define	M_PLT_ENTSIZE	8	/* plt entry size in bytes */
#define	M_PLT_INSSIZE	4	/* single plt instruction size */

#define	M_GOT_XBLRL	-1	/* got index for addres of blrl instruction */
#define	M_GOT_XDYNAMIC	0	/* got index for _DYNAMIC */
#define	M_GOT_XNumber	4	/* reserved # of got entries (2 listed above) */
#define	M_GOT_ENTSIZE	4	/* got entry size in bytes */
#define	M_GOT_MAXSMALL	16384	/* maximum no. of small gots */
#define	M_PLT_MAXSMALL	16384	/* maximum no. of small plts */
	/* transition flags for got sizing */
#define	M_GOT_LARGE	(Word)(-M_GOT_MAXSMALL - 1)
#define	M_GOT_SMALL	(Word)(-M_GOT_MAXSMALL - 2)


/*
 * Other machine dependent entities
 */
#define	M_SEGM_ORIGIN	(Addr) 0x02000000    /* default first segment offset */
#define	M_SEGM_ALIGN	ELF_PPC_MAXPGSZ
#define	M_WORD_ALIGN	4



/*
 * Make machine class dependent functions transparent to the common code
 */
#define	ELF_R_TYPE	ELF32_R_TYPE
#define	ELF_R_INFO	ELF32_R_INFO
#define	ELF_R_SYM	ELF32_R_SYM
#define	ELF_ST_BIND	ELF32_ST_BIND
#define	ELF_ST_TYPE	ELF32_ST_TYPE
#define	ELF_ST_INFO	ELF32_ST_INFO
#define	elf_fsize	elf32_fsize
#define	elf_getehdr	elf32_getehdr
#define	elf_getphdr	elf32_getphdr
#define	elf_newehdr	elf32_newehdr
#define	elf_newphdr	elf32_newphdr
#define	elf_getshdr	elf32_getshdr


/*
 * Make relocation types transparent to the common code
 */
#define	M_REL_DT_TYPE	DT_RELA		/* .dynamic entry */
#define	M_REL_DT_SIZE	DT_RELASZ	/* .dynamic entry */
#define	M_REL_DT_ENT	DT_RELAENT	/* .dynamic entry */
#define	M_REL_SHT_TYPE	SHT_RELA	/* section header type */
#define	M_REL_ELF_TYPE	ELF_T_RELA	/* data buffer type */
#define	M_REL_PREFIX	".rela"		/* section name prefix */
#define	M_REL_TYPE	"rela"		/* mapfile section type */
#define	M_REL_STAB	".rela.stab"	/* stab name prefix and */
#define	M_REL_STABSZ	10		/* 	its string size */

#define	M_REL_CONTYPSTR	conv_reloc_PPC_type_str

/*
 * Make common relocation types transparent to the common code
 */
#define	M_R_NONE	R_PPC_NONE
#define	M_R_GLOB_DAT	R_PPC_GLOB_DAT
#define	M_R_COPY	R_PPC_COPY
#define	M_R_RELATIVE	R_PPC_RELATIVE
#define	M_R_JMP_SLOT	R_PPC_JMP_SLOT

/*
 * Make plt section information transparent to the common code.
 */
#define	M_PLT_SHF_FLAGS	(SHF_ALLOC | SHF_WRITE | SHF_EXECINSTR)

/*
 * Make data segment information transparent to the common code.
 */
#define	M_DATASEG_PERM	(PF_R | PF_W)

/*
 * Define a set of identifies for special sections.  These allow the sections
 * to be ordered within the output file image.
 *
 *  o	null identifies indicate that this section does not need to be added
 *	to the output image (ie. shared object sections or sections we're
 *	going to recreate (sym tables, string tables, relocations, etc.));
 *
 *  o	any user defined section will be first in the associated segment.
 *
 *  o	the hash, dynsym, dynstr and rel's are grouped together as these
 *	will all be accessed first by ld.so.1 to perform relocations.
 *
 *  o	the got, dynamic, and plt are grouped together as these may also be
 *	accessed first by ld.so.1 to perform relocations, fill in DT_DEBUG
 *	(executables only), and .plt[0].
 *
 *  o	unknown sections (stabs and all that crap) go at the end.
 */
#define	M_ID_NULL	0x00
#define	M_ID_USER	0x01

#define	M_ID_INTERP	0x02			/* SHF_ALLOC */
#define	M_ID_HASH	0x03
#define	M_ID_DYNSYM	0x04
#define	M_ID_DYNSTR	0x05
#define	M_ID_VERSION	0x06
#define	M_ID_REL	0x07
#define	M_ID_TEXT	0x08			/* SHF_ALLOC + SHF_EXECINSTR */
#define	M_ID_DATA	0x09

#define	M_ID_GOT	0x02			/* SHF_ALLOC + SHF_WRITE */
#define	M_ID_DYNAMIC	0x03
#define	M_ID_PLT	0x10
#define	M_ID_BSS	0xff

#define	M_ID_SYMTAB	0x02
#define	M_ID_STRTAB	0x03
#define	M_ID_NOTE	0x04

#define	M_ID_UNKNOWN	0xfe

#endif
