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
 *	Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 *	Use is subject to license terms.
 */
#pragma ident	"@(#)amd64unwind.c	1.7	05/06/08 SMI"

#include	<string.h>
#include	<stdio.h>
#include	<sys/types.h>
#include	<sgs.h>
#include	<debug.h>
#include	<_libld.h>
#include	<sys/elf_amd64.h>
#include	<dwarf.h>
#include	<stdlib.h>

/*
 * A EH_FRAME_HDR consists of the following:
 *
 *	Encoding	Field
 *	--------------------------------
 *	unsigned byte	version
 *	unsigned byte	eh_frame_ptr_enc
 *	unsigned byte	fde_count_enc
 *	unsigned byte	table_enc
 *	encoded		eh_frame_ptr
 *	encoded		fde_count
 *	[ binary search table ]
 *
 * The binary search table entries each consists of:
 *
 *	encoded		initial_func_loc
 *	encoded		FDE_address
 *
 * The entries in the binary search table are sorted
 * in a increasing order by the initial location.
 *
 *
 * version
 *
 *   Version of the .eh_frame_hdr format. This value shall be 1.
 *
 * eh_frame_ptr_enc
 *
 *    The encoding format of the eh_frame_ptr field.  For shared
 *    libraries the encoding must be
 *    DW_EH_PE_sdata4|DW_EH_PE_pcrel or
 *    DW_EH_PE_sdata4|DW_EH_PE_datarel.
 *
 *
 * fde_count_enc
 *
 *    The encoding format of the fde_count field. A value of
 *    DW_EH_PE_omit indicates the binary search table is not
 *    present.
 *
 * table_enc
 *
 *    The encoding format of the entries in the binary search
 *    table. A value of DW_EH_PE_omit indicates the binary search
 *    table is not present. For shared libraries the encoding
 *    must be DW_EH_PE_sdata4|DW_EH_PE_pcrel or
 *    DW_EH_PE_sdata4|DW_EH_PE_datarel.
 *
 *
 * eh_frame_ptr
 *
 *    The encoded value of the pointer to the start of the
 *    .eh_frame section.
 *
 * fde_count
 *
 *    The encoded value of the count of entries in the binary
 *    search table.
 *
 * binary search table
 *
 *    A binary search table containing fde_count entries. Each
 *    entry of the table consist of two encoded values, the
 *    initial location of the function to which an FDE applies,
 *    and the address of the FDE. The entries are sorted in an
 *    increasing order by the initial location value.
 *
 */


/*
 * EH_FRAME sections
 * =================
 *
 * The call frame information needed for unwinding the stack is output in
 * an ELF section(s) of type SHT_AMD64_UNWIND. In the simplest case there
 * will be one such section per object file and it will be named
 * ".eh_frame".  An .eh_frame section consists of one or more
 * subsections. Each subsection contains a CIE (Common Information Entry)
 * followed by varying number of FDEs (Frame Descriptor Entry). A FDE
 * corresponds to an explicit or compiler generated function in a
 * compilation unit, all FDEs can access the CIE that begins their
 * subsection for data.
 *
 * If an object file contains C++ template instantiations there shall be
 * a separate CIE immediately preceding each FDE corresponding to an
 * instantiation.
 *
 * Using the preferred encoding specified below, the .eh_frame section can
 * be entirely resolved at link time and thus can become part of the
 * text segment.
 *
 * .eh_frame Section Layout
 * ------------------------
 *
 * EH_PE encoding below refers to the pointer encoding as specified in the
 * enhanced LSB Chapter 7 for Eh_Frame_Hdr.
 *
 * Common Information Entry (CIE)
 * ------------------------------
 * CIE has the following format:
 *
 *                           Length
 *                              in
 *     Field                   Byte      Description
 *     -----                  ------     -----------
 *  1. Length                   4        Length of CIE (not including
 *					 this 4-byte field).
 *
 *  2. CIE id                   4        Value Zero (0) for .eh_frame
 *					 (used to distinguish CIEs and
 *					 FDEs when scanning the section)
 *
 *  3. Version                  1        Value One (1)
 *
 *  4. CIE Augmentation       string     Null-terminated string with legal
 *					 values being "" or 'z' optionally
 *					 followed by single occurrances of
 *					 'P', 'L', or 'R' in any order.
 *     String                            The presence of character(s) in the
 *                                       string dictates the content of
 *                                       field 8, the Augmentation Section.
 *					 Each character has one or two
 *					 associated operands in the AS.
 *					 Operand order depends on
 *					 position in the string ('z' must
 *					 be first).
 *
 *  5. Code Align Factor      uleb128    To be multiplied with the
 *					 "Advance Location" instructions in
 *                                       the Call Frame Instructions
 *
 *  6. Data Align Factor      sleb128    To be multiplied with all offset
 *                                       in the Call Frame Instructions
 *
 *  7. Ret Address Reg          1        A "virtual' register representation
 *                                       of the return address. In Dwarf V2,
 *                                       this is a byte, otherwise it is
 *                                       uleb128. It is a byte in gcc 3.3.x
 *
 *  8. Optional CIE           varying    Present if Augmentation String in
 *     Augmentation Section              field 4 is not 0.
 *
 *     z:
 * 	size		   uleb128       Length of the remainder of the
 *				         Augmentation Section
 *
 *     P:
 * 	personality_enc    1	         Encoding specifier - preferred
 *					 value is a pc-relative, signed
 *				         4-byte
 *
 *
 *        personality routine (encoded)  Encoded pointer to personality
 *					 routine (actually to the PLT
 *				         entry for the personality
 *				         routine)
 *     R:
 * 	code_enc           1	      Non-default encoding for the
 *				      code-pointers (FDE members
 *				      "initial_location" and "address_range"
 *				      and the operand for DW_CFA_set_loc)
 *				      - preferred value is pc-relative,
 *				      signed 4-byte.
 *     L:
 * 	lsda_enc	   1	      FDE augmentation bodies may contain
 *				      LSDA pointers. If so they are
 *				      encoded as specified here -
 *				      preferred value is pc-relative,
 *				      signed 4-byte possibly indirect
 *				      thru a GOT entry.
 *
 *
 *  9. Optional Call Frame varying
 *     Instructions
 *
 * The size of the optional call frame instruction area must be computed
 * based on the overall size and the offset reached while scanning the
 * preceding fields of the CIE.
 *
 *
 * Frame Descriptor Entry (FDE)
 * ----------------------------
 * FDE has the following format:
 *
 *                            Length
 *                              in
 *     Field                   Byte      Description
 *     -----                  ------     -----------
 *  1. Length                   4        Length of remainder of this FDE
 *
 *  2. CIE Pointer              4        Distance from this field to the
 *				         nearest preceding CIE
 *				         (uthe value is subtracted from the
 *					 current address). This value
 *				         can never be zero and thus can
 *				         be used to distinguish CIE's and
 *				         FDE's when scanning the
 *				         .eh_frame section
 *
 *  3. Initial Location       varying    Reference to the function code
 *                                       corresponding to this FDE.
 *                                       If 'R' is missing from the CIE
 *                                       Augmentation String, the field is an
 *                                       8-byte absolute pointer. Otherwise,
 *                                       the corresponding EH_PE encoding in the
 *                                       CIE Augmentation Section is used to
 *                                       interpret the reference.
 *
 *  4. Address Range          varying    Size of the function code corresponding
 *                                       to this FDE.
 *                                       If 'R' is missing from the CIE
 *                                       Augmentation String, the field is an
 *                                       8-byte unsigned number. Otherwise,
 *                                       the size is determined by the
 *				         corresponding EH_PE encoding in the
 *                                       CIE Augmentation Section (the
 *				         value is always absolute).
 *
 *  5. Optional FDE           varying    present if CIE augmentation
 *     Augmentation Section	         string is non-empty.
 *
 *
 *     'z':
 * 	length		   uleb128       length of the remainder of the
 *				         FDE augmentation section
 *
 *
 *     'L' (and length > 0):
 *         LSDA               varying    LSDA pointer, encoded in the
 *				         format specified by the
 *				         corresponding operand in the CIE's
 *				         augmentation body.
 *
 *  6. Optional Call          varying
 *     Frame Instructions
 *
 * The size of the optional call frame instruction area must be computed
 * based on the overall size and the offset reached while scanning the
 * preceding fields of the FDE.
 *
 * The overall size of a .eh_frame section is given in the ELF section
 * header.  The only way to determine the number of entries is to scan
 * the section till the end and count.
 *
 */




/*
 * The job of this function is to determine how much space
 * will be required for the final table.  We'll build
 * it later.
 */
uintptr_t
make_amd64_unwindhdr(Ofl_desc *ofl)
{
	Shdr	    *shdr;
	Elf_Data    *elfdata;
	Is_desc	    *isec;
	Xword	    size;
	Xword	    fde_cnt;
	Listnode    *lnp;
	Os_desc	    *osp;

	/*
	 * we only build a unwind header if we have
	 * sum unwind information in the file.
	 */
	if (ofl->ofl_unwind.head == NULL)
		return (1);

	/*
	 * Allocate and initialize the Elf_Data structure.
	 */
	if ((elfdata = libld_calloc(sizeof (Elf_Data), 1)) == 0)
		return (S_ERROR);
	elfdata->d_type = ELF_T_BYTE;
	elfdata->d_align = M_WORD_ALIGN;
	elfdata->d_version = ofl->ofl_libver;


	/*
	 * Allocate and initialize the Shdr structure.
	 */
	if ((shdr = libld_calloc(sizeof (Shdr), 1)) == 0)
		return (S_ERROR);
	shdr->sh_type = SHT_AMD64_UNWIND;
	shdr->sh_flags = SHF_ALLOC;
	shdr->sh_addralign = M_WORD_ALIGN;
	shdr->sh_entsize = 0;

	/*
	 * Allocate and initialize the Is_desc structure.
	 */
	if ((isec = libld_calloc(1, sizeof (Is_desc))) == 0)
		return (S_ERROR);
	isec->is_name = MSG_ORIG(MSG_SCN_UNWINDHDR);
	isec->is_shdr = shdr;
	isec->is_indata = elfdata;


	if ((ofl->ofl_unwindhdr = place_section(ofl, isec,
	    M_ID_UNWINDHDR, 0)) == (Os_desc *)S_ERROR)
		return (S_ERROR);

	/*
	 * Following we quickly scan through all of
	 * the input Frame info - counting for each
	 * FDE will have to index.  Each fde_entry
	 * will get a corresponding entry in the
	 * binary search table.
	 */
	fde_cnt = 0;
	for (LIST_TRAVERSE(&ofl->ofl_unwind, lnp, osp)) {
		Is_desc	    *isp;
		Listnode    *_lnp;
		for (LIST_TRAVERSE(&osp->os_isdescs, _lnp, isp)) {
			unsigned char	*data;
			size_t		datasize;
			uint64_t	off;
			uint_t		length, id;

			data = (unsigned char *)isp->is_indata->d_buf;
			datasize = isp->is_indata->d_size;
			off = 0;
			while (off < datasize) {
				uint_t	    ndx;

				ndx = 0;
				/*
				 * extract length in lsb format
				 */
				length = LSB32EXTRACT(data + off + ndx);
				ndx += 4;

				/*
				 * extract CIE id in lsb format
				 */
				id = LSB32EXTRACT(data + off + ndx);
				ndx += 4;

				/*
				 * A CIE record has a id of '0', otherwise
				 * this is a FDE entry and the 'id' is the
				 *  CIE pointer.
				 */
				if (id == 0) {
					uint_t	cieversion;
					/*
					 * The only CIE version supported
					 * is '1' - quick sanity check
					 * here.
					 */
					cieversion = data[off + ndx];
					ndx += 1;
					if (cieversion != 1) {
					    eprintf(ERR_FATAL,
						MSG_INTL(MSG_UNW_BADCIEVERS),
						isp->is_file->ifl_name,
						isp->is_name,
						off);
					    return (S_ERROR);
					}
				} else
					fde_cnt++;
				off += length + 4;
			}
		}
	}


	/*
	 * section size:
	 *	byte	    version		+1
	 *	byte	    eh_frame_ptr_enc	+1
	 *	byte	    fde_count_enc	+1
	 *	byte	    table_enc		+1
	 *	4 bytes	    eh_frame_ptr	+4
	 *	4 bytes	    fde_count		+4
	 *	[4 bytes] [4bytes] * fde_count	...
	 */
	size = 12 + (8 * fde_cnt);

	if ((elfdata->d_buf = libld_calloc(size, 1)) == 0)
		return (S_ERROR);
	elfdata->d_size = size;
	shdr->sh_size = (Xword)size;

	return (1);
}


/*
 * the comparator function needs to calculate
 * the actual 'initloc' of a bintab entry - to
 * do this we initialize the following global to point
 * to it.
 */
static Addr framehdr_addr;

static int
bintabcompare(const void *p1, const void *p2)
{
	uint_t	    *bintab1, *bintab2;
	uint_t	    ent1, ent2;

	bintab1 = (uint_t *)p1;
	bintab2 = (uint_t *)p2;

	assert(bintab1 != 0);
	assert(bintab2 != 0);

	ent1 = *bintab1 + framehdr_addr;
	ent2 = *bintab2 + framehdr_addr;

	if (ent1 > ent2)
		return (1);
	if (ent1 < ent2)
		return (-1);
	return (0);
}

uintptr_t
populate_amd64_unwindhdr(Ofl_desc *ofl)
{
	unsigned char	    *hdrdata;
	uint_t		    *binarytable;
	uint_t		    hdroff;
	Listnode	    *lnp;
	Addr		    hdraddr;
	Os_desc		    *hdrosp;
	Os_desc		    *osp;
	Os_desc		    *first_unwind;
	uint_t		    fde_count;
	uint_t		    *uint_ptr;

	/*
	 * Are we building the unwind hdr?
	 */
	if ((hdrosp = ofl->ofl_unwindhdr) == 0)
		return (1);

	hdrdata = hdrosp->os_outdata->d_buf;
	hdraddr = hdrosp->os_shdr->sh_addr;
	hdroff = 0;

	/*
	 * version == 1
	 */
	hdrdata[hdroff++] = 1;
	/*
	 * The encodings are:
	 *
	 *  eh_frameptr_enc	sdata4 | pcrel
	 *  fde_count_enc	udata4
	 *  table_enc		sdata4 | datarel
	 */
	hdrdata[hdroff++] = DW_EH_PE_sdata4 | DW_EH_PE_pcrel;
	hdrdata[hdroff++] = DW_EH_PE_udata4;
	hdrdata[hdroff++] = DW_EH_PE_sdata4 | DW_EH_PE_datarel;

	/*
	 *	Header Offsets
	 *	-----------------------------------
	 *	byte	    version		+1
	 *	byte	    eh_frame_ptr_enc	+1
	 *	byte	    fde_count_enc	+1
	 *	byte	    table_enc		+1
	 *	4 bytes	    eh_frame_ptr	+4
	 *	4 bytes	    fde_count		+4
	 */
	/* LINTED */
	binarytable =  (uint_t *)(hdrdata + 12);
	first_unwind = 0;
	fde_count = 0;

	for (LIST_TRAVERSE(&ofl->ofl_unwind, lnp, osp)) {
		unsigned char	*fdata;
		uint64_t	fdatasize;
		uint_t		foff;
		uint64_t	fndx;
		uint_t		cieRflag;
		uint_t		ciePflag;
		Shdr		*shdr;

		/*
		 * remember first UNWIND section to
		 * point to in the frame_ptr entry.
		 */
		if (first_unwind == 0)
			first_unwind = osp;

		fdata = osp->os_outdata->d_buf;
		shdr = osp->os_shdr;
		fdatasize = shdr->sh_size;
		foff = 0;
		ciePflag = 0;
		cieRflag = 0;
		while (foff < fdatasize) {
			uint_t	    length;
			uint_t	    id;

			fndx = 0;
			/*
			 * extract length in lsb format
			 */
			length = LSB32EXTRACT(fdata + foff + fndx);
			fndx += 4;

			/*
			 * extract id in lsb format
			 */
			id = LSB32EXTRACT(fdata + foff + fndx);
			fndx += 4;

			/*
			 * A CIE record has a id of '0'; otherwise
			 * this is a FDE entry and the 'id' is the
			 * CIE pointer.
			 */
			if (id == 0) {
				char	*cieaugstr;
				uint_t	cieaugndx;

				ciePflag = 0;
				cieRflag = 0;
				/*
				 * We need to drill through the CIE
				 * to find the Rflag.  It's the Rflag
				 * which describes how the FDE code-pointers
				 * are encoded.
				 */

				/*
				 * burn through version
				 */
				fndx++;
				/*
				 * augstr
				 */
				cieaugstr = (char *)(&fdata[foff + fndx]);
				fndx += strlen(cieaugstr) + 1;

				/*
				 * calign & dalign
				 */
				(void) uleb_extract(&fdata[foff], &fndx);
				(void) sleb_extract(&fdata[foff], &fndx);
				/*
				 * retreg
				 */
				fndx++;

				/*
				 * we walk through the augmentation
				 * section now looking for the Rflag
				 */
				for (cieaugndx = 0; cieaugstr[cieaugndx];
				    cieaugndx++) {
					switch (cieaugstr[cieaugndx]) {
					case 'z':
					    /* size */
					    (void) uleb_extract(&fdata[foff],
						&fndx);
					    break;
					case 'P':
					    /* personality */
					    ciePflag = fdata[foff + fndx];
					    fndx++;
						/*
						 * Just need to extract the
						 * value to move on to the next
						 * field.
						 */
					    (void) dwarf_ehe_extract(
						&fdata[foff + fndx],
						&fndx, ciePflag,
						ofl->ofl_ehdr->e_ident,
						shdr->sh_addr + foff + fndx);
					    break;
					case 'R':
					    /* code encoding */
					    cieRflag = fdata[foff + fndx];
					    fndx++;
					    break;
					case 'L':
					    /* lsda encoding */
					    fndx++;
					    break;
					}
				}
			} else {
				uint_t	    bintabndx;
				uint64_t    initloc;
				uint64_t    fdeaddr;

				bintabndx = fde_count * 2;
				fde_count++;

				/*
				 * FDEaddr is adjusted
				 * to account for the length & id which
				 * have already been consumed.
				 */
				fdeaddr = shdr->sh_addr + foff;

				initloc = dwarf_ehe_extract(&fdata[foff],
				    &fndx, cieRflag, ofl->ofl_ehdr->e_ident,
				    shdr->sh_addr + foff + fndx);
				binarytable[bintabndx] = (uint_t)(initloc -
					hdraddr);
				binarytable[bintabndx + 1] = (uint_t)(fdeaddr -
					hdraddr);
			}

			/*
			 * the length does not include the length
			 * itself - so account for that too.
			 */
			foff += length + 4;
		}
	}

	/*
	 * Do a quick sort on the binary table
	 */
	framehdr_addr = hdraddr;
	qsort((void *)binarytable, (size_t)fde_count,
		(size_t)(sizeof (uint_t) * 2), bintabcompare);
	/*
	 * Fill in:
	 *	first_frame_ptr
	 *	fde_count
	 */
	hdroff = 4;
	/* LINTED */
	uint_ptr = (uint_t *)(&hdrdata[hdroff]);
	*uint_ptr = first_unwind->os_shdr->sh_addr -
		hdrosp->os_shdr->sh_addr + hdroff;

	hdroff += 4;
	/* LINTED */
	uint_ptr = (uint_t *)&hdrdata[hdroff];
	*uint_ptr = fde_count;

	return (1);
}

uintptr_t
append_amd64_unwind(Os_desc *osp, Ofl_desc * ofl)
{
	Listnode    *lnp;
	Os_desc	    *_osp;
	/*
	 * Check to see if this output section is already
	 * on the list.
	 */
	for (LIST_TRAVERSE(&ofl->ofl_unwind, lnp, _osp))
		if (osp == _osp)
		    return (1);

	/*
	 * Append output section to unwind list
	 */
	if (list_appendc(&ofl->ofl_unwind, osp) == 0)
	    return (S_ERROR);
	return (1);
}

uintptr_t
process_amd64_unwind(const char *name, Ifl_desc *ifl, Shdr *shdr,
	Elf_Scn *scn, Word ndx, int ident, Ofl_desc *ofl)
{
	Os_desc	    *osp;
	Is_desc	    *isp;

	ident = M_ID_UNWIND;
	if (process_section(name, ifl, shdr, scn, ndx, ident, ofl) ==
	    S_ERROR)
		return (S_ERROR);
	/*
	 * When producing a relocatable object - just collect
	 * the sections.
	 */
	if (ofl->ofl_flags & FLG_OF_RELOBJ)
		return (1);
	/*
	 * If we are producing a executable or shared library, we need
	 * to keep track of all the output UNWIND sections - so that
	 * we can create the appropriate frame_hdr information.
	 *
	 * If the section hasn't been placed in the output file,
	 * then there's nothing for us to do.
	 */
	if (((isp = ifl->ifl_isdesc[ndx]) == 0) ||
	    ((osp = isp->is_osdesc) == 0))
		return (1);

	return (append_amd64_unwind(osp, ofl));
}
