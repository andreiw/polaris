/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)tss.h	5.4 (Berkeley) 1/18/91
 * $FreeBSD: src/sys/i386/include/tss.h,v 1.13 2002/09/23 05:04:05 peter Exp $
 */

#ifndef	_AMD64_TSS_H
#define	_AMD64_TSS_H

#pragma ident	"@(#)tss.h	1.4	05/06/12 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef _ASM
/*
 * amd64 long mode TSS definition
 */
#pragma pack(2)
typedef struct amd64tss {
	uint32_t	tss_rsvd0;	/* reserved, ignored */
	uint64_t	tss_rsp0; 	/* stack pointer CPL = 0 */
	uint64_t	tss_rsp1; 	/* stack pointer CPL = 1 */
	uint64_t	tss_rsp2; 	/* stack pointer CPL = 2 */
	uint64_t	tss_rsvd1;	/* reserved, ignored */
	uint64_t	tss_ist1;	/* Interrupt stack table 1 */
	uint64_t	tss_ist2;	/* Interrupt stack table 2 */
	uint64_t	tss_ist3;	/* Interrupt stack table 3 */
	uint64_t	tss_ist4;	/* Interrupt stack table 4 */
	uint64_t	tss_ist5;	/* Interrupt stack table 5 */
	uint64_t	tss_ist6;	/* Interrupt stack table 6 */
	uint64_t	tss_ist7;	/* Interrupt stack table 7 */
	uint64_t	tss_rsvd2;	/* reserved, ignored */
	uint16_t	tss_rsvd3;	/* reserved, ignored */
	uint16_t	tss_iobase;	/* io bitmap offset */
} amd64tss_t;
#pragma pack()

/*
 * Legacy 386 TSS definition
 */
typedef struct i386tss {
	uint32_t	tss_link;	/* 16-bit prior TSS selector */
	uint32_t	tss_esp0;
	uint32_t	tss_ss0;
	uint32_t	tss_esp1;
	uint32_t	tss_ss1;
	uint32_t	tss_esp2;
	uint32_t	tss_ss2;
	uint32_t	tss_cr3;
	uint32_t	tss_eip;
	uint32_t	tss_eflags;
	uint32_t	tss_eax;
	uint32_t	tss_ecx;
	uint32_t	tss_edx;
	uint32_t	tss_ebx;
	uint32_t	tss_esp;
	uint32_t	tss_ebp;
	uint32_t	tss_esi;
	uint32_t	tss_edi;
	uint32_t	tss_es;
	uint32_t	tss_cs;
	uint32_t	tss_ss;
	uint32_t	tss_ds;
	uint32_t	tss_fs;
	uint32_t	tss_gs;
	uint32_t	tss_ldt;
	uint32_t	tss_bitmapbase;
} i386tss_t;

#endif	/* !_ASM */

#ifdef	__cplusplus
}
#endif

#endif	/* _AMD64_TSS_H */
