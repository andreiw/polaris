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
 * Copyright 1996 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */
#pragma ident	"@(#)syms.c	1.6	05/06/11 SMI"

#include <stdio.h>
#include <string.h>
#include <libelf.h>
#include "rdb.h"


retc_t
str_map_sym(const char * symname, map_info_t * mp, GElf_Sym * symptr,
	char ** str)
{
	sym_tbl_t *	symp;
	char *		strs;
	int		i;

	if (mp->mi_symtab.st_syms)
		symp = &(mp->mi_symtab);
	else if (mp->mi_dynsym.st_syms)
		symp = &(mp->mi_dynsym);
	else
		return (RET_FAILED);

	strs = symp->st_strs;

	for (i = 0; i < (int)symp->st_symn; i++) {
		GElf_Sym sym;

		if (gelf_getsym(symp->st_syms, i, &sym) == 0) {
			printf("gelf_getsym(): %s\n", elf_errmsg(-1));
			return (RET_FAILED);
		}

		if (sym.st_name == 0)
			continue;
		if ((sym.st_shndx == SHN_UNDEF) ||
		    (strcmp(strs + sym.st_name, symname) != 0))
			continue;
		*symptr = sym;
		if (str != NULL)
			*str = (char *)strs + symptr->st_name;
		if ((mp->mi_flags & FLG_MI_EXEC) == 0)
			symptr->st_value += (GElf_Addr)(mp->mi_addr);
		return (RET_OK);
	}

	return (RET_FAILED);
}

/*
 * If two syms are of equal value this routine will
 * favor one over the other based off of it's symbol
 * type.
 */
static GElf_Sym
sym_swap(GElf_Sym * s1, GElf_Sym * s2)
{
	int	t1 = GELF_ST_TYPE(s1->st_info);
	int	t2 = GELF_ST_TYPE(s2->st_info);

	if ((t1 == STT_FUNC) || (t2 == STT_FUNC)) {
		if (t1 == STT_FUNC)
			return (*s1);
		return (*s2);
	}

	if ((t1 == STT_OBJECT) || (t2 == STT_OBJECT)) {
		if (t1 == STT_OBJECT)
			return (*s1);
		return (*s2);
	}

	if ((t1 == STT_OBJECT) || (t2 == STT_OBJECT)) {
		if (t1 == STT_OBJECT)
			return (*s1);
		return (*s2);
	}
	return (*s1);
}


retc_t
addr_map_sym(map_info_t * mp, ulong_t addr, GElf_Sym * symptr,
	char ** str)
{
	sym_tbl_t *	symp;
	GElf_Sym	sym;
	GElf_Sym *	symr = 0;
	GElf_Sym *	lsymr = 0;
	GElf_Sym	rsym;
	GElf_Sym	lsym;
	ulong_t		baseaddr = 0;
	int		i;

	if ((mp->mi_flags & FLG_MI_EXEC) == 0)
		baseaddr = (ulong_t)mp->mi_addr;

	if (mp->mi_symtab.st_syms)
		symp = &(mp->mi_symtab);
	else if (mp->mi_dynsym.st_syms)
		symp = &(mp->mi_dynsym);
	else
		return (RET_FAILED);

	/*
	 * normalize address
	 */
	addr -= baseaddr;
	for (i = 0; i < (int)symp->st_symn; i++) {
		ulong_t	svalue;

		if (gelf_getsym(symp->st_syms, i, &sym) == 0) {
			printf("gelf_getsym(): %s\n", elf_errmsg(-1));
			return (RET_FAILED);
		}
		if ((sym.st_name == 0) || (sym.st_shndx == SHN_UNDEF))
			continue;

		svalue = (ulong_t)sym.st_value;

		if (svalue <= addr) {
			/*
			 * track both the best local and best
			 * global fit for this address.  Later
			 * we will favor the global over the local
			 */
			if ((GELF_ST_BIND(sym.st_info) == STB_LOCAL) &&
			    ((lsymr == 0) ||
			    (svalue >= (ulong_t)lsymr->st_value))) {
				if (lsymr && (lsymr->st_value == svalue))
					*lsymr = sym_swap(lsymr, &sym);
				else {
					lsymr = &lsym;
					*lsymr = sym;
				}
			} else if ((symr == 0) ||
			    (svalue >= (ulong_t)symr->st_value)) {
				if (symr && (symr->st_value == svalue))
					*symr = sym_swap(symr, &sym);
				else {
					symr = &rsym;
					*symr = sym;
				}
			}
		}
	}
	if ((symr == 0) && (lsymr == 0))
		return (RET_FAILED);

	if (lsymr) {
		/*
		 * If a possible local symbol was found should
		 * we use it.
		 */
		if (symr && (lsymr->st_value > symr->st_value))
			symr = lsymr;
		else if (symr == 0)
			symr = lsymr;
	}

	*symptr = *symr;
	*str = (char *)(symp->st_strs + symptr->st_name);
	symptr->st_value += baseaddr;
	return (RET_OK);
}


retc_t
addr_to_sym(struct ps_prochandle * ph, ulong_t addr,
	GElf_Sym * symp, char ** str)
{
	map_info_t *	mip;

	if ((mip = addr_to_map(ph, addr)) == 0)
		return (RET_FAILED);

	return (addr_map_sym(mip, addr, symp, str));
}


retc_t
str_to_sym(struct ps_prochandle * ph, const char * name,
	GElf_Sym * symp)
{
	map_info_t *	mip;

	if (ph->pp_lmaplist.ml_head == 0) {
		if (str_map_sym(name, &(ph->pp_ldsomap), symp, NULL) == RET_OK)
			return (RET_OK);

		return (str_map_sym(name, &(ph->pp_execmap), symp, NULL));
	}

	for (mip = ph->pp_lmaplist.ml_head; mip; mip = mip->mi_next)
		if (str_map_sym(name, mip, symp, NULL) == RET_OK)
			return (RET_OK);

	return (RET_FAILED);
}
