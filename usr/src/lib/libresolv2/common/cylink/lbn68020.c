/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * Cylink Corporation � 1998
 * 
 * This software is licensed by Cylink to the Internet Software Consortium to
 * promote implementation of royalty free public key cryptography within IETF
 * standards.  Cylink wishes to expressly thank the contributions of Dr.
 * Martin Hellman, Whitfield Diffie, Ralph Merkle and Stanford University for
 * their contributions to Internet Security.  In accordance with the terms of
 * this license, ISC is authorized to distribute and sublicense this software
 * for the practice of IETF standards.  
 *
 * The software includes BigNum, written by Colin Plumb and licensed by Philip
 * R. Zimmermann for royalty free use and distribution with Cylink's
 * software.  Use of BigNum as a stand alone product or component is
 * specifically prohibited.
 *
 * Disclaimer of All Warranties. THIS SOFTWARE IS BEING PROVIDED "AS IS",
 * WITHOUT ANY EXPRESSED OR IMPLIED WARRANTY OF ANY KIND WHATSOEVER. IN
 * PARTICULAR, WITHOUT LIMITATION ON THE GENERALITY OF THE FOREGOING, CYLINK
 * MAKES NO REPRESENTATION OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR
 * PURPOSE.
 *
 * Cylink or its representatives shall not be liable for tort, indirect,
 * special or consequential damages such as loss of profits or loss of
 * goodwill from the use or inability to use the software for any purpose or
 * for any reason whatsoever.
 *
 * EXPORT LAW: Export of the Foundations Suite may be subject to compliance
 * with the rules and regulations promulgated from time to time by the Bureau
 * of Export Administration, United States Department of Commerce, which
 * restrict the export and re-export of certain products and technical data.
 * If the export of the Foundations Suite is controlled under such rules and
 * regulations, then the Foundations Suite shall not be exported or
 * re-exported, directly or indirectly, (a) without all export or re-export
 * licenses and governmental approvals required by any applicable laws, or (b)
 * in violation of any applicable prohibition against the export or re-export
 * of any part of the Foundations Suite. All export licenses for software
 * containing the Foundations Suite are the sole responsibility of the licensee.
 */
 
/*
 * lbn68020.c - 32-bit bignum primitives for the 68020+ (0r 683xx) processors.
 *
 * Copyright (c) 1995  Colin Plumb.  All rights reserved.
 * For licensing and other legal details, see the file legal.c.
 *
 * This was written for Metrowerks C, and while it should be reasonably
 * portable, NOTE that Metrowerks lets a callee trash a0, a1, d0, d1, and d2.
 * Some 680x0 compilers make d2 callee-save, so instructions to save it
 * will have to be added.
 * 
 * This code supports 16 or 32-bit ints, based on UINT_MAX.
 * Regardless of UINT_MAX, only bignums up to 64K words (2 million bits)
 * are supported.  (68k hackers will recognize this as a consequence of
 * using dbra.)
 *
 * These primitives use little-endian word order.
 * (The order of bytes within words is irrelevant to this issue.)
 *
 * TODO: Schedule this for the 68040's pipeline.  (When I get a 68040 manual.)
 */

#pragma ident	"@(#)lbn68020.c	1.1	00/06/27 SMI"

#include <limits.h>

#include "lbn.h"        /* Should include lbn68020.h */

/*
 * The Metrowerks C compiler (1.2.2) produces bad 68k code for the
 * following input, which happens to be the inner loop of lbnSub1,
 * so a few less than critical routines have been recoded in assembly
 * to avoid the bug.  (Optimizer on or off does not matter.)
 * 
 * unsigned
 * decrement(unsigned *num, unsigned len)
 * {
 *      do {
 *              if ((*num++)-- != 0)
 *                      return 0;
 *      } while (--len);
 *      return 1;
 * }
 */
asm BNWORD32
lbnSub1_32(BNWORD32 *num, unsigned len, BNWORD32 borrow)
{
        movea.l 4(sp),a0        /* num */
#if UINT_MAX == 0xffff
        move.l  10(sp),d0       /* borrow */
#else
        move.l  12(sp),d0       /* borrow */
#endif
        sub.l   d0,(a0)+
        bcc             done
#if UINT_MAX == 0xffff
        move.w  8(sp),d0        /* len */
#else
        move.w  10(sp),d0       /* len */
#endif
        subq.w  #2,d0
        bcs             done
loop:
        subq.l  #1,(a0)+
        dbcc    d0,loop
done:
        moveq.l #0,d0
        addx.w  d0,d0
        rts
}

asm BNWORD32
lbnAdd1_32(BNWORD32 *num, unsigned len, BNWORD32 carry)
{
        movea.l 4(sp),a0        /* num */
#if UINT_MAX == 0xffff
        move.l  10(sp),d0       /* carry */
#else
        move.l  12(sp),d0       /* carry */
#endif
        add.l   d0,(a0)+
        bcc             done
#if UINT_MAX == 0xffff
        move.w  8(sp),d0        /* len */
#else
        move.w  10(sp),d0       /* len */
#endif
        subq.w  #2,d0
        bcs             done
loop:
        addq.l  #1,(a0)+
        dbcc    d0,loop
done:
        moveq.l #0,d0
        addx.w  d0,d0
        rts
}

asm void
lbnMulN1_32(BNWORD32 *out, BNWORD32 const *in, unsigned len, BNWORD32 k)
{
        machine 68020
        
        movem.l d3-d5,-(sp)     /* 12 bytes of extra data */
        moveq.l #0,d4
        move.l  16(sp),a1       /* out */
        move.l  20(sp),a0       /* in */
#if UINT_MAX == 0xffff
        move.w  24(sp),d5       /* len */
        move.l  26(sp),d2       /* k */
#else
        move.w  26(sp),d5       /* len */
        move.l  28(sp),d2       /* k */
#endif

        move.l  (a0)+,d3        /* First multiply */
        mulu.l  d2,d1:d3        /* dc.w    0x4c02, 0x3401 */
        move.l  d3,(a1)+

        subq.w  #1,d5           /* Setup for loop unrolling */
        lsr.w   #1,d5
        bcs.s   m32_even
        beq.s   m32_short
        
        subq.w  #1,d5           /* Set up software pipeline properly */
        move.l  d1,d0
        
m32_loop:
        move.l  (a0)+,d3
        mulu.l  d2,d1:d3        /* dc.w    0x4c02, 0x3401 */
        add.l   d0,d3
        addx.l  d4,d1
        move.l  d3,(a1)+
m32_even:

        move.l  (a0)+,d3
        mulu.l  d2,d0:d3        /* dc.w    0x4c02, 0x3400 */
        add.l   d1,d3
        addx.l  d4,d0
        move.l  d3,(a1)+

        dbra    d5,m32_loop
        
        move.l  d0,(a1)
        movem.l (sp)+,d3-d5
        rts
m32_short:
        move.l  d1,(a1)
        movem.l (sp)+,d3-d5
        rts
}


asm BNWORD32
lbnMulAdd1_32(BNWORD32 *out, BNWORD32 const *in, unsigned len, BNWORD32 k)
{
        machine 68020
        movem.l d3-d5,-(sp)     /* 12 bytes of extra data */
        moveq.l #0,d4
        move.l  16(sp),a1       /* out */
        move.l  20(sp),a0       /* in */
#if UINT_MAX == 0xffff
        move.w  24(sp),d5       /* len */
        move.l  26(sp),d2       /* k */
#else
        move.w  26(sp),d5       /* len */
        move.l  28(sp),d2       /* k */
#endif

        move.l  (a0)+,d3        /* First multiply */
        mulu.l  d2,d1:d3        /* dc.w    0x4c02, 0x3401 */
        add.l   d3,(a1)+
        addx.l  d4,d1

        subq.w  #1,d5           /* Setup for loop unrolling */
        lsr.w   #1,d5
        bcs.s   ma32_even
        beq.s   ma32_short
        
        subq.w  #1,d5           /* Set up software pipeline properly */
        move.l  d1,d0
        
ma32_loop:
        move.l  (a0)+,d3
        mulu.l  d2,d1:d3        /* dc.w    0x4c02, 0x3401 */
        add.l   d0,d3
        addx.l  d4,d1
        add.l   d3,(a1)+
        addx.l  d4,d1
ma32_even:

        move.l  (a0)+,d3
        mulu.l  d2,d0:d3        /* dc.w    0x4c02, 0x3400 */
        add.l   d1,d3
        addx.l  d4,d0
        add.l   d3,(a1)+
        addx.l  d4,d0

        dbra    d5,ma32_loop
        
        movem.l (sp)+,d3-d5
        rts
ma32_short:
        move.l  d1,d0   
        movem.l (sp)+,d3-d5
        rts
}


asm BNWORD32
lbnMulSub1_32(BNWORD32 *out, BNWORD32 const *in, unsigned len, BNWORD32 k)
{
        machine 68020
        movem.l d3-d5,-(sp)     /* 12 bytes of extra data */
        moveq.l #0,d4
        move.l  16(sp),a1       /* out */
        move.l  20(sp),a0       /* in */
#if UINT_MAX == 0xffff
        move.w  24(sp),d5       /* len */
        move.l  26(sp),d2       /* k */
#else
        move.w  26(sp),d5       /* len */
        move.l  28(sp),d2       /* k */
#endif

        move.l  (a0)+,d3        /* First multiply */
        mulu.l  d2,d1:d3        /* dc.w    0x4c02, 0x3401 */
        sub.l   d3,(a1)+
        addx.l  d4,d1

        subq.w  #1,d5           /* Setup for loop unrolling */
        lsr.w   #1,d5
        bcs.s   ms32_even
        beq.s   ms32_short
        
        subq.w  #1,d5           /* Set up software pipeline properly */
        move.l  d1,d0
        
ms32_loop:
        move.l  (a0)+,d3
        mulu.l  d2,d1:d3        /* dc.w    0x4c02, 0x3401 */
        add.l   d0,d3
        addx.l  d4,d1
        sub.l   d3,(a1)+
        addx.l  d4,d1
ms32_even:

        move.l  (a0)+,d3
        mulu.l  d2,d0:d3        /* dc.w    0x4c02, 0x3400 */
        add.l   d1,d3
        addx.l  d4,d0
        sub.l   d3,(a1)+
        addx.l  d4,d0

        dbra    d5,ms32_loop
        
        movem.l (sp)+,d3-d5
        rts
        
ms32_short:
        move.l  d1,d0
        movem.l (sp)+,d3-d5
        rts
}


asm BNWORD32
lbnDiv21_32(BNWORD32 *q, BNWORD32 nh, BNWORD32 nl, BNWORD32 d)
{
        machine 68020
        move.l  8(sp),d0
        move.l  12(sp),d1
        move.l  4(sp),a0
        divu.l  16(sp),d0:d1    /*  dc.w    0x4c6f, 0x1400, 16 */
        move.l  d1,(a0)
        rts
}

asm unsigned
lbnModQ_32(BNWORD32 const *n, unsigned len, unsigned d)
{
        machine 68020
        move.l  4(sp),a0        /* n */
        move.l  d3,a1
#if UINT_MAX == 0xffff
        moveq.l #0,d2
        move.w  8(sp),d1        /* len */
        move.w  10(sp),d2       /* d */
#else
        move.w  10(sp),d1       /* len */
        move.l  12(sp),d2       /* d */
#endif
        dc.w    0x41f0, 0x1cfc  /* lea  -4(a0,d1.L*4),a0 */

	/* First time, divide 32/32 - may be faster than 64/32 */
        move.l  (a0),d3
        divul.l d2,d0:d3        /* dc.w    0x4c02, 0x3000 */
        subq.w  #2,d1
        bmi	mq32_done

mq32_loop:
        move.l  -(a0),d3
        divu.l  d2,d0:d3        /* dc.w    0x4c02,0x3400 */
        dbra    d1,mq32_loop    
                        
mq32_done:
        move.l  a1,d3
        rts
}

/* 45678901234567890123456789012345678901234567890123456789012345678901234567 */
