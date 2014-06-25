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
 * Copyright (c) 2006 by Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 *
 * Copyright (c) 2006, Noah Yan <noah.yan@gmail.com>
 */

#pragma ident 	"%Z%%M% %I%     %E% NY"

/*
 * standalone startup code for ppc booters. r5 contains client interface
 * handler address
 */

#include <sys/reg.h>
#include <sys/asm_linkage.h>

#if defined(lint)

/*ARGSUSED*/
void
_start(void *a, ...)
{}

#else
	.section	".text"
	.align	8
	.global	end
	.global	edata
	.global	main

	.section	".bss"
	.align	8

/*
 * Create a stack just below _start.
 */

#define	STACK_SIZE	0x8000 /* 32 kbytes */
	.lcomm _bootstack, STACK_SIZE, 16
_bootstacktop:			/* end --top-- of boot stack */

/*
 * Enter here for all disk/secondary booters loaded by a bootblk program or
 * inetboot loaded by OBP.
 */
	.section	".text"
	.global	prom_exit_to_mon

	ENTRY(_start)

	/* Clear BSS as ANSI C requirements */
	lis	r9, __bss_start@h
	ori	r9, r9, __bss_start@l
	addi	r9, r9, 3
	rlwinm	r9, r9, 0, 0, 29
	lis	r10, _end@h
	ori	r10, r10, _end@l
	addi	r9, r9, -4
	li	r0, 0
clear_bss:
	addi	r9, r9, 4
	stwu	r0, 4(r9)
	cmp	r0, r9, r10
	ble	clear_bss

	/* Set up the stack */
	lis	r9, _bootstacktop@h        /*  r9 = &_start (text section entry) */
	ori	r9, r9, _bootstacktop@l
	subi	r1, r9, 64                 /* Start the stack 64 bytes below _start */
	clrrwi	r1, r1, 4                  /* Make sure it is aligned on 16 bytes. */
	stwu	r0, -16(r1)                /* r0 = 0 */

	mr	r3, r5                     /* pass cif */

	b	main                       /* main(prom-cookie) */

	/* XXX - enable Hi BAT's to test 0xE000.0000 location of kernel on ODW only*/

#ifdef XXXODW
			
	lis	r8,0xe000		/* enable virt space for kernel */
	ori	r8,r8,0x01ff		/* size of 16 mb for these BATs */
	isync
	mtspr	560,r8			/* IBAT4U and DBAT4U */
	isync
	mtspr	568,r8
	isync
	lis	r8,0x0800		/* physical will map at 128mb start */
	ori	r8,r8,0x0012		/* give it full rights */
	isync
	mtspr	561,r8			/* IBAT4L and DBAT4L */
	isync
	mtspr	569,r8
	isync
	mfspr	r8,1008
	oris	r8,r8,0x80
	isync
	mtspr	1008,r8
	isync
#endif

        b       main                    /* main(prom-cookie) */

        /* print stupid error message here */

	b	prom_enter_mon          /* can't happen .. :-) */
	nop
	SET_SIZE(_start)

#endif	/* lint */
