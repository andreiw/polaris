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

/*
 * Bootstrap the linker/loader.
 */

#include <sys/types.h>
#include <sys/bootconf.h>
#include <sys/link.h>
#include <sys/auxv.h>
#include <sys/kobj.h>
#include <sys/elf.h>
#include <sys/kobj_impl.h>
#if defined(__GNUC__) && defined(_ASM_INLINES)
#include <asm/flush.h>
#endif

#define	MASK(n)		((1<<(n))-1)
#define	IN_RANGE(v, n)	((-(1<<((n)-1))) <= (v) && (v) < (1<<((n)-1)))
#define	roundup		ALIGN

/*
 * Boot transfers control here. At this point,
 * we haven't relocated our own symbols, so the
 * world (as we know it) is pretty small right now.
 */
void
/* _kobj_boot(cif, dvec, bootops, ebp)  */
_kobj_boot(unusedr3, unusedr4, cif, bootops, ebp)
	int unusedr3, unusedr4;
	int (*cif)(void *);
	struct bootops *bootops;
	Boot *ebp;
{
	Shdr *section[30];	/* cache */
	val_t bootaux[BA_NUM];
	Phdr *phdr;
	auxv_t *auxv = NULL;
	Shdr *sh;
	u_int sh_num;
	u_int end, edata = 0;
	int i;

	if (0) {
		__asm__ ("loopit:\n\t"
			"b loopit");
	}

	for (i = 0; i < BA_NUM; i++)
		bootaux[i].ba_val = NULL;
	/*
	 * Check the bootstrap vector.
	 */
	for (; ebp->eb_tag != EB_NULL; ebp++) {
		switch (ebp->eb_tag) {
		case EB_AUXV:
			auxv = (auxv_t *)ebp->eb_un.eb_ptr;
			break;
		case EB_DYNAMIC:
			bootaux[BA_DYNAMIC].ba_ptr = (void *)ebp->eb_un.eb_ptr;
			break;
		}
	}
	if (auxv == NULL)
		return;
	/*
	 * Now the aux vector.
	 */
	for (; auxv->a_type != AT_NULL; auxv++) {
		switch (auxv->a_type) {
		case AT_PHDR:
			bootaux[BA_PHDR].ba_ptr = auxv->a_un.a_ptr;
			break;
		case AT_PHENT:
			bootaux[BA_PHENT].ba_val = auxv->a_un.a_val;
			break;
		case AT_PHNUM:
			bootaux[BA_PHNUM].ba_val = auxv->a_un.a_val;
			break;
		case AT_PAGESZ:
			bootaux[BA_PAGESZ].ba_val = auxv->a_un.a_val;
			break;
		case AT_SUN_LDELF:
			bootaux[BA_LDELF].ba_ptr = auxv->a_un.a_ptr;
			break;
		case AT_SUN_LDSHDR:
			bootaux[BA_LDSHDR].ba_ptr = auxv->a_un.a_ptr;
			break;
		case AT_SUN_LDNAME:
			bootaux[BA_LDNAME].ba_ptr = auxv->a_un.a_ptr;
			break;
		case AT_SUN_LPAGESZ:
			bootaux[BA_LPAGESZ].ba_val = auxv->a_un.a_val;
			break;
		case AT_ENTRY:
			bootaux[BA_ENTRY].ba_ptr = auxv->a_un.a_ptr;
			break;
		}
	}

	sh = (Shdr *)bootaux[BA_LDSHDR].ba_ptr;
	sh_num = ((Ehdr *)bootaux[BA_LDELF].ba_ptr)->e_shnum;
	/*
	 * Build cache table for section addresses.
	 */
	for (i = 0; i < sh_num; i++) {
		section[i] = sh++;
	}
	/*
	 * Find the end of data
	 * (to allocate bss)
	 */
	phdr = (Phdr *)bootaux[BA_PHDR].ba_ptr;
	for (i = 0; i < bootaux[BA_PHNUM].ba_val; i++) {
		if (phdr->p_type == PT_LOAD &&
		    (phdr->p_flags & PF_W) && (phdr->p_flags & PF_X)) {
			edata = end = phdr->p_vaddr + phdr->p_memsz;
			break;
		}
		phdr = (Phdr *)((u_int)phdr + bootaux[BA_PHENT].ba_val);
	}
	if (edata == NULL)
		return;

	/*
	 * Find the symbol table, and then loop
	 * through the symbols adjusting their
	 * values to reflect where the sections
	 * were loaded.
	 */
	for (i = 1; i < sh_num; i++) {
		Shdr *shp;
		Sym *sp;
		u_int off;

		shp = section[i];
		if (shp->sh_type != SHT_SYMTAB)
			continue;

		for (off = 0; off < shp->sh_size; off += shp->sh_entsize) {
			sp = (Sym *)(shp->sh_addr + off);

			if (sp->st_shndx == SHN_ABS ||
			    sp->st_shndx == SHN_UNDEF)
				continue;
			/*
			 * Assign the addresses for COMMON
			 * symbols even though we haven't
			 * actually allocated bss yet.
			 */
			if (sp->st_shndx == SHN_COMMON) {
				end = ALIGN(end, sp->st_value);
				sp->st_value = end;
				/*
				 * Squirrel it away for later.
				 */
				if (bootaux[BA_BSS].ba_val == 0)
					bootaux[BA_BSS].ba_val = end;
				end += sp->st_size;
				continue;
			} else if (sp->st_shndx > (Half)sh_num)
				return;

			/*
			 * Symbol's new address.
			 */
			sp->st_value += section[sp->st_shndx]->sh_addr;
		}
	}
	/*
	 * Allocate bss for COMMON, if any.
	 */
	if (end > edata) {
		unsigned int va, bva;
		unsigned int asize;
		unsigned int align;

		if (bootaux[BA_LPAGESZ].ba_val) {
			asize = bootaux[BA_LPAGESZ].ba_val;
			align = bootaux[BA_LPAGESZ].ba_val;
		} else {
			asize = bootaux[BA_PAGESZ].ba_val;
			align = BO_NO_ALIGN;
		}
		va = roundup(edata, asize);
		bva = roundup(end, asize);

		if (bva > va) {
			boot_cell_t	args[7];
			int	(*bsys_1275_call)(void *);
			char		allocstr[6];

			bsys_1275_call =
			    (int (*)(void *))bootops->bsys_1275_call;
			allocstr[0] = 'a';
			allocstr[1] = 'l';
			allocstr[2] = 'l';
			allocstr[3] = 'o';
			allocstr[4] = 'c';
			allocstr[5] = '\0';
			args[0] = boot_ptr2cell(allocstr);
			args[1] = 3;
			args[2] = 1;
			args[3] = boot_ptr2cell((caddr_t)va);
			args[4] = boot_size2cell(bva - va);
			args[5] = boot_int2cell(align);
			(void) (bsys_1275_call)(args);
			bva = (uintptr_t)((caddr_t)boot_ptr2cell(args[6]));
			if (bva == NULL)
				return;
		}
		/*
		 * Zero it.
		 */
		for (va = edata; va < end; va++)
			*(char *)va = 0;
		/*
		 * Update the size of data.
		 */
		phdr->p_memsz += (end - edata);
	}
	/*
	 * Relocate our own symbols.  We'll handle the
	 * undefined symbols later.
	 */
	for (i = 1; i < sh_num; i++) {
		Shdr *rshp, *shp, *ssp;
		unsigned long baseaddr, reladdr, rend;
		int relocsize;

		rshp = section[i];

		if (rshp->sh_type != SHT_RELA)
			continue;
		/*
		 * Get the section being relocated
		 * and the symbol table.
		 */
		shp = section[rshp->sh_info];
		ssp = section[rshp->sh_link];

		reladdr = rshp->sh_addr;
		baseaddr = shp->sh_addr;
		rend = reladdr + rshp->sh_size;
		relocsize = rshp->sh_entsize;
		/*
		 * Loop through relocations.
		 */
		while (reladdr < rend) {
			Sym *symref;
			Rela *reloc;
			register unsigned long stndx;
			unsigned long off, *offptr;
			long addend, value;
			int rtype;

			reloc = (Rela *)reladdr;
			off = reloc->r_offset;
			addend = (long)reloc->r_addend;
			rtype = ELF32_R_TYPE(reloc->r_info);
			stndx = ELF32_R_SYM(reloc->r_info);

			reladdr += relocsize;

			if (rtype == R_PPC_NONE) {
				continue;
			}
			off += baseaddr;
			/*
			 * if R_PPC_RELATIVE, simply add base addr
			 * to reloc location
			 */
			if (rtype == R_PPC_RELATIVE) {
				value = baseaddr;
			} else {
				register unsigned int symoff, symsize;

				symsize = ssp->sh_entsize;

				for (symoff = 0; stndx; stndx--)
					symoff += symsize;
				symref = (Sym *)(ssp->sh_addr + symoff);

				/*
				 * Check for bad symbol index.
				 */
				if (symoff > ssp->sh_size)
					return;
				/*
				 * Just bind our own symbols at this point.
				 */
				if (symref->st_shndx == SHN_UNDEF) {
					continue;
				}

				value = symref->st_value;
				/* if (ELF32_ST_BIND(symref->st_info) !=
				    STB_LOCAL) { */
					/*
					 * If PC-relative, subtract ref addr.
					 */
					if (rtype == R_PPC_REL24 ||
					    rtype == R_PPC_REL14 ||
					    rtype == R_PPC_REL14_BRTAKEN ||
					    rtype == R_PPC_REL14_BRNTAKEN ||
					    rtype == R_PPC_ADDR30 ||
					    rtype == R_PPC_REL32 ||
					    rtype == R_PPC_PLT24)
						value -= off;
			}
			offptr = (unsigned long *)off;
			/*
			 * insert value calculated at reference point
			 * 3 cases - normal byte order aligned, normal byte
			 * order unaligned, and byte swapped
			 * for the swapped and unaligned cases we insert value
			 * a byte at a time
			 */
			switch (rtype) {
			case R_PPC_ADDR32:
			case R_PPC_GLOB_DAT:
			case R_PPC_RELATIVE:
			case R_PPC_REL32:
				*offptr = value + addend;
				break;
			case R_PPC_ADDR24:
			case R_PPC_REL24:
			case R_PPC_PLT24:
				value += addend;
				value >>= 2;
				if (IN_RANGE(value, 24)) {
					value &= MASK(24);
					value <<= 2;
				} else {
					return;
				}
				*offptr |= value;
				break;
			case R_PPC_ADDR16:
				value += addend;
				if (IN_RANGE(value, 16))
					*offptr |= value;
				else {
					return;
				}
				break;
			case R_PPC_ADDR16_LO:
				value += addend;
				value &= MASK(16);
				/* The shift (<<16) is because compiler (gcc) that sets
				 * the relocation offset as the address of the
				 * relocation bytes (not the instruction address).
				 * Only the upper 16 bits of offptr are
				 * used (the lower 16 bits are for next instruction).
				 * Same thing for the R_PPC_ADDR16_HA (HI).
				 *
				 * The clear of the 16-bit reloation files is because gcc
				 * already sets the addend in the relocation fileds for the
				 * instruction (addi), so a direct "or" causes the addend
				 * being added twice. For R_PPC_ADDR16_HA, it seems that gcc
				 * does not set the addent there, yet for safety, we do this.
				 */
				value <<= 16;
				*offptr &= 0x0000FFFF;
				*offptr |= value;
				break;
			case R_PPC_ADDR16_HI:
				value += addend;
				value >>= 16;
				value &= MASK(16);
				*offptr |= value;
				break;
			case R_PPC_ADDR16_HA:
				value += addend;
				if (value & 0x8000) {
					value >>= 16;
					++value;
				}
				else
					value >>= 16;
				value &= MASK(16);
				value <<= 16;
				*offptr &= 0x0000FFFF;
				*offptr |= value;
				break;
			case R_PPC_ADDR14:
			case R_PPC_ADDR14_BRTAKEN:
			case R_PPC_ADDR14_BRNTAKEN:
			case R_PPC_REL14:
			case R_PPC_REL14_BRTAKEN:
			case R_PPC_REL14_BRNTAKEN:
				value += addend;
				if (IN_RANGE(value, 16)) {
					value &= ~3;
					*offptr |= value;
				} else {
					return;
				}
				break;
			case R_PPC_ADDR30:
				value += addend;
				value &= ~3;
				*offptr |= value;
				break;
			default:
				return;
			}

			/* force instruction to be visible to icache */
			doflush(offptr);

			/*
			 * We only need to do it once.
			 */
			reloc->r_info = ELF32_R_INFO(stndx, R_PPC_NONE);
		} /* while */
	}
	/*
	 * Done relocating all of our *defined*
	 * symbols, so we hand off.
	 */
	kobj_init(cif, (void *)NULL, bootops, bootaux);
}
