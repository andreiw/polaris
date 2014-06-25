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
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

	.ident	"@(#)atomic.s	1.3	05/06/08 SMI"

	.file	"atomic.s"

#include <sys/asm_linkage.h>

#if defined(_KERNEL)
	/*
	 * Legacy kernel interfaces; they will go away (eventually).
	 */
	ANSI_PRAGMA_WEAK2(cas8,atomic_cas_8,function)
	ANSI_PRAGMA_WEAK2(cas32,atomic_cas_32,function)
	ANSI_PRAGMA_WEAK2(cas64,atomic_cas_64,function)
	ANSI_PRAGMA_WEAK2(caslong,atomic_cas_ulong,function)
	ANSI_PRAGMA_WEAK2(casptr,atomic_cas_ptr,function)
	ANSI_PRAGMA_WEAK2(atomic_and_long,atomic_and_ulong,function)
	ANSI_PRAGMA_WEAK2(atomic_or_long,atomic_or_ulong,function)
#else
	/*
	 * Include the definitions for the libc weak aliases.
	 */
#include "../atomic_asm_weak.h"
#endif

	ENTRY(atomic_inc_8)
	ALTENTRY(atomic_inc_uchar)
	lock
	incb	(%rdi)
	ret
	SET_SIZE(atomic_inc_uchar)
	SET_SIZE(atomic_inc_8)

	ENTRY(atomic_inc_16)
	ALTENTRY(atomic_inc_ushort)
	lock
	incw	(%rdi)
	ret
	SET_SIZE(atomic_inc_ushort)
	SET_SIZE(atomic_inc_16)

	ENTRY(atomic_inc_32)
	ALTENTRY(atomic_inc_uint)
	lock
	incl	(%rdi)
	ret
	SET_SIZE(atomic_inc_uint)
	SET_SIZE(atomic_inc_32)

	ENTRY(atomic_inc_64)
	ALTENTRY(atomic_inc_ulong)
	lock
	incq	(%rdi)
	ret
	SET_SIZE(atomic_inc_ulong)
	SET_SIZE(atomic_inc_64)

	ENTRY(atomic_inc_8_nv)
	ALTENTRY(atomic_inc_uchar_nv)
	movb	(%rdi), %al	/ %al = old value
1:
	leaq	1(%rax), %rcx	/ %cl = new value
	lock
	cmpxchgb %cl, (%rdi)	/ try to stick it in
	jne	1b
	movzbl	%cl, %eax	/ return new value
	ret
	SET_SIZE(atomic_inc_uchar_nv)
	SET_SIZE(atomic_inc_8_nv)

	ENTRY(atomic_inc_16_nv)
	ALTENTRY(atomic_inc_ushort_nv)
	movw	(%rdi), %ax	/ %ax = old value
1:
	leaq	1(%rax), %rcx	/ %cx = new value
	lock
	cmpxchgw %cx, (%rdi)	/ try to stick it in
	jne	1b
	movzwl	%cx, %eax	/ return new value
	ret
	SET_SIZE(atomic_inc_ushort_nv)
	SET_SIZE(atomic_inc_16_nv)

	ENTRY(atomic_inc_32_nv)
	ALTENTRY(atomic_inc_uint_nv)
	movl	(%rdi), %eax	/ %eax = old value
1:
	leaq	1(%rax), %rcx	/ %ecx = new value
	lock
	cmpxchgl %ecx, (%rdi)	/ try to stick it in
	jne	1b
	movl	%ecx, %eax	/ return new value
	ret
	SET_SIZE(atomic_inc_uint_nv)
	SET_SIZE(atomic_inc_32_nv)

	ENTRY(atomic_inc_64_nv)
	ALTENTRY(atomic_inc_ulong_nv)
	movq	(%rdi), %rax	/ %rax = old value
1:
	leaq	1(%rax), %rcx	/ %rcx = new value
	lock
	cmpxchgq %rcx, (%rdi)	/ try to stick it in
	jne	1b
	movq	%rcx, %rax	/ return new value
	ret
	SET_SIZE(atomic_inc_ulong_nv)
	SET_SIZE(atomic_inc_64_nv)

	ENTRY(atomic_dec_8)
	ALTENTRY(atomic_dec_uchar)
	lock
	decb	(%rdi)
	ret
	SET_SIZE(atomic_dec_uchar)
	SET_SIZE(atomic_dec_8)

	ENTRY(atomic_dec_16)
	ALTENTRY(atomic_dec_ushort)
	lock
	decw	(%rdi)
	ret
	SET_SIZE(atomic_dec_ushort)
	SET_SIZE(atomic_dec_16)

	ENTRY(atomic_dec_32)
	ALTENTRY(atomic_dec_uint)
	lock
	decl	(%rdi)
	ret
	SET_SIZE(atomic_dec_uint)
	SET_SIZE(atomic_dec_32)

	ENTRY(atomic_dec_64)
	ALTENTRY(atomic_dec_ulong)
	lock
	decq	(%rdi)
	ret
	SET_SIZE(atomic_dec_ulong)
	SET_SIZE(atomic_dec_64)

	ENTRY(atomic_dec_8_nv)
	ALTENTRY(atomic_dec_uchar_nv)
	movb	(%rdi), %al	/ %al = old value
1:
	leaq	-1(%rax), %rcx	/ %cl = new value
	lock
	cmpxchgb %cl, (%rdi)	/ try to stick it in
	jne	1b
	movzbl	%cl, %eax	/ return new value
	ret
	SET_SIZE(atomic_dec_uchar_nv)
	SET_SIZE(atomic_dec_8_nv)

	ENTRY(atomic_dec_16_nv)
	ALTENTRY(atomic_dec_ushort_nv)
	movw	(%rdi), %ax	/ %ax = old value
1:
	leaq	-1(%rax), %rcx	/ %cx = new value
	lock
	cmpxchgw %cx, (%rdi)	/ try to stick it in
	jne	1b
	movzwl	%cx, %eax	/ return new value
	ret
	SET_SIZE(atomic_dec_ushort_nv)
	SET_SIZE(atomic_dec_16_nv)

	ENTRY(atomic_dec_32_nv)
	ALTENTRY(atomic_dec_uint_nv)
	movl	(%rdi), %eax	/ %eax = old value
1:
	leaq	-1(%rax), %rcx	/ %ecx = new value
	lock
	cmpxchgl %ecx, (%rdi)	/ try to stick it in
	jne	1b
	movl	%ecx, %eax	/ return new value
	ret
	SET_SIZE(atomic_dec_uint_nv)
	SET_SIZE(atomic_dec_32_nv)

	ENTRY(atomic_dec_64_nv)
	ALTENTRY(atomic_dec_ulong_nv)
	movq	(%rdi), %rax	/ %rax = old value
1:
	leaq	-1(%rax), %rcx	/ %rcx = new value
	lock
	cmpxchgq %rcx, (%rdi)	/ try to stick it in
	jne	1b
	movq	%rcx, %rax	/ return new value
	ret
	SET_SIZE(atomic_dec_ulong_nv)
	SET_SIZE(atomic_dec_64_nv)

	ENTRY(atomic_add_8)
	ALTENTRY(atomic_add_char)
	lock
	addb	%sil, (%rdi)
	ret
	SET_SIZE(atomic_add_char)
	SET_SIZE(atomic_add_8)

	ENTRY(atomic_add_16)
	ALTENTRY(atomic_add_short)
	lock
	addw	%si, (%rdi)
	ret
	SET_SIZE(atomic_add_short)
	SET_SIZE(atomic_add_16)

	ENTRY(atomic_add_32)
	ALTENTRY(atomic_add_int)
	lock
	addl	%esi, (%rdi)
	ret
	SET_SIZE(atomic_add_int)
	SET_SIZE(atomic_add_32)

	ENTRY(atomic_add_64)
	ALTENTRY(atomic_add_ptr)
	ALTENTRY(atomic_add_long)
	lock
	addq	%rsi, (%rdi)
	ret
	SET_SIZE(atomic_add_long)
	SET_SIZE(atomic_add_ptr)
	SET_SIZE(atomic_add_64)

	ENTRY(atomic_or_8)
	ALTENTRY(atomic_or_uchar)
	lock
	orb	%sil, (%rdi)
	ret
	SET_SIZE(atomic_or_uchar)
	SET_SIZE(atomic_or_8)

	ENTRY(atomic_or_16)
	ALTENTRY(atomic_or_ushort)
	lock
	orw	%si, (%rdi)
	ret
	SET_SIZE(atomic_or_ushort)
	SET_SIZE(atomic_or_16)

	ENTRY(atomic_or_32)
	ALTENTRY(atomic_or_uint)
	lock
	orl	%esi, (%rdi)
	ret
	SET_SIZE(atomic_or_uint)
	SET_SIZE(atomic_or_32)

	ENTRY(atomic_or_64)
	ALTENTRY(atomic_or_ulong)
	lock
	orq	%rsi, (%rdi)
	ret
	SET_SIZE(atomic_or_ulong)
	SET_SIZE(atomic_or_64)

	ENTRY(atomic_and_8)
	ALTENTRY(atomic_and_uchar)
	lock
	andb	%sil, (%rdi)
	ret
	SET_SIZE(atomic_and_uchar)
	SET_SIZE(atomic_and_8)

	ENTRY(atomic_and_16)
	ALTENTRY(atomic_and_ushort)
	lock
	andw	%si, (%rdi)
	ret
	SET_SIZE(atomic_and_ushort)
	SET_SIZE(atomic_and_16)

	ENTRY(atomic_and_32)
	ALTENTRY(atomic_and_uint)
	lock
	andl	%esi, (%rdi)
	ret
	SET_SIZE(atomic_and_uint)
	SET_SIZE(atomic_and_32)

	ENTRY(atomic_and_64)
	ALTENTRY(atomic_and_ulong)
	lock
	andq	%rsi, (%rdi)
	ret
	SET_SIZE(atomic_and_ulong)
	SET_SIZE(atomic_and_64)

	ENTRY(atomic_add_8_nv)
	ALTENTRY(atomic_add_char_nv)
	movb	(%rdi), %al	/ %al = old value
1:
	movb	%sil, %cl
	addb	%al, %cl	/ %cl = new value
	lock
	cmpxchgb %cl, (%rdi)	/ try to stick it in
	jne	1b
	movzbl	%cl, %eax	/ return new value
	ret
	SET_SIZE(atomic_add_char_nv)
	SET_SIZE(atomic_add_8_nv)

	ENTRY(atomic_add_16_nv)
	ALTENTRY(atomic_add_short_nv)
	movw	(%rdi), %ax	/ %ax = old value
1:
	movw	%si, %cx
	addw	%ax, %cx	/ %cx = new value
	lock
	cmpxchgw %cx, (%rdi)	/ try to stick it in
	jne	1b
	movzwl	%cx, %eax	/ return new value
	ret
	SET_SIZE(atomic_add_short_nv)
	SET_SIZE(atomic_add_16_nv)

	ENTRY(atomic_add_32_nv)
	ALTENTRY(atomic_add_int_nv)
	movl	(%rdi), %eax
1:
	movl	%esi, %ecx
	addl	%eax, %ecx
	lock
	cmpxchgl %ecx, (%rdi)
	jne	1b
	movl	%ecx, %eax
	ret
	SET_SIZE(atomic_add_int_nv)
	SET_SIZE(atomic_add_32_nv)

	ENTRY(atomic_add_64_nv)
	ALTENTRY(atomic_add_ptr_nv)
	ALTENTRY(atomic_add_long_nv)
	movq	(%rdi), %rax
1:
	movq	%rsi, %rcx
	addq	%rax, %rcx
	lock
	cmpxchgq %rcx, (%rdi)
	jne	1b
	movq	%rcx, %rax
	ret
	SET_SIZE(atomic_add_long_nv)
	SET_SIZE(atomic_add_ptr_nv)
	SET_SIZE(atomic_add_64_nv)

	ENTRY(atomic_and_8_nv)
	ALTENTRY(atomic_and_uchar_nv)
	movb	(%rdi), %al	/ %al = old value
1:
	movb	%sil, %cl
	andb	%al, %cl	/ %cl = new value
	lock
	cmpxchgb %cl, (%rdi)	/ try to stick it in
	jne	1b
	movzbl	%cl, %eax	/ return new value
	ret
	SET_SIZE(atomic_and_uchar_nv)
	SET_SIZE(atomic_and_8_nv)

	ENTRY(atomic_and_16_nv)
	ALTENTRY(atomic_and_ushort_nv)
	movw	(%rdi), %ax	/ %ax = old value
1:
	movw	%si, %cx
	andw	%ax, %cx	/ %cx = new value
	lock
	cmpxchgw %cx, (%rdi)	/ try to stick it in
	jne	1b
	movzwl	%cx, %eax	/ return new value
	ret
	SET_SIZE(atomic_and_ushort_nv)
	SET_SIZE(atomic_and_16_nv)

	ENTRY(atomic_and_32_nv)
	ALTENTRY(atomic_and_uint_nv)
	movl	(%rdi), %eax
1:
	movl	%esi, %ecx
	andl	%eax, %ecx
	lock
	cmpxchgl %ecx, (%rdi)
	jne	1b
	movl	%ecx, %eax
	ret
	SET_SIZE(atomic_and_uint_nv)
	SET_SIZE(atomic_and_32_nv)

	ENTRY(atomic_and_64_nv)
	ALTENTRY(atomic_and_ulong_nv)
	movq	(%rdi), %rax
1:
	movq	%rsi, %rcx
	andq	%rax, %rcx
	lock
	cmpxchgq %rcx, (%rdi)
	jne	1b
	movq	%rcx, %rax
	ret
	SET_SIZE(atomic_and_ulong_nv)
	SET_SIZE(atomic_and_64_nv)

	ENTRY(atomic_or_8_nv)
	ALTENTRY(atomic_or_uchar_nv)
	movb	(%rdi), %al	/ %al = old value
1:
	movb	%sil, %cl
	orb	%al, %cl	/ %cl = new value
	lock
	cmpxchgb %cl, (%rdi)	/ try to stick it in
	jne	1b
	movzbl	%cl, %eax	/ return new value
	ret
	SET_SIZE(atomic_and_uchar_nv)
	SET_SIZE(atomic_and_8_nv)

	ENTRY(atomic_or_16_nv)
	ALTENTRY(atomic_or_ushort_nv)
	movw	(%rdi), %ax	/ %ax = old value
1:
	movw	%si, %cx
	orw	%ax, %cx	/ %cx = new value
	lock
	cmpxchgw %cx, (%rdi)	/ try to stick it in
	jne	1b
	movzwl	%cx, %eax	/ return new value
	ret
	SET_SIZE(atomic_or_ushort_nv)
	SET_SIZE(atomic_or_16_nv)

	ENTRY(atomic_or_32_nv)
	ALTENTRY(atomic_or_uint_nv)
	movl	(%rdi), %eax
1:
	movl	%esi, %ecx
	orl	%eax, %ecx
	lock
	cmpxchgl %ecx, (%rdi)
	jne	1b
	movl	%ecx, %eax
	ret
	SET_SIZE(atomic_or_uint_nv)
	SET_SIZE(atomic_or_32_nv)

	ENTRY(atomic_or_64_nv)
	ALTENTRY(atomic_or_ulong_nv)
	movq	(%rdi), %rax
1:
	movq	%rsi, %rcx
	orq	%rax, %rcx
	lock
	cmpxchgq %rcx, (%rdi)
	jne	1b
	movq	%rcx, %rax
	ret
	SET_SIZE(atomic_or_ulong_nv)
	SET_SIZE(atomic_or_64_nv)

	ENTRY(atomic_cas_8)
	ALTENTRY(atomic_cas_uchar)
	movzbl	%sil, %eax
	lock
	cmpxchgb %dl, (%rdi)
	ret
	SET_SIZE(atomic_cas_uchar)
	SET_SIZE(atomic_cas_8)

	ENTRY(atomic_cas_16)
	ALTENTRY(atomic_cas_ushort)
	movzwl	%si, %eax
	lock
	cmpxchgw %dx, (%rdi)
	ret
	SET_SIZE(atomic_cas_ushort)
	SET_SIZE(atomic_cas_16)

	ENTRY(atomic_cas_32)
	ALTENTRY(atomic_cas_uint)
	movl	%esi, %eax
	lock
	cmpxchgl %edx, (%rdi)
	ret
	SET_SIZE(atomic_cas_uint)
	SET_SIZE(atomic_cas_32)

	ENTRY(atomic_cas_64)
	ALTENTRY(atomic_cas_ulong)
	ALTENTRY(atomic_cas_ptr)
	movq	%rsi, %rax
	lock
	cmpxchgq %rdx, (%rdi)
	ret
	SET_SIZE(atomic_cas_ptr)
	SET_SIZE(atomic_cas_ulong)
	SET_SIZE(atomic_cas_64)

	ENTRY(atomic_swap_8)
	ALTENTRY(atomic_swap_uchar)
	movzbl	%sil, %eax
	lock
	xchgb %al, (%rdi)
	ret
	SET_SIZE(atomic_swap_uchar)
	SET_SIZE(atomic_swap_8)

	ENTRY(atomic_swap_16)
	ALTENTRY(atomic_swap_ushort)
	movzwl	%si, %eax
	lock
	xchgw %ax, (%rdi)
	ret
	SET_SIZE(atomic_swap_ushort)
	SET_SIZE(atomic_swap_16)

	ENTRY(atomic_swap_32)
	ALTENTRY(atomic_swap_uint)
	movl	%esi, %eax
	lock
	xchgl %eax, (%rdi)
	ret
	SET_SIZE(atomic_swap_uint)
	SET_SIZE(atomic_swap_32)

	ENTRY(atomic_swap_64)
	ALTENTRY(atomic_swap_ulong)
	ALTENTRY(atomic_swap_ptr)
	movq	%rsi, %rax
	lock
	xchgq %rax, (%rdi)
	ret
	SET_SIZE(atomic_swap_ptr)
	SET_SIZE(atomic_swap_ulong)
	SET_SIZE(atomic_swap_64)

	ENTRY(atomic_set_long_excl)
	xorl	%eax, %eax
	lock
	btsq	%rsi, (%rdi)
	jnc	1f
	decl	%eax			/ return -1
1:
	ret
	SET_SIZE(atomic_set_long_excl)

	ENTRY(atomic_clear_long_excl)
	xorl	%eax, %eax
	lock
	btrq	%rsi, (%rdi)
	jc	1f
	decl	%eax			/ return -1
1:
	ret
	SET_SIZE(atomic_clear_long_excl)

#if !defined(_KERNEL)

	ENTRY(membar_enter)
	ALTENTRY(membar_exit)
	mfence
	ret
	SET_SIZE(membar_exit)
	SET_SIZE(membar_enter)

	ENTRY(membar_producer)
	sfence
	ret
	SET_SIZE(membar_producer)

	ENTRY(membar_consumer)
	lfence
	ret
	SET_SIZE(membar_consumer)

#endif	/* !_KERNEL */
