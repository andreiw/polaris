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
 * Copyright 2001,2003 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"@(#)wrsmlib.s	1.6	05/06/08 SMI"
	
/*
 * TL1 trap handler for DMV interrupts generated by WCIs
 * Note: You must build sun4u/genassym before assembling this file.
 */

#if defined(lint)
#include <sys/types.h>
#else
#include <sys/asm_linkage.h>
#include <sys/fsr.h>
#include <sys/sun4asi.h>	
#endif /* lint */

/*
 * Zero the parts of the fpreg file that we actually use
 * ( 1 or 3 sets of 8 registers )
 */
#define	FZERO1				\
	fzero	%f0			;\
	fzero	%f2			;\
	faddd	%f0, %f2, %f4		;\
	fmuld	%f0, %f2, %f6		;\
	faddd	%f0, %f2, %f8		;\
	fmuld	%f0, %f2, %f10		;\
	faddd	%f0, %f2, %f12		;\
	fmuld	%f0, %f2, %f14

#define	FZERO3				\
	fzero	%f0			;\
	fzero	%f2			;\
	faddd	%f0, %f2, %f4		;\
	fmuld	%f0, %f2, %f6		;\
	faddd	%f0, %f2, %f8		;\
	fmuld	%f0, %f2, %f10		;\
	faddd	%f0, %f2, %f12		;\
	fmuld	%f0, %f2, %f14		;\
	faddd	%f0, %f2, %f16		;\
	fmuld	%f0, %f2, %f18		;\
	faddd	%f0, %f2, %f20		;\
	fmuld	%f0, %f2, %f22		;\
	faddd	%f0, %f2, %f24		;\
	fmuld	%f0, %f2, %f26		;\
	faddd	%f0, %f2, %f28		;\
	fmuld	%f0, %f2, %f30		;\
	faddd	%f0, %f2, %f32		;\
	fmuld	%f0, %f2, %f34		;\
	faddd	%f0, %f2, %f36		;\
	fmuld	%f0, %f2, %f38		;\
	faddd	%f0, %f2, %f40		;\
	fmuld	%f0, %f2, %f42		;\
	faddd	%f0, %f2, %f44		;\
	fmuld	%f0, %f2, %f46

/*
 * Size of each block being stored
 */
#define	VIS_BLOCKSIZE		64

/*
 * Size of stack frame in order to accomodate 64-byte aligned
 * floating-point register save area and a 64-bit temp locations.
 */
#define	BLKCOPYFRAMESIZE	((VIS_BLOCKSIZE * 2) + 8)
#define BCOPY_FPREGS_OFFSET	(VIS_BLOCKSIZE * 2)
#define BCOPY_FPREGS_ADJUST	(VIS_BLOCKSIZE - 1)
#define	BCOPY_FPRS_OFFSET	(BCOPY_FPREGS_OFFSET + 8)
	
#define	BLKWRITEFRAMESIZE	((VIS_BLOCKSIZE * 4) + 8)
#define BWRITE_FPREGS_OFFSET	(VIS_BLOCKSIZE * 4)
#define BWRITE_FPREGS_ADJUST	((VIS_BLOCKSIZE * 3) - 1)
#define	BWRITE_FPRS_OFFSET	(BWRITE_FPREGS_OFFSET + 8)


#if defined(lint)
/* ARGSUSED */
void
wrsmlib_blkcopy(void *src, void *dst, uint_t num_blocks)
{}
#else /* !lint */
!
! Move a single cache line of data.  Survive UE and CE on the read.
! The source and destination must both be 64-byte aligned.
!
! %i0 = src va
! %i1 = dst va
! %i2 = num_blocks
! %i4 = cache of fpu state
!
! XXX redo to allow for case of misaligned data
	ENTRY(wrsmlib_blkcopy)

	save	%sp, -SA(MINFRAME + BLKCOPYFRAMESIZE), %sp

#ifndef	BCOPY_BUG_FIXED
	membar	#Sync			! protect against bcopy bug
#endif
	rd	%fprs, %i4
	st	%i4, [%fp + STACK_BIAS - BCOPY_FPRS_OFFSET] ! save orig %fprs
	btst	FPRS_FEF, %i4
	bz,a	1f
	  wr	%g0, FPRS_FEF, %fprs	

	! save in-use fpregs on stack
	membar	#Sync
	add	%fp, STACK_BIAS - BCOPY_FPREGS_ADJUST, %o2
	and	%o2, -VIS_BLOCKSIZE, %o2
	stda	%d0, [%o2]ASI_BLK_P
	membar	#Sync

	tst	%i2			! set CC	
1:
	! Perform block move
	bz,pn	%icc, 2f		! (always 32-bit) while (%i2 > 0) {
	 nop
	ldda	[%i0] ASI_BLK_P, %d0	!   tmp = *src;
#ifdef WRSM_NOCHEETAH
	membar	#Sync			! not needed by Cheetah
#endif
	stda	%d0, [%i1] ASI_BLK_P	!   *dst = tmp;
	membar	#Sync
	add	%i0, VIS_BLOCKSIZE, %i0	!   src++;
	add	%i1, VIS_BLOCKSIZE, %i1	!   dst++;
	ba	1b			!   branch back to 1: 
	deccc	%i2			! %i2-- ;}
2:
	! restore fp to the way we got it
	ld	[%fp + STACK_BIAS - BCOPY_FPRS_OFFSET], %i4
	btst	FPRS_FEF, %i4
	bz,a	3f			! branch forward to label 3:
	  nop

	! restore fpregs from stack
	add	%fp, STACK_BIAS - BCOPY_FPREGS_ADJUST, %o2
	and	%o2, -VIS_BLOCKSIZE, %o2
	ldda	[%o2] ASI_BLK_P, %d0
	membar	#Sync

	ba	4f
	  wr	%i4, 0, %fprs	
3:
	FZERO1
	wr	%i4, 0, %fprs		! fpu back to the way it was
4:
	ret
	restore
	SET_SIZE(wrsmlib_blkcopy)
#endif /* lint */

/*
 * Number of bytes needed to "break even" using VIS-accelerated
 * memory operations.
 */
#define	HW_THRESHOLD		256

/*
 * Number of outstanding prefetches.
 */
#define	CHEETAH_PREFETCH	7

#if defined(lint)
/* ARGSUSED */
void
wrsmlib_blkwrite(void *src, void *dst, uint_t num_blocks)
{}
#else /* !lint */
!
! Copy multiple cache lines of data by prefetching into P$.
! The destination address must be 64-byte aligned.
!
! %i0 = src va
! %i1 = dst va
! %i2 = num_blocks
! %i4 = cache of fpu state

	ENTRY(wrsmlib_blkwrite)

	save		%sp, -SA(MINFRAME + BLKWRITEFRAMESIZE), %sp

#ifndef	BCOPY_BUG_FIXED
	membar	#Sync				! protect against bcopy bug
#endif

	sllx		%i2, 6, %i2		! convert blocks to bytes

	rd		%fprs, %i4
	st		%i4, [%fp + STACK_BIAS - BWRITE_FPRS_OFFSET] 
	btst		FPRS_FEF, %i4
	bz,a		.do_blkcopy
	  wr		%g0, FPRS_FEF, %fprs	! always enable FPU

.fpregs_inuse:

	! save in-use fpregs on stack
	membar		#Sync
	add		%fp, STACK_BIAS - BWRITE_FPREGS_ADJUST, %o2
	and		%o2, -VIS_BLOCKSIZE, %o2
	stda		%d0, [%o2]ASI_BLK_P
	add		%o2, VIS_BLOCKSIZE, %o2
	stda		%d16, [%o2]ASI_BLK_P
	add		%o2, VIS_BLOCKSIZE, %o2
	stda		%d32, [%o2]ASI_BLK_P
	membar		#Sync

.do_blkcopy:

#define REALSRC %i0
#define DST	%i1
#define CNT	%i2
#define SRC	%i3

	cmp		CNT, HW_THRESHOLD	! for lesser than HW_THRESHOLD
	blu		%ncc, .slow_copy	!   branch to slow_copy
	  .empty
	
1:	membar		#StoreLoad
	alignaddr	REALSRC, %g0, SRC
	
	! SRC - 8-byte aligned
	! DST - 64-byte aligned
	prefetch	[SRC], #one_read
	prefetch	[SRC + (1 * VIS_BLOCKSIZE)], #one_read
	prefetch	[SRC + (2 * VIS_BLOCKSIZE)], #one_read
	prefetch	[SRC + (3 * VIS_BLOCKSIZE)], #one_read
	ldd		[SRC], %f0
#if CHEETAH_PREFETCH >= 4
	prefetch	[SRC + (4 * VIS_BLOCKSIZE)], #one_read
#endif	
	ldd		[SRC + 0x8], %f2
#if CHEETAH_PREFETCH >= 5
	prefetch	[SRC + (5 * VIS_BLOCKSIZE)], #one_read
#endif	
	ldd		[SRC + 0x10], %f4
#if CHEETAH_PREFETCH >= 6
	prefetch	[SRC + (6 * VIS_BLOCKSIZE)], #one_read
#endif	
	faligndata	%f0, %f2, %f32
	ldd		[SRC + 0x18], %f6
#if CHEETAH_PREFETCH >= 7
	prefetch	[SRC + (7 * VIS_BLOCKSIZE)], #one_read
#endif
	faligndata	%f2, %f4, %f34
	ldd		[SRC + 0x20], %f8
	faligndata	%f4, %f6, %f36
	ldd		[SRC + 0x28], %f10
	faligndata	%f6, %f8, %f38
	ldd		[SRC + 0x30], %f12
	faligndata	%f8, %f10, %f40
	ldd		[SRC + 0x38], %f14
	faligndata	%f10, %f12, %f42
	ldd		[SRC + VIS_BLOCKSIZE], %f0
	sub		CNT, VIS_BLOCKSIZE, CNT
 	add		SRC, VIS_BLOCKSIZE, SRC		! Inc src
 	add		REALSRC, VIS_BLOCKSIZE, REALSRC	! Inc realsrc
	ba,a,pt		%ncc, 2f
	.align	32
2:	
	ldd		[SRC + 0x8], %f2
	faligndata	%f12, %f14, %f44
	ldd		[SRC + 0x10], %f4
	faligndata	%f14, %f0, %f46
	stda		%f32, [DST] ASI_BLK_P		! Store cache line
	ldd		[SRC + 0x18], %f6
	faligndata	%f0, %f2, %f32
	ldd		[SRC +0x20], %f8
	faligndata	%f2, %f4, %f34	
	ldd		[SRC+ 0x28], %f10
	faligndata	%f4, %f6, %f36
	ldd		[SRC  + 0x30], %f12
	faligndata	%f6, %f8, %f38
	ldd		[SRC + 0x38], %f14
	faligndata	%f8, %f10, %f40
	ldd		[SRC + VIS_BLOCKSIZE], %f0
	prefetch	[SRC + (CHEETAH_PREFETCH * VIS_BLOCKSIZE)], #one_read
	faligndata	%f10, %f12, %f42
	sub             CNT, VIS_BLOCKSIZE, CNT		! Dec cnt
	add		DST, VIS_BLOCKSIZE, DST		! Inc dst
	add		REALSRC, VIS_BLOCKSIZE, REALSRC	! Inc dst
	cmp		CNT, VIS_BLOCKSIZE + 8
	bgu,pt		%ncc, 2b
	  add		SRC, VIS_BLOCKSIZE, SRC		! Inc src

	! only if REALSRC & 0x7 is 0
	andcc		REALSRC, 0x7, %g0
	bz		%ncc, 4f
	  nop

3:	
	ldd		[SRC + 0x8], %f2
	faligndata	%f12, %f14, %f44
	ldd		[SRC + 0x10], %f4
	faligndata	%f14, %f0, %f46
	stda		%f32, [DST] ASI_BLK_P		! Store cache line
	ldd		[SRC + 0x18], %f6
	faligndata	%f0, %f2, %f32
	ldd		[SRC +0x20], %f8
	faligndata	%f2, %f4, %f34	
	ldd		[SRC+ 0x28], %f10
	faligndata	%f4, %f6, %f36
	ldd		[SRC  + 0x30], %f12
	faligndata	%f6, %f8, %f38
	ldd		[SRC + 0x38], %f14
	faligndata	%f8, %f10, %f40
	ldd		[SRC + VIS_BLOCKSIZE], %f0
	faligndata	%f10, %f12, %f42
	add		REALSRC, VIS_BLOCKSIZE, REALSRC	! Inc realsrc	
	sub		CNT, VIS_BLOCKSIZE, CNT		! Dec cnt
	add		DST, VIS_BLOCKSIZE, DST		! Inc dst
	faligndata	%f12, %f14, %f44
	faligndata	%f14, %f0, %f46
	stda		%f32, [DST] ASI_BLK_P		! Store cache line
	ba		5f
	  add		SRC, VIS_BLOCKSIZE, SRC		! Inc src
4:
	ldd		[SRC + 0x08], %f2
	fsrc1		%f12, %f44
	ldd		[SRC + 0x10], %f4
	fsrc1		%f14, %f46
	stda		%f32, [DST]ASI_BLK_P
	ldd		[SRC + 0x18], %f6
	ldd		[SRC + 0x20], %f8
	ldd		[SRC + 0x28], %f10
	ldd		[SRC + 0x30], %f12
	ldd		[SRC + 0x38], %f14
	sub		CNT, VIS_BLOCKSIZE, CNT
	add		DST, VIS_BLOCKSIZE, DST
	add		REALSRC, VIS_BLOCKSIZE, REALSRC
	stda		%f0, [DST]ASI_BLK_P
	ba		5f
	  add		SRC, VIS_BLOCKSIZE, SRC		
5:
.copy_exit:
	membar		#StoreLoad|#StoreStore

	! restore fp to the way we got it
	ld		[%fp + STACK_BIAS - BWRITE_FPRS_OFFSET], %i4
	btst		FPRS_FEF, %i4
	bz,a		6f				! branch to label 5
	  nop

	! restore fpregs from stack
	membar		#Sync
	add		%fp, STACK_BIAS - BWRITE_FPREGS_ADJUST, %o2
	and		%o2, -VIS_BLOCKSIZE, %o2
	ldda		[%o2]ASI_BLK_P, %d0
	add		%o2, VIS_BLOCKSIZE, %o2
	ldda		[%o2]ASI_BLK_P, %d16
	add		%o2, VIS_BLOCKSIZE, %o2
	ldda		[%o2]ASI_BLK_P, %d32
	membar		#Sync

	ba		7f	
	  wr		%i4, 0, %fprs			! restore fprs
6:
	FZERO3						! zero all of the fpregs
	wr		%i4, 0, %fprs			! change fpu way it was
7:
	ret
	restore

.slow_copy:

	andcc	REALSRC, 0x7, %g0
	bz	%ncc, 9f
	alignaddr REALSRC, %g0, SRC

8:	cmp	CNT, %g0
	ble,pn	%ncc, .copy_exit
	  nop
	ldd	[SRC], %f0
	ldd	[SRC + 0x08], %f2
	ldd	[SRC + 0x10], %f4
	faligndata %f0, %f2, %f32
	ldd	[SRC + 0x18], %f6
	faligndata %f2, %f4, %f34
	ldd	[SRC + 0x20], %f8
	faligndata %f4, %f6, %f36
	ldd	[SRC + 0x28], %f10
	faligndata %f6, %f8, %f38
	ldd	[SRC + 0x30], %f12
	faligndata %f8, %f10, %f40
	ldd	[SRC + 0x38], %f14
	faligndata %f10, %f12, %f42
	ldd	[SRC + VIS_BLOCKSIZE], %f0
	sub	CNT, VIS_BLOCKSIZE, CNT
	add	SRC, VIS_BLOCKSIZE, SRC
	add	REALSRC, VIS_BLOCKSIZE, REALSRC
	faligndata %f12, %f14, %f44
	faligndata %f14, %f0, %f46
	stda	%f32, [DST]ASI_BLK_P
	ba	8b
	  add	DST, VIS_BLOCKSIZE, DST

	! SRC - 8-byte aligned
	! DST - 64-byte aligned

9:	cmp		CNT, %g0
	ble,pn		%ncc, .copy_exit
	  nop
	ldd		[SRC], %f0
	ldd		[SRC + 0x08], %f2
	ldd		[SRC + 0x10], %f4
	ldd		[SRC + 0x18], %f6
	ldd		[SRC + 0x20], %f8
	ldd		[SRC + 0x28], %f10
	ldd		[SRC + 0x30], %f12
	ldd		[SRC + 0x38], %f14
	sub		CNT, VIS_BLOCKSIZE, CNT
	add		SRC, VIS_BLOCKSIZE, SRC
	add		REALSRC, VIS_BLOCKSIZE, REALSRC
	stda		%f0, [DST]ASI_BLK_P
	ba		9b
	  add		DST, VIS_BLOCKSIZE, DST

	SET_SIZE(wrsmlib_blkwrite)
#endif /* lint */
