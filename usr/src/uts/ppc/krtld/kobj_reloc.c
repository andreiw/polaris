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
 * PowerPC relocation code.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/user.h>
#include <sys/bootconf.h>
#include <sys/modctl.h>
#include <sys/elf.h>
#include <sys/kobj.h>
#include <sys/kobj_impl.h>

#include "reloc.h"

int
do_relocations(struct module *mp)
{
	unsigned int secnum;
	Shdr *shp, *rshp;
	unsigned int nreloc;

	/* do the relocations */
	for (secnum = 1; secnum < mp->hdr.e_shnum; secnum++) {
		rshp = (Shdr *)
			(mp->shdrs + secnum * mp->hdr.e_shentsize);
		if (rshp->sh_type == SHT_REL) {
			_kobj_printf(ops, "%s can't process type SHT_REL\n",
			    mp->filename);
			return (1);
		}
		if (rshp->sh_type != SHT_RELA)
			continue;
		if (rshp->sh_link != mp->symtbl_section) {
			_kobj_printf(ops, "%s reloc for non-default symtab\n",
			    mp->filename);
			return (-1);
		}
		if (rshp->sh_info >= mp->hdr.e_shnum) {
			_kobj_printf(ops, "do_relocations: %s sh_info out of "
			    "range ", mp->filename);
			_kobj_printf(ops, "%d\n", secnum);
			goto bad;
		}
		nreloc = rshp->sh_size / rshp->sh_entsize;

		/* get the section header that this reloc table refers to */
		shp = (Shdr *)
		    (mp->shdrs + rshp->sh_info * mp->hdr.e_shentsize);

		/*
		 * Do not relocate any section that isn't loaded into memory.
		 * Most commonly this will skip over the .rela.stab* sections
		 */
		if (!(shp->sh_flags & SHF_ALLOC))
			continue;
#ifdef	KOBJ_DEBUG
		if (kobj_debug & D_RELOCATIONS)
			_kobj_printf(ops, "krtld: relocating: file=%s ",
				mp->filename);
			_kobj_printf(ops, "section=%d\n", secnum);
#endif

		if (do_relocate(mp, (char *)rshp->sh_addr, rshp->sh_type, nreloc, 
		    rshp->sh_entsize, shp->sh_addr) < 0) {
			_kobj_printf(ops, "do_relocations: %s do_relocate failed\n",
			    mp->filename);
			goto bad;
		}
		kobj_free((void *)rshp->sh_addr, rshp->sh_size);
	}
	return (0);
bad:
	kobj_free((void *)rshp->sh_addr, rshp->sh_size);
	return (-1);
}

/*
 * Probe Discovery
 */

void *__tnf_probe_list_head;
void *__tnf_tag_list_head;

#define	PROBE_MARKER_SYMBOL		"__tnf_probe_version_1"
#define	PROBE_CONTROL_BLOCK_LINK_OFFSET	4
#define	TAG_MARKER_SYMBOL	"__tnf_tag_version_1"


/*
 * The kernel run-time linker calls this to try to resolve a reference
 * it can't otherwise resolve.  We see if it's marking a probe control
 * block; if so, we do the resolution and return 0.  If not, we return
 * 1 to show that we can't resolve it, either.
 */

int
tnf_reloc_resolve(char *symname, Addr *value_p, unsigned long *offset_p)
{
	if (strcmp(symname, PROBE_MARKER_SYMBOL) == 0) {
		*value_p = (long)__tnf_probe_list_head;
		__tnf_probe_list_head = (void *)*offset_p;
		*offset_p += PROBE_CONTROL_BLOCK_LINK_OFFSET;
		return (0);
	}
	if (strcmp(symname, TAG_MARKER_SYMBOL) == 0) {
		*value_p = (long)__tnf_tag_list_head;
		__tnf_tag_list_head = (void *)*offset_p;
		return (0);
	}
	return (1);
}

int
do_relocate(struct module * mp, char * reltbl, Word relshtype, int nreloc,
	int relocsize, Addr baseaddr)
{
	unsigned long stndx;
	register unsigned long reladdr, rend;
	unsigned long off;
	register unsigned int rtype;
	register long addend;
	long value;
	Sym *symref;
	int symnum;
	int err = 0;

	reladdr = (unsigned long)reltbl;
	rend = reladdr + nreloc * relocsize;

#ifdef	KOBJ_DEBUG
	if (kobj_debug & D_RELOCATIONS) {
		_kobj_printf(ops, "krtld:\ttype\t\t\toffset\t   addend"
			"      symbol\n");
		_kobj_printf(ops, "krtld:\t\t\t\t\t   value\n");
	}
#endif

	symnum = -1;
	/* loop through relocations */
	while (reladdr < rend) {

		symnum++;
		rtype = ELF_R_TYPE(((Rela *)reladdr)->r_info);
		off = ((Rela *)reladdr)->r_offset;
		stndx = ELF_R_SYM(((Rela *)reladdr)->r_info);
		if (stndx >= mp->nsyms) {
			_kobj_printf(ops, "do_relocate: bad strndx %d\n", symnum);
			return (-1);
		}
		addend = (long)(((Rela *)reladdr)->r_addend);
		reladdr += relocsize;

#ifdef	KOBJ_DEBUG
		if (kobj_debug & D_RELOCATIONS) {
			Sym *	symp;
			symp = (Sym *)
				(mp->symtbl+(stndx * mp->symhdr->sh_entsize));
			_kobj_printf(ops, "krtld:\t%s",
				conv_reloc_PPC_type_str(rtype));
			_kobj_printf(ops, "\t0x%8x ", off);
			_kobj_printf(ops, "0x%8x  ", addend);
			_kobj_printf(ops, "%s\n", (const char *)mp->strings + symp->st_name);
		}
#endif

		if (rtype == R_PPC_NONE)
			continue;

		if (!(mp->flags & KOBJ_EXEC))
			off += baseaddr;

		/*
		 * if R_PPC_RELATIVE, simply add base addr
		 * to reloc location
		 */
		if (rtype == R_PPC_RELATIVE)
			value = baseaddr;
		else {
			/*
			 * get symbol table entry - if symbol is local
			 * value is base address of this object
			 */
			symref = (Sym *)
				(mp->symtbl+(stndx * mp->symhdr->sh_entsize));

			if (ELF_ST_BIND(symref->st_info) == STB_LOCAL) {
				/* *** this is different for .o and .so */
				value = symref->st_value;
			} else {
				/*
				 * It's global. Allow weak references. If
				 * the symbol is undefined, give TNF (the
				 * kernel probes facility) a chance to see
				 * if it's a probe site, and fix it up if so.
				 */
				if (symref->st_shndx == SHN_UNDEF &&
				    tnf_reloc_resolve(mp->strings +
					symref->st_name, &symref->st_value,
					&off) != 0) {
					if (ELF_ST_BIND(symref->st_info)
					    != STB_WEAK) {
						_kobj_printf(ops, "not found: %s\n",
						    mp->strings +
						    symref->st_name);
						err = 1;
					}
					continue;
				} else { /* symbol found  - relocate */
					/*
					 * calculate location of definition
					 * - symbol value plus base address of
					 * containing shared object
					 */
					value = symref->st_value;

				} /* end else symbol found */
			} /* end global or weak */
		} /* end not R_PPC_RELATIVE */

		value += addend;
		/*
		 * calculate final value -
		 * if PC-relative, subtract ref addr
		 */
		if (IS_PC_RELATIVE(rtype))
			value -= off;

#ifdef	KOBJ_DEBUG
		if (kobj_debug & D_RELOCATIONS) {
			_kobj_printf(ops, "krtld:\t\t\t\t0x%8x ", off);
			_kobj_printf(ops, "0x%8x\n", value);
		}
#endif
		if (do_reloc(rtype, (unsigned char *)off, (Word *)&value,
		    (const char *)mp->strings + symref->st_name,
		    mp->filename, 0) == 0)
			err = 1;
	} /* end of while loop */

	if (err)
		return (-1);
	return (0);
}
