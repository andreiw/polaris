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

	
/* GENERAL COMMENTS - locore.s is always an interesting exercise. The code here builds but true
 * functionality and actual debugging for portions of it is pending. It is characterized
 * at generally 3 levels 
 * 1 - functional
 * 2 - coded but not tested, 
 * 3 - 2.6 source code that just builds and
 * is here mostly as a placeholder or reference awaiting the real development to start.
 * 
 * The approach has been to engage code in the specific functions when we need to.  
 * 
 * 	
 * The kernel init, exception table install and general level 0,1 exception handlers, and some
 * timer calls have been directly worked on, other areas like interrupts, sys calls, 
 * still have the old 2.6 framework and need attention going forward. References to
 * TRAPTRACE have been removed for the moment but need to be worked back in.
 * KADB has been replaced by KMDB so much of the boot related KADB code has been removed, 
 * however some related items may be laying around again for reference
 * 
 * It should also be noted that CTFmerge/stabs has yet to be ported for PPC. For now we have 
 * reverted to the old 2.6 way of generating assym.h in order to make progress.
 *
 * Note on optimizations: there are none really, we could have done more with macros and inline
 * code but have chosen to get things working for the community's sake and return to enhance things
 * later.
 */
	
/*
 * FYI changes made to to Solaris/PPC 2.6 assembly language,
 * in order to conform to modern AS usage.  Some changes are
 * to ensure correct code for both 32-bit and 64-bit hardware.
 * others are changes in usage of simplified mnemonics.
 *
 *	old	new	no[1]	page[2]	note
 *	-----	-----	--	-----	-----------
 *	cmp	cmpw	10	8-28	cmp, L=0
 *	cmpi	cmpwi	52	8-29	cmpi, L=0
 *	cmpl	cmplw	10	8-30	cmpl, L=0
 *	cmpli	cmplwi	 3	8-31	cmpli, L=0
 *	sl	slw	 3	8-158
 *	sli	slwi	 4	8-155	rlwinm
 *	sri	srwi	 9	8-155	rlwinm
 *
 * [1] no: number of occurrances changed.
 * [2] page: Page number in MPCFPE32B, Rev. 3, 2005/09.
 */

#pragma ident	"%Z%%M%	%I%	%E% SML"

#include <sys/asm_linkage.h>
#include <sys/asm_misc.h>	/* from S10 - empty may use for future assy routines */

#include <sys/reg.h>		/* needed this from /ppc/sys */
#include <sys/psw.h>		/* needed this from /ppc/sys */
#include <sys/spr.h>

#include <sys/stack.h>		/* in-2.6 */
#include <sys/machthread.h>	
#include <sys/cpu.h>		/* added from /ppc/sys but may not need */
#include <sys/trap.h>		/* in-2.6 needed this from /ppc/sys */
#include <sys/traptrace.h>	/* in-2.6 needed this from /ppc/sys */
#include <sys/mmu.h>		/* in-2.6 needed this from /ppc/sys */
#include <sys/clock.h>		/* in-2.6 /prep/sys, PowerPC had it in /ppc/sys */
#include <sys/isa_defs.h>	/* /common/sys but enhanced for PowerPC */


#if defined(lint) || defined(__lint)
#include <sys/types.h>
#include <sys/thread.h>
#include <sys/param.h>
#include <sys/t_lock.h>
#include <sys/promif.h>
#include <sys/time.h>
#else /* lint */
#include "assym.h"
#endif /* lint */

/*
 * the base of the vector table could be 0x0 or 0xfff00000
 * XXX - not used currently in locore.s
 */
#define VECBASE 0x0

/*
 * physical base addresses of additional level0 handlers or sections of
 * them which are copied into low memory.
 * XXX old 2.6 values must revisit after exception table is completed
 */
#define FASTTRAP_PADDR		0x2100
#define TRAP_RTN_PADDR		0x2300
#define RESET_PADDR		0x2500
#define SYSCALL_RTN_PADDR	0x2700
#define HDLR_END		0x28FC	/* last word used by trap handlers */
					/* == (0x2900 - 4) */

/*
 * BAT values for Level 0 handlers to map low memory.
 * XXX old 2.6 values using as placeholders, not valid with the ODW single BAT mappings
 */
#define IBATU_LOMEM_MAPPING	0xfe	/* for PowerPC IBAT Upper */
#define IBATL_LOMEM_MAPPING	0x12	/* for PowerPC IBAT Lower */

/* XXX - Comments below are related to 2.6 and may or may not be relevant dependent on
 * what has been implemented
 *
 * Have eliminated all 601 core references
 * At this time, the PowerPC 603, and 604 are supported.
 * The primary issues that need to be addressed are:
 *
 * The time base frequency needs to be initialized
 * based on platform specific information, presumably made
 * available by Open Firmware.
 *
 * Single threaded interrupt handling should be done when a
 * PANIC is in progress.  This may be doable outside of this
 * code by making all interrupts run at the same level, but
 * this may require checking here.
 *
 * The code the deals with "unsafe" drivers should be removed
 * when unsafe drivers are no longer supported.
 *
 * It appears that the code below the label "bump_dec" is not
 * correct, shown by the instructions that have been commented
 * out.  This needs more careful study when MP support is added.
 * There is more code like this below "clock_done".
 *
 * The code dealing with "release interrupts" should be removed.
 *
 * The level 1 "sys_call" code should be rewritten to perform
 * better.  The current algorithm is from x86, which is very
 * inappropriate.
 *
 * Other review comments:
 *
 * In general, there should more use of Symbolic constants
 * instead of numeric ones.  One example of this is the
 * kernel VSID (Virtual Segment ID) group number, which is 1.
 * Another is the "pvr" register values for identifying which
 * of the PowerPC processors we're running on, 1 for 601, etc.
 * sprg register usage should be symbolic.
 *
 * The clock code could benefit from optimistic masking
 * techniques ... and may need them.  If a clock interrupt is
 * taken while a dispatcher lock is held, it could deadlock
 * trying to get the dispatcher lock.  So, dispatcher locks must
 * disable the clock.  Doing this by setting a mask in a register
 * (an sprg?) or just cpu->cpu_pri would be cheap.  If the clock
 * interrupt occurred while the mask is set, then a pending
 * indication is set somewhere (in the sprg or cpu struct) and
 * the handler returns with the interrupt disabled.  This could
 * be done in the level 0 for the clock.  When the interrupt is
 * enabled again (by splx or disp_lock_exit, etc), the mask is
 * cleared and the pending indication is checked.  If the
 * indication is set, then the interrupt handler is invoked.
 *
 * Since clocks can't be blocked, maybe the level 1 handler
 * should just not fire off the clock thread if clock level
 * interrupts should be blocked by cpu->cpu_pri, but still do
 * everything else (increment time, etc)., and when it is
 * unmasked, the code can invoke the clock thread.
 *
 * Approach taken has done optimistic masking on x86 (and I've been
 * meaning to review his work), and this approach could work
 * for PPC on all interrupts, and could greatly speed spl code.
 *
 * Of course, this can all be done later, except I'm wondering
 * if disp_lock code blocks the clock interrupt somehow.  Isn't
 * this a problem now?
 *
 * This file contains kernel ml startup code, interrupt code,
 * system call and trap ml code, and hrt primitives for PowerPC.
 *
 * This section defines some standard Solaris symbols and the
 * stack and thread structure for thread 0.
 */

/* XXX - this section is used as is from 2.6 but removed sysbase, syslimit */


#if !defined(lint) && !defined(__lint)
	.data
 	.align	2

	.globl	t0stack
	.type	t0stack,@object
	.align	4		/* stacks are 16-byte (2**4 bytes) aligned */
t0stack:.skip	T0STKSZ		/* thread 0's stack should be the 1st thing */
				/* in the data section */

	.align	5		/* thread structures are 32-byte aligned */
	.globl	t0
t0:	.skip	THREAD_SIZE	/* set in genassym */

	.globl	Kernelbase;
	.type	Kernelbase,@object;
	.size	Kernelbase,0;
	.set	Kernelbase, KERNELBASE

	.global	msgbuf
	.type	msgbuf, @object;
	.size	msgbuf, MSGBUFSIZE;
	.set	msgbuf, V_MSGBUF_ADDR	/* address of printf message buffer */


/* XXX - have omitted the #ifdef TRAPTRACE in the 2.6 code for now */

/*
 * Terminology used to describe levels of exception handling:
 *
 * Level 0 - ml code running with instruction relocation off
 * Level 1 - ml code running with instruction relocation on
 * Level 2 - C code
 *
 * The number 1 priority use for the 4 sprg registers is to make
 * the transition out of level 0 as efficient as possible.  The
 * current use of these registers is as follows:
 *
 * sprg0 - curthread (virtual)
 * sprg1 - curthread->t_stack (virtual)
 * sprg2 - CPU (physical)
 * sprg3 - scratch
 *
 * The transition from level 0 to level 1 code is done with a
 * a blr by putting the addr of the appropriate level-1 handler
 * in the LR.
 *
 * Level 0 handlers are copied into physical addresses 0x100, 0x200,
 * ... during kernel initialization.
 *
 * There is the following static data that is needed to make an
 * efficient transition from level 0 code to level 1 code.  It is
 * copied from the statically defined data into the CPU structure
 * during initialization.  This is described just below the next
 * section which defines the first stuff in the data section.
 *
 *
 * structure member Entry points and MSRs
 * -------------------------------------------------------------
 * CPU_CMNTRAP - &cmntrap (always handled by the kernel)
 * CPU_SYSCALL - &sys_call
 * CPU_CMNINT - &cmnint
 * CPU_CLOCKINTR - &clock_intr
 *
 * CPU_MSR_ENABLED - MSR with interrupts enabled
 * CPU_MSR_DISABLED - MSR with interrupts disabled
 *
 */


level0_2_level1:
	.long	cmntrap
	.long	0
	.long	sys_call
	.long	cmnint
	.long	clock_intr

	.long	MSR_EE|MSR_ME|MSR_IR|MSR_DR|MSR_FP	/* level 1 bits needed */
	.long	MSR_ME|MSR_IR|MSR_DR|MSR_FP		/* level 1 bits needed */

#endif /* lint */

/*
 * _start() - Kernel initialization
 *
 * Our assumptions:
 * - We are running with virtual addressing.
 * - Interrupts are disabled.
 * - The kernel's text, initialized data and bss are mapped.
 *
 * Our actions:
 * - Save arguments
 * r3 - empty
 * r4 - empty
 * r5 - &cif_handler of secondary boot for inetboot or ODW OF
 * r6 - &bootops
 *
 * r7 - empty
 * 
 * - Initialize our stack pointer to the thread 0 stack (t0stack)
 * and leave room for a phony "struct regs".
 *
 *
 * (NOTE: we have dumped all KADB related items from 2.6 )
 * - Check for cpu type, is done is cpuid_pass1()
 * - mlsetup(sp, cif_handler) gets called.
 * - We change our appearance to look like the real thread 0.
 * Initialize sprg0, sprg1, and sprg2
 * - Setup Level 0 interrupt handlers
 * - main() gets called.  (NOTE: main() never returns).
 *
 */

/*
 * NOW, the real code!
 */
	/*
	 * Added globals:
	 */

 	.global _start
	.global	mlsetup
	.global	get_cpu_id	/* added for call to mlsetup */
	.global	main
	.global	panic
	.global	sysp
	.global	edata		/* define edata (referenced for /dev/ksyms) */
	.global	cpus
	.global	cifp

	.comm	cif_handler,4,2
	.comm	cifp,4,2
	.comm	bootopsp,4,2
	.comm	edata,4,2


#if defined(lint)

void
_start(void)
{}

#else /* lint */

/* _start() - Kernel Initialization */


	.text
	ENTRY_NP(_start)
	bl	_start1			/* get the following addresses */
	.long	edata			/* define edata (referenced for /dev/ksyms */
	.long	0			/* placeholder for old KABD value */
	.long	t0stack			/* r16 */
	.long	t0			/* r2 (t0 is curthread) */
	.long	level0_2_level1		/* r18 */
 	.long	cpus			/* r20 - CPU */

_start1:
	mflr	r14			/* points back to array of pointers */
	lwz	r15,4(r14)
	lwz	r16,8(r14)
	lwz	THREAD_REG,12(r14)
	lwz	r18,16(r14)
	lwz	r20,20(r14)

/*
 * - Save arguments, and we don't check for kadb anymore
 * r5 - &cif_handler
 * r5 - &bopp
 */

	lis	r11,cifp@ha		/* save pointer to caller's cif_handler */
	stw	r5,cifp@l(r11)		/* This is in R5 now */
	mr	r31,r5			/* save value of cif_handler in r31 */
	nop
	
 	lis	r11,bootopsp@ha		/* PPC nfsboot will leave *bootops in r6, elfentry is r7 */
 	stw	r6,bootopsp@l(r11)
 	lis	r11,bootops@ha
 	stw	r6,bootops@l(r11)


/* - Initialize our stack pointer to the thread 0 stack (t0stack)
 * and leave room for a phony "struct regs".
 */
	addi	r1,r16,SA(T0STKSZ-REGSIZE-MINFRAME)

	lwz	r3,20(r18)
	or	r3,r3,r4		/* current msr "or" interrupts enabled */
	stw	r3,CPU_MSR_ENABLED(r20)
	lwz	r3,24(r18)
	or	r3,r3,r4		/* current msr "or" interrupts disabled */
	stw	r3,CPU_MSR_DISABLED(r20)
	lwz	r3,0(r18)		/* "cmntrap" */
	stw	r3,CPU_CMNTRAP(r20)
	lwz	r3,8(r18)		/* "sys_call" */
	stw	r3,CPU_SYSCALL(r20)
	lwz	r3,12(r18)		/* "cmnint" */
	stw	r3,CPU_CMNINT(r20)
	lwz	r3,16(r18)		/* "clock_intr" */
	stw	r3,CPU_CLOCKINTR(r20)

/* 	bl	clear_bss	*/	/* Clear the BSS - this was done to allow 
					 * the kernel to boot standalone w/o the JTAG
					 * debugger. KRTLD usually does this but we
					 * don't have it right now
					 */

	/* Need to call prom_init() before using promif library.
	 * This is done after clearing BSS and before first printing. */
	lis	r3, pgmname@ha
	la	r3, pgmname@l(r3)
	mr	r4, r31
	bl	prom_init

/* 	bl	print_bss	*/	/* Display BSS info */

	/* - Check for cpu type, MPC74xx or G4 family */
	bl      cpuid_pass1             /* got to cpuid and figure it out */

	cmpwi   r3,0                    /* return value of 0 is not good */

	beq-	.unsupported_cpu
	b	.cpu_identified

.unsupported_cpu:
	/* PANIC: Unknown CPU type */

	lis	r3, badcpu@ha
	la	r3, badcpu@l(r3)
	mfpvr	r0
	srwi	r4,r0,16
 	bl	prom_printf		/* print PANIC message */
1:
	b	1b			/* loop for ever...*/
	.data
badcpu:
	.string "\n\nPANIC: Unsupported CPU type with PVR value	d.\n\nHalted."
pgmname:
	.string "kernel"
	.text

.cpu_identified:
	lis	r11,cputype@ha		/* right now just saving PVR value */
	sth	r3,cputype@l(r11)

	/* - mlsetup(sp, cif_handler) gets called. */
	mr	r3,r1			/* pass in the current stack pointer. */
					/* NOTE that it points to Thread 0's stack. */
	mr	r4,r31			/* cif_handler */
	bl	mlsetup

/*	 - We change our appearance to look like the real thread 0.
 *	We assume that mlsetup has set t0->t_stk and t0->t_cpu.
 *	For PowerPC, this is a good time to Initialize sprg0,
 *	sprg1 and sprg2.
 *
 *	sprg0 - curthread (virtual)
 *	sprg1 - curthread->t_stack (virtual) (Really, sprg1
 *	is the kernel stack address when a transition
 *	from user mode to kernel mode occurs)
 *	sprg2 - CPU + CPU_R3 (physical)
 *	sprg3 - scratch
 */
	lwz	r1,T_STACK(THREAD_REG)
	li	r0,0

	mtsprg	0,THREAD_REG		/* curthread is thread0 */
	mtsprg	1,r1
	stw	r0,0(r1)		/* terminate t0 stack trace */
	stw	r0,4(r1)
/*	bl	check_OF_revision */     /* XXX - this was VOF specific probably won't need */

	bl	ppcmmu_init
	lwz	r3,T_CPU(THREAD_REG)	/* adjust it so kadb can do store at offset 0 */
	addi	r3,r3,CPU_R3
	bl	va_to_pa
	mtsprg	2,r4			/* returning a 64bit value */

	bl	leave_locore		/* Display status message */

/* 	- main() gets called.  (NOTE: main() never returns). */
	bl	main
 	.long	0			/* a guaranteed illegal instruction!? */
	/* NOTREACHED */

	SET_SIZE(_start)

/*
 * Define macros to allow SPRG2 to point to physical save area
 * with an offset of 0 instead of an offset of CPU_R3 for
 * compatibility with kadb's offsets 0, 4, and 8.
 */

#define L0_R3	CPU_R3 - CPU_R3
#define L0_SRR0 CPU_SRR0 - CPU_R3
#define L0_SRR1 CPU_SRR1 - CPU_R3
#define L0_DSISR CPU_DSISR - CPU_R3
#define L0_DAR	CPU_DAR - CPU_R3

/*
 * Macros describing level 0 handlers and other code to be copied to low
 * memory.  These allow different code to be copied down depending on the
 * platform and other choices.
 *
 * The list of handlers to copy contains a pointer the the code	description,
 * and the low-memory destination address.
 *
 * The code description consists of a number of sections, terminated by a
 * zero-length section.  Each section has a simple header containing the
 * byte count.
 * The headers aren't copied.
 *
 * Think of these sections as startup-time #ifdefs.
 */

/*
 * HANDLER(vector, desc).
 * vector = the low memory physical address for the handler.
 * desc = pointer to the SECTION_LIST comprising the handler.
 */

#define	HANDLER(vector, desc)						\
	.long	vector;		/* copy to this address */		\
	.long	desc;		/* copy	from this code section list */

/*
 * SECTION_LIST(label).
 * label = address of the SECTION of code for the handler.
 * The section list is terminated by a 0 section.
 */

#define	SECTION_LIST(label)	\
	.long	label 		/* address of section */

/*
 * SECTION(label, label_end).
 * label = label for this section.
 * label_end = this is length in bytes calculated at the end of the section.
 * 
 *
 */
	
#define SECTION(label, label_end)	\
	.long	label;			\
	.long	label_end;		\
label:

/*
 * SECTION_END(label_start, label_end).
 * End of a section of a handler, calculate the length
 * 
 */

#define	SECTION_END(label_start, label_end)	\
	.set	label_end, .-label_start;	\
	.long	label_end;


/*
 * Table of level 0 handlers.
 * 
 */
	.text
level0_handlers:
	HANDLER(0x100, .L0_common)	/* soft reset */
	HANDLER(0x200, .L0_common)	/* machine check */
	HANDLER(0x300, .L0_common)	/* data storage exception */
	HANDLER(0x400, .L0_common)	/* instruction storage exception */
	HANDLER(0x500, .L500)		/* external interrupt */
	HANDLER(0x600, .L0_common)	/* alignment */
	HANDLER(0x700, .L0_common)	/* program interrupt */
	HANDLER(0x800, .L0_common)	/* floating-point unavailable */
	HANDLER(0x900, .L900)		/* decrementer interrupt */
	HANDLER(0xa00, .L0_common)	/* reserved */
	HANDLER(0xb00, .L0_common)	/* reserved */

	/* XXX system call handler is not complete */
	
	HANDLER(0xc00, .Lc00)		/* system call */
	HANDLER(0xd00, .L0_common)	/* trace */
	/*
	 * TLB miss handlers (for the 603) are installed by the secondary
	 * boot while running without address translation enabled.
	 */
	
	/* 1000 instruction TLB miss (603) */
	/* 1100 data fetch TLB miss (603) */
	/* 1200 data store TLB miss (603) */
	HANDLER(0x1300,	.L0_common)		/* breakpoint exception (603) */
	HANDLER(0x1400,	.L0_common)		/* System Management Interrupt (603) */

	HANDLER(0, 0)			/* list terminator */

	/* XXX these handlers need to be sorted out */

	HANDLER(0x2000, .L0_common)		/* run mode exception (601-only) */
 	HANDLER(FASTTRAP_PADDR, .Lfasttrap)	/* fast trap */
	HANDLER(TRAP_RTN_PADDR, .L_trap_rtn)	/* trap return */
 	HANDLER(RESET_PADDR, .hard_reset)	/* reboot/reset */
 	HANDLER(SYSCALL_RTN_PADDR, .L_syscall_rtn) /* syscall return */
	HANDLER(HDLR_END, .dummy_hdlr)		/* dummy handler for size check */
	HANDLER(0, 0)				/* list terminator */

.dummy_hdlr:
	SECTION_LIST(0)				/* empty list for size check */


/*	-----------------------------------------------------------------------
 * Copy level 0 handlers into low physical memory
 *	-----------------------------------------------------------------------
 */

/* Register Usage
 * r3 - source
 * r4 - destination
 * r5 - tmp during word copy
 * r6 - pointer into level0_handlers table
 * r7 - pointer to list of sections
 * r8 - section end terminator, byte size check
 * r9 - size in bytes
 * r10 - old kadb_entry (not used)
 * r11,r12 saved BAT entry. Caller doesn't mind if I corrupt r11,r12
 * ctr - word count for copy
 */

/* this is called from mlsetup.c  */

	ENTRY_NP(copy_handlers)
	
#ifdef XXXPPC
/* XXX here for reference only right now there is only one BAT allocated 
 * for mem and it is mapped 1:1 on the ODW currently so this will get 
 * sorted out eventually 
 */
1:	mfspr	r11, DBAT2U		/* Save the BAT */
	mfspr	r12, DBAT2L		/* Save the BAT */
	li	r0, IBATU_LOMEM_MAPPING
	mtspr	DBAT2U, r0		/* set DBAT2 to map low memory */
	li	r0, IBATL_LOMEM_MAPPING
	mtspr	DBAT2L, r0
	isync
#endif
	andis.	r8, r8, 0xffff		/* clear revision */
	lis	r6, level0_handlers@ha
	la	r6, level0_handlers@l(r6)
	subi	r6, r6, 4		/* pre decrement the ptr */

	/*
	 * Copy next handler
	 */
	li	r4, 0			/* clear previous destination */
.h0_loop1:
	lwzu	r5, 4(r6)		/* destination address in low memory */
	lwzu	r7, 4(r6)		/* source entry table */
	cmpwi	r7, 0
	beq-	.h0_done
	subi	r7, r7, 4		/* pre-decrement section list */
	cmplw	r5, r4			/* compare new destination with old */
	blt	.h0_error		/* error, overlap */
	subi	r4, r5, 4		/* pre-decrement destination */

	/* Copy next section */
.h0_loop3:
	lwzu	r3, 4(r7)		/* load pointer to section */
	cmpwi	r3, 0
	beq-	.h0_loop1		/* done get next handler entry */
/* 	subi	r3, r3, 4	*/	/* pre-decrement section pointer */
	lwzu	r9, 4(r3)		/* load size in bytes (not including header */

	/* Copy one section */

.h0_loop4:
	cmplw	r9, r8			/* check original size with end of section byte tag */
	beq-	.h0_loop3		/* done with this section */
	srwi	r5, r9, 2		/* size in words */
	mtctr	r5			/* setup counter for copy */

	/* copy part of section, cnt words from r3 to r4. */

.h0_loop2:
	lwzu	r5,4(r3)
	stwu	r5,4(r4)
	dcbst	r0,r4			/* force to memory */
	sync				/* guarantee dcbst is done before icbi */
	icbi	r0,r4			/* force out of instruction cache */
	bdnz	.h0_loop2
	lwzu	r8,4(r3)		/* load in the end of section byte check */
	b	.h0_loop4
	/* copy next section */

.h0_error:
	lis	r3, .copy_msg@ha
	la	r3, .copy_msg@l(r3)
	bl	prom_printf		/*Print error message */
	b	.			/* hang */


.h0_done:
	sync				/* one sync for the last icbi */

#ifdef XXXPPC
/* XXX here for reference only right now there is only one BAT and it is 1:1 see above */
	mtspr DBAT2U, r11		/* restore DBAT2 */
	mtspr DBAT2L, r12
#endif
	
	isync
	blr				/* return */
	SET_SIZE(copy_handlers)

	.data
.copy_msg:
	.asciz	"copy_handlers: level0 handler overlap at x\n"
	.text

/*
 * XXX - Omitted since we do not support 601 anymore
 * Code patching routine.
 * Used to patch BAT values for 601 machines, placeholder for now
 *
 */
	ENTRY_NP(_patch_code)
	b	.		/* hang */
	SET_SIZE(_patch_code)



#endif /* lint */


/*
 * Stack frames for PowerPC exceptions (including interrupts)
 *
 * Whenever an exception occurs, the level 0 code creates a stack
 * frame to save the current state of activity.  The saved information
 * is known as the "regs" structure (see sys/reg.h).  Additionally, an
 * additional stack frame is created that is often used by C functions
 * that are called from this machine language code.  The following
 * diagrams shows the stack frame created:
 *
 * r1 -> 0 - back chain (0)
 * 4 - reserved for callee's LR
 * Next is the regs structure
 * 8-164
 *
 * Thus we need SA(MINFRAME + REGSIZE) = 176 bytes for the stack frames.
 * When in kernel mode, we just decrement the existing kernel mode
 * stack pointer (r1) by SA(MINFRAME + REGSIZE) to create the new stack
 * frame, while if we were running in user mode, we just grab the
 * value in sprg1 that we have previously set up for this purpose.
 *
 * Here we define the byte offsets used for saving the various
 * registers during exception handling.
 */

#define REGS_R0	(R_R0 * 4 + MINFRAME)
#define REGS_R1	(R_R1 * 4 + MINFRAME)
#define REGS_R2 (R_R2 * 4 + MINFRAME)
#define REGS_R3 (R_R3 * 4 + MINFRAME)
#define REGS_R4 (R_R4 * 4 + MINFRAME)
#define REGS_R5 (R_R5 * 4 + MINFRAME)
#define REGS_R6 (R_R6 * 4 + MINFRAME)
#define REGS_R7 (R_R7 * 4 + MINFRAME)
#define REGS_R8 (R_R8 * 4 + MINFRAME)
#define REGS_R9 (R_R9 * 4 + MINFRAME)
#define REGS_R10 (R_R10 * 4 + MINFRAME)
#define REGS_R11 (R_R11 * 4 + MINFRAME)
#define REGS_R12 (R_R12 * 4 + MINFRAME)
#define REGS_R13 (R_R13 * 4 + MINFRAME)
#define REGS_R14 (R_R14 * 4 + MINFRAME)
#define REGS_R15 (R_R15 * 4 + MINFRAME)
#define REGS_R16 (R_R16 * 4 + MINFRAME)
#define REGS_R17 (R_R17 * 4 + MINFRAME)
#define REGS_R18 (R_R18 * 4 + MINFRAME)
#define REGS_R19 (R_R19 * 4 + MINFRAME)
#define REGS_R20 (R_R20 * 4 + MINFRAME)
#define REGS_R21 (R_R21 * 4 + MINFRAME)
#define REGS_R22 (R_R22 * 4 + MINFRAME)
#define REGS_R23 (R_R23 * 4 + MINFRAME)
#define REGS_R24 (R_R24 * 4 + MINFRAME)
#define REGS_R25 (R_R25 * 4 + MINFRAME)
#define REGS_R26 (R_R26 * 4 + MINFRAME)
#define REGS_R27 (R_R27 * 4 + MINFRAME)
#define REGS_R28 (R_R28 * 4 + MINFRAME)
#define REGS_R29 (R_R29 * 4 + MINFRAME)
#define REGS_R30 (R_R30 * 4 + MINFRAME)
#define REGS_R31 (R_R31 * 4 + MINFRAME)
#define REGS_PC	 (R_PC * 4 + MINFRAME)
#define REGS_CR  (R_CR * 4 + MINFRAME)
#define REGS_LR  (R_LR * 4 + MINFRAME)
#define REGS_MSR (R_MSR * 4 + MINFRAME)
#define REGS_CTR (R_CTR * 4 + MINFRAME)
#define REGS_XER (R_XER * 4 + MINFRAME)

/*
 * For all level 0 exception handlers, the following is the state when
 * transferring control to level 1.
 *
 * r1 - points to a MINFRAME sized stack frame, followed
 * by a register save area ("struct regs" described
 * in reg.h) on the current kernel stack.
 *
 * the register save area is filled in with saved values for
 * R1, R2, R4, R5, R6, R20, CR, PC (saved srr0), and MSR (saved
 * srr1).  The other registers are saved in the level 1 code.
 *
 * r2 - curthread
 *
 * r4 - trap type (same as interrupt vector, see trap.h)
 *
 * r5 - value of the DAR (when trap type is a data fault)
 *
 * r6 - value of the DSISR (when trap type is a data fault
 * or an alignment fault)
 *
 * r20 - CPU
 *
 * The other interrupt handlers are invoked with a single argument
 * that was provided when the interrupt routine was registered via
 * ddi_add_intr().  When we go from level 0 to level 1 for non-clock
 * interrupts, there is nothing special needed to be done.  The
 * level 1 code needs to read the vector number provided in a
 * platform-specific manner, e.g., read a on-board status register.
 *
 *-------------------------------------------------------------------
 *
 * Level 0 exception handlers (they need to be copied to low memory).
 *
 * Tactics used include:
 *
 * minimize extra stores to memory (use proper kernel stack)
 * loading special registers into proper greg for call to trap()
 * enable interrupts fairly quickly in level-1 (cmntrap)
 * be MP-safe (only use sprgN and/or per-cpu locations as temps)
 *
 * The solution is close to optimum unless there is a penalty for
 * instruction execution with IR disabled.  If we determine that
 * this is the case, we should revisit this code.
 *
 * Some instruction reordering can be done for improvement.
 * This has not yet been done to keep things clean for now.
 *
 * It is assumed that trap() is called as follows:
 *
 * trap(rp, type, dar, dsisr, cpuid)
 *
 * where:
 *
 * rp - pointer to regs structure
 * type - trap type (vector number, e.g., 0x100, 0x200, ...)
 * dar - DAR register (traps 0x300 and 0x600)
 * dsisr - DSISR register (traps 0x300 and 0x600)
 * cpuid - processor ID of the CPU that has trapped
 *
 * XXX PPC - use BAT 0 for better performance.
 */

/*
 * .L0_common()
 *
 * Common Level 0 Exception handler.
 * This basically saves the state and calls the level 1 common trap handler.
 * Currently the following exceptions are handled by this:
 *	0x0100 Soft Reset exception trap (NMI trap?)
 *	0x0200 Machine check
 *	0x0300 Data Storage Exception
 *	0x0400 Instruction Storage Exception
 *	0x0600 Alignment
 *	0x0700 Program Interrupt
 *	0x0800 Floating Point Unavailable Exception
 *	0x0a00 Reserved
 *	0x0b00 Reserved
 *	0x1300 Instruction Address Breakpoint Exception (603 specific)
 *	0x1400 System Management Interrupt (603 specific)
 *
 * NOTE: When special handling is required for any of the above	exceptions
 * then they need seperate handlers.
 */


#if !defined(lint) && !defined(__lint)
	.text	
.L0_common:
	SECTION_LIST(.L0_entry)
	SECTION_LIST(.L0_cmntrap)
	SECTION_LIST(0)
	
.L0_entry:
	SECTION(.L0_entry1, .L0_entry1_end)

/* States are:
 *	's'means saved,
 *	't' in tmp,
 *	'T' means in 2nd tmp,
 *	'Z' means in last temp, and
 *	'.' means needs saving.
 */

	.align	4
				/* 1245630pmc */
	mtsprg	3,r1		/* t......... */
	mfsprg	r1,2		/* t......... */
	stw	r3,L0_R3(r1)	/* tt........ */
	mfsrr0	r3		/* tt.....t.. */
	stw	r3,L0_SRR0(r1)	/* tt.....T.. */
	mfsrr1	r3		/* tt.....Tt. */
	stw	r3,L0_SRR1(r1)	/* tt.....TT. */
	mfdsisr r3
	stw	r3,L0_DSISR(r1)
	mfdar	r3
	stw	r3, L0_DAR(r1)
	mfmsr	r1		/* tt.....TT. */
	ori	r1,r1,MSR_DR	/* enable virtual addressing for data */
				/* We need to save CR before we use it below */
	mfcr	r3		/* tt.....TTt */
	mtmsr	r1		/* tt.....TTt */
	mfsrr1	r1		/* tt.....TTt */
	andi.	r1,r1,MSR_PR	/* was in kernel mode? */
	beq-	1f		/* If not, so use the bottom of the kernel stack */
	lis	r1, 0x2000
	ori	r1, r1, 0x1e
	isync
	mtsr	14, r1
	ori	r1, r1,	0x1f	/* set SR14,15 with kernel SR values */
	mtsr	15, r1
	isync
	mfsprg	r1,1		/* curthread->t_stack */
	b	2f
1:
				/* YES, so make room for a new kernel stack frame */
	mfsprg	r1,3		/* get old kernel stack pointer, then adjust it */
	subi	r1,r1, SA(REGSIZE + MINFRAME) /* make room for registers */
2:
	
/* 	r1 now points to a valid kernel stack frame for saving everything. */
	
					/* 1245630pmc */
					/* tt.....TTt */
	stw	THREAD_REG,REGS_R2(r1)	/* tt...s.TTt make room for curthread */
	stw	r20,REGS_R20(r1)	/* tt...ssTTt make room for CPU */
	stw	r3,REGS_CR(r1)		/* tt...ssTTs save CR */
	mfsprg	THREAD_REG,0		/* establish curthread in THREAD_REG */
	stw	r4,REGS_R4(r1)		/* tts..ssTTs */
	lwz	r20,T_CPU(THREAD_REG)	/* establish CPU in r20 */
	stw	r6,REGS_R6(r1)		/* tts.sssTTs */
	lwz	r3,CPU_SRR0(r20)	/* tts.sssZTs */
	stw	r5,REGS_R5(r1)		/* ttsssssZTs */
	lwz	r4,CPU_SRR1(r20)	/* ttsssssZZs */
	mfsprg	r6,3			/* TtsssssZZs */
	stw	r3,REGS_PC(r1)		/* TtssssssZs save PC */
	stw	r4,REGS_MSR(r1)		/* Ttssssssss save MSR */
	stw	r6,REGS_R1(r1)		/* stssssssss */

#ifdef XXXPPC
.patch_batu1:
	li	r3, IBATU_LOMEM_MAPPING
.patch_batl1:
	li	r4, IBATL_LOMEM_MAPPING /* XXX - just testing for now */
	mtspr	IBAT2U, r3		/* ODW's OF is not using IBATU2 */
	mtspr	IBAT2L, r4		/* ODW's OF is not using IBATL2 */
#endif
	
	mflr	r4
	stw	r4, REGS_LR(r1)		/*  save LR */
	SECTION_END(.L0_entry1, .L0_entry1_end)

.L0_cmntrap:
	SECTION(.L0_cmntrap1, .L0_cmntrap_end)
	lwz	r6,CPU_MSR_DISABLED(r20)/* MSR for traps */
	lwz	r5,CPU_CMNTRAP(r20)	/* address of cmntrap() */
	lwz	r3,CPU_R3(r20)		/* ssssssssss */
	mtlr	r5
	mtmsr	r6			/* MSR_IR on */
	isync				/* MSR_IR takes effect */
	blrl				/* jump to cmntrap and put type in lr */
	SECTION_END(.L0_cmntrap1, .L0_cmntrap_end)


/*
 * Hard Reset - Jump to ROM
 */

/* XXX - merged from 2.6, haven't tested or even looked at carefully */

	
.hard_reset:
	SECTION_LIST(.hard_resets)
	SECTION_LIST(0)

/*
 * Hard Reset for 603/604 - Jump to ROM
 */

	SECTION(.hard_resets, .hard_reset_end)
	SECTION_END(.hard_resets, .hard_reset_end)	/* continues with .hard_reset_cmn */


	SECTION(.hard_reset_std, .hard_reset_std_end)

	mfspr	r2,SPR_HID0
	ori	r2,r2,0x0800		/* ICFI on */
	rlwinm	r2,r2,0x0,0x11,0xf	/* ICE off */
	isync
	mtspr	SPR_HID0,r2		/* I-cache is disabled */

	rlwinm	r2,r2,0x0,0x12,0x10	/* DCE off */
	ori	r2,r2,0x0400		/* DCFI on */
	isync
	mtspr	SPR_HID0,r2			/* D-cache is disabled */
	sync
	sync
	nop
	nop
	nop

	mfmsr	r2
	rlwinm	r2, r2,0,0,30		/* LE off */
	rlwinm	r2, r2,0,16,14		/* ILE off */
	li	r5,0x0000
	lis	r29, PCIISA_VBASE >> 16
	li	r28, 0x95
	sync
	mtmsr	r2
	isync
	sync
	sync
	sync
	sync
	stbx	r5, r28, r29
	sync

	SECTION_END(.hard_reset_std, .hard_reset_std_end)


	SECTION(.hard_reset_cmn, .hard_reset_cmd_end)

	/* -------------------------------------------------------------
	 * String of palindromic instructions that work the same
	 * regardless of whether they are fetched and interpreted in
	 * Big-Endian mode or Little-Endian mode.
	 * Somewhere in the midst of this list the machine actually gets
	 * itself into Big-Endian mode; further assembly is done for
	 * Big-Endian.
	 * -------------------------------------------------------------
	 */

	addi	r0, r1, 0x138
	addi	r0, r1, 0x138
	addi	r0, r1, 0x138
	addi	r0, r1, 0x138
	addi	r0, r1, 0x138
	addi	r0, r1, 0x138
	addi	r0, r1, 0x138
	addi	r0, r1, 0x138
	addi	r0, r1, 0x138
	addi	r0, r1, 0x138
	addi	r0, r1, 0x138
	addi	r0, r1, 0x138
	addi	r0, r1, 0x138
	addi	r0, r1, 0x138

/* 	.endian big */	/* switch the assembler to Big-Endian mode */

	addi	r0, r1, 0x138
	addi	r0, r1, 0x138
	addi	r0, r1, 0x138
	addi	r0, r1, 0x138
	addi	r0, r1, 0x138
	addi	r0, r1, 0x138
	addi	r0, r1, 0x138
	addi	r0, r1, 0x138
	addi	r0, r1, 0x138
	addi	r0, r1, 0x138
	addi	r0, r1, 0x138
	addi	r0, r1, 0x138

	mfmsr	r0
	ori	r0,r0,0x3000
	andi.	r0,r0,0x7fcf
	mtsrr1	r0
	mtmsr	r0
	isync
	mtsrr1	r0
	lis	r0,EXT16(0xfff0)
	ori	r0,r0,0x0100
	mtsrr0	r0
	rfi
 /*	.endian little */	/* switch the assembler to Little-Endian mode */
	SECTION_END(.hard_reset_cmn, .hard_reset_cmd_end )

/*
 * External Interrupts
 */

	.align 4

.L500:
	SECTION_LIST(.L0_entry)
	SECTION_LIST(.L0_cmnint)
	SECTION_LIST(0)
	
.L0_cmnint:
	SECTION(.L0_cmnint1, .L0_cmnint_end)
	lwz	r4,CPU_MSR_DISABLED(r20)	/* MSR for interrupts */
	lwz	r5,CPU_CMNINT(r20)		/* cmnint code address */
	lwz	r3,CPU_R3(r20)			/* ssssssssss */
	mtlr	r5
	mtmsr	r4				/* MSR_IR on */
	isync					/* MSR_IR takes effect */
	blrl					/* branch high to cmnint */
	SECTION_END(.L0_cmnint1, .L0_cmnint_end)

/* XXX - not used at the moment since there ias no kadb, will cleanup later
 * Level-0 kadb trap.
 *	Level 0 handler which allows kadb (if present) to intercept the trap.
 *	Otherwise, same as L0_common.
 * 	Used for:
 *	700	Program interrupt
 *	d00	Trace interrupt
 *
 */

	.align	4

.L0_common_kadb:
	SECTION_LIST(.L0_entry)		/* standard trap entry code */
	SECTION_LIST(.L0_no_kadb)	/* no kadb if present */
	SECTION_LIST(0)

/*
 * Section to enter --  cmntrap section is used.
 */

.L0_no_kadb:
	SECTION(.L0_no_kadb1, .L0_no_kadb_end)
	lwz	r6,CPU_MSR_DISABLED(r20)	/* MSR for traps */
	lwz	r5,CPU_CMNTRAP(r20)		/* address of cmntrap() */
	lwz	r3,CPU_R3(r20)		/* ssssssssss */
	mtlr	r5
	mtmsr	r6			/* MSR_IR on */
	isync				/* MSR_IR takes effect */
	blrl				/* jump to cmntrap and put type in lr */
	SECTION_END(.L0_no_kadb1, .L0_no_kadb_end)


/*
 * Decrementer Interrupt
 */
	.align 4

.L900:
	SECTION_LIST(.L0_entry)
	SECTION_LIST(.L0_decr)
	SECTION_LIST(0)

.L0_decr:	
	SECTION(.L0_decr1, .L0_decr_end)
	lwz	r4,CPU_MSR_DISABLED(r20)	/* MSR for clock interrupt */
	lwz	r5,CPU_CLOCKINTR(r20)	/* clock_intr code address */
	lwz	r3,CPU_R3(r20)		/* ssssssssss */
	mtmsr	r4			/* MSR_IR on */
	isync				/* MSR_IR takes effect */
	mtlr	r5
	blrl				/* branch high to clock_intr() */
	SECTION_END(.L0_decr1, .L0_decr_end)

/*
 * System Call Interrupt
 * XXX - 2.6 code framework / placeholder only... really haven't looked at it yet... surely needs work
 */

.Lc00:
	SECTION_LIST(.L0_fastcheck)
	SECTION_LIST(.L0_entry)
	SECTION_LIST(.L0_syscall)
	SECTION_LIST(0)

.L0_fastcheck:
	SECTION(.L0_fastcheck1, .L0_fastcheck_end)

	/* XXX - just the framework here */

	cmpwi	r0, -1
  	bne+	.regsyscall	/* do regular syscall */

	cmpwi	r3, SC_GETHRESTIME
	bgt	.fast_unknown
	ba	FASTTRAP_PADDR	/* branch to .Lfasttrap (physical addr) */

.fast_unknown:
/*	li	r0, 0		*/	/* put 0 in r0 (invalid syscall) */
/*  	b	.regsyscall	*/	/* stubbed out in 2.6 - do regular syscall */

.regsyscall:
	SECTION_END(.L0_fastcheck1, .L0_fastcheck_end)

.L0_syscall:
	SECTION(.L0_syscall1, .L0_syscall_end)
	lwz	r4,CPU_MSR_DISABLED(r20)	/* MSR for syscalls */
	lwz	r5,CPU_SYSCALL(r20)		/* sys_call code address */
	lwz	r3,CPU_R3(r20)			/* ssssssssss */
	mtmsr	r4				/* MSR_IR on */
	isync					/* MSR_IR takes effect */
	mtlr	r5
	blrl					/* branch high to sys_call() */
	SECTION_END(.L0_syscall1, .L0_syscall_end)
/*
 * Fast syscalls (like gethrtime) are handled here. r3 contains code for
 * fast syscall. Volatile registers need not be saved. r3, r4 contain
 * longlong value on return from geth*time calls & r3 contains fpuflags on
 * return from getlwpfpu.
 */

.Lfasttrap:
	SECTION_LIST(.Lfasttraps)
	SECTION_LIST(0)

.Lfasttraps:
	SECTION(.Lfasttraps1, .Lfasttraps_end)
	mtsprg	3,r1		/* save sp in sprg3 */
	mfsprg	r1,2		/* get cpu ptr (phys addr) in r1 */
	mfsrr0	r4
	stw	r4,L0_SRR0(r1)	/* save srr0 in cpu struct */
	mfsrr1	r5
	stw	r5,L0_SRR1(r1)	/* save srr1 in cpu struct */

	lis	r1, 0x2000
	ori	r1, r1, 0x1e
	isync
	mtsr	14, r1
	ori	r4, r4, 0x1f		/* set SR14,15 with kernel SR values */
	mtsr	15, r4
	isync
.patch_batu2:
	li	r4, IBATU_LOMEM_MAPPING	/* 603, 604, 620 IBAT 1-1 mapping for 8M */
.patch_batl2:
	li	r5, IBATL_LOMEM_MAPPING
	mtspr	IBAT3U, r4
	mtspr	IBAT3L, r5

	mfmsr	r1
	ori	r1, r1, MSR_DR|MSR_IR
	mtmsr	r1			/* turn on translations */
	isync

	mfsprg	r1, 1			/* get kernel stack from sprg1 */

	stw	THREAD_REG, REGS_R2(r1)	/* save r2 */
	mflr	r4
	mfsprg	THREAD_REG, 0		/* set r2 to curthread */
	stw	r4, REGS_LR(r1)		/* save lr */

	cmpwi	r3, SC_GETLWPFPU
	beq	.Lgetlwpfpu
	cmpwi	r3, SC_GETHRTIME
	beq	.Lgethrtime
	cmpwi	r3, SC_GETHRVTIME
	beq	.Lgethrvtime
	cmpwi	r3, SC_GETHRESTIME
	beq	.Lgethrestime

.Lgetlwpfpu:
	lwz	r3, T_LWP(THREAD_REG)
	lwz	r3, LWP_PCB_FLAGS(r3)
	b	.Lfastexit

.Lgethrtime:
	lis	r4, gethrtime@ha
	ori	r4, r4, gethrtime@l
	b	.Lgethtimecall

.Lgethrvtime:
	lis	r4, gethrvtime@ha
	ori	r4, r4, gethrvtime@l
	b	.Lgethtimecall

.Lgethrestime:
	lis	r4, get_hrestime@ha
	ori	r4, r4, get_hrestime@l

.Lgethtimecall:
	mtlr	r4
	blrl				/* branch high into kernel segment */

.Lfastexit:
	lwz	r5, REGS_LR(r1)
	lwz	THREAD_REG, REGS_R2(r1)	/* restore r2 */

	mfmsr	r8
	rlwinm	r8, r8, 0, 28, 25	/* clear IR and DR */
	mtmsr	r8			/* turn off translations */
	isync


	li	r8, 0
	mtspr	IBAT3U, r8
	mtspr	IBAT3L, r8

.Lretfromfast:
	mtsr	14, r8
	mtsr	15, r8			/* invalidate SR14,15 for user */

	mfsprg	r1, 2
	lwz	r6, L0_SRR0(r1)
	lwz	r7, L0_SRR1(r1)
	mtlr	r5			/* restore lr */
	mtsrr0	r6			/* restore srr0 */
	mtsrr1	r7			/* restore srr1 */
	mfsprg	r1, 3			/* restore sp */
	rfi				/* RET to userland */
					/* r3, r4 contain return vals */
	SECTION_END(.Lfasttraps1, .Lfasttraps_end)


/*
 * Return from trap code that executes in low-memory, since we have to run
 * in physical address mode when restoring srr0, srr1.
 */


.L_trap_rtn:
	SECTION_LIST(.L_trap_rtns)
	SECTION_LIST(0)

.L_trap_rtns:
	SECTION(.L_trap_rtns1, .L_trap_rtns_end)

	lwz	r11, T_CPU(THREAD_REG)	/* get cpu ptr (virt addr) in r11 */

	lwz	r4,REGS_PC(r1)
	lwz	r5,REGS_MSR(r1)

	stw	r4, CPU_SRR0(r11)	/* store srr0, srr1 in cpu struct. */
	stw	r5, CPU_SRR1(r11)

	lwz	r6,REGS_LR(r1)
	lwz	r8,REGS_CTR(r1)
	mtlr	r6			/* restore LR */
	lwz	r9,REGS_XER(r1)
	mtctr	r8			/* restore CTR */
	mtxer	r9			/* restore XER */

	andi.	r5, r5, MSR_PR
	bne+	.user_return
/*
 * 	restore general registers
 */

.kernel_return:
	lwz	r0,REGS_R0(r1)
	lwz	r2,REGS_R2(r1)
	lwz	r3,REGS_R3(r1)
	lwz	r4,REGS_R4(r1)

	stw	r3, CPU_R3(r11)		/* save r3 in cpu struct */
	lwz	r3,REGS_CR(r1)

	lwz	r5,REGS_R5(r1)
	lwz	r6,REGS_R6(r1)
	lwz	r7,REGS_R7(r1)
	lwz	r8,REGS_R8(r1)
	lwz	r9,REGS_R9(r1)
	lwz	r10,REGS_R10(r1)
	lwz	r11,REGS_R11(r1)
	lwz	r12,REGS_R12(r1)
	lwz	r13,REGS_R13(r1)
	lwz	r14,REGS_R14(r1)
	lwz	r15,REGS_R15(r1)
	lwz	r16,REGS_R16(r1)
	lwz	r17,REGS_R17(r1)
	lwz	r18,REGS_R18(r1)
	lwz	r19,REGS_R19(r1)
	lwz	r20,REGS_R20(r1)
	lwz	r21,REGS_R21(r1)
	lwz	r22,REGS_R22(r1)
	lwz	r23,REGS_R23(r1)
	lwz	r24,REGS_R24(r1)
	lwz	r25,REGS_R25(r1)
	lwz	r26,REGS_R26(r1)
	lwz	r27,REGS_R27(r1)
	lwz	r28,REGS_R28(r1)
	lwz	r29,REGS_R29(r1)
	lwz	r30,REGS_R30(r1)
	lwz	r31,REGS_R31(r1)
	lwz	r1,REGS_R1(r1)

	mtsprg	3, r1

	mfmsr	r1
	rlwinm	r1, r1, 0, 28, 25	/* clear IR and DR */
	mtmsr	r1			/* xlations off */
	isync

	li	r1, 0
	mtspr	IBAT3U, r1		/* IBAT3U invalidate */
	mtspr	IBAT3L, r1		/* IBAT3L invalidate */

	mtcrf	0xff, r3		/* restore CR */
	mfsprg	r3, 2			/* get cpu ptr (phys addr) in r3 */

	lwz	r1, L0_SRR0(r3)
	mtsrr0	r1			/* restore srr0 */
	lwz	r1, L0_SRR1(r3)
	mtsrr1	r1			/* restore srr1 */
	lwz	r3, L0_R3(r3)		/* restore r3 */
	mfsprg	r1, 3			/* restore r1 */

	rfi				/* Back, to the Future */

.user_return:
	SECTION_END(.L_trap_rtns1, .L_trap_rtns_end)

	/*
	 * Sub-section to restore user registers.
	 */
.L_trap_rtn_usr:
	SECTION(.L_trap_rtn_usr1, .L_trap_rtn_usr_end)

	lwz	r0,REGS_R0(r1)
	lwz	r2,REGS_R2(r1)
	lwz	r3,REGS_R3(r1)
	lwz	r4,REGS_R4(r1)

	stw	r3, CPU_R3(r11)		/* save r3 in cpu struct */
	lwz	r3,REGS_CR(r1)

	lwz	r5,REGS_R5(r1)
	lwz	r6,REGS_R6(r1)
	lwz	r7,REGS_R7(r1)
	lwz	r8,REGS_R8(r1)
	lwz	r9,REGS_R9(r1)
	lwz	r10,REGS_R10(r1)
	lwz	r11,REGS_R11(r1)
	lwz	r12,REGS_R12(r1)
	lwz	r13,REGS_R13(r1)
	lwz	r14,REGS_R14(r1)
	lwz	r15,REGS_R15(r1)
	lwz	r16,REGS_R16(r1)
	lwz	r17,REGS_R17(r1)
	lwz	r18,REGS_R18(r1)
	lwz	r19,REGS_R19(r1)
	lwz	r20,REGS_R20(r1)
	lwz	r21,REGS_R21(r1)
	lwz	r22,REGS_R22(r1)
	lwz	r23,REGS_R23(r1)
	lwz	r24,REGS_R24(r1)
	lwz	r25,REGS_R25(r1)
	lwz	r26,REGS_R26(r1)
	lwz	r27,REGS_R27(r1)
	lwz	r28,REGS_R28(r1)
	lwz	r29,REGS_R29(r1)
	lwz	r30,REGS_R30(r1)
	lwz	r31,REGS_R31(r1)
	lwz	r1,REGS_R1(r1)

	mtsprg	3, r1

	mfmsr	r1
	rlwinm	r1, r1, 0, 28, 25	/* clear IR and DR */
	mtmsr	r1			/* xlations off */
	isync

	li	r1, 0
	mtspr	IBAT3U, r1		/* IBAT3U invalidate */
	mtspr	IBAT3L, r1		/* IBAT3L invalidate */

	mtsr	14, r1			/* invalidate SR14,15 since */
	mtsr	15, r1			/* returning to user */

	mtcrf	0xff, r3		/* restore CR */
	mfsprg	r3, 2			/* get cpu ptr (phys addr) in r3 */

	lwz	r1, L0_SRR0(r3)
	mtsrr0	r1			/* restore srr0 */
	lwz	r1, L0_SRR1(r3)
/*
 * Paranoia - the next line guarantees that we do not return to user mode
 * from interrupt/trap with interrupt disabled. This seems to have fixed
 * some system hangs(?).
 */
	ori	r1,r1,MSR_EE		/* msr - interrupts enabled */
	mtsrr1	r1			/* restore srr1 */
	lwz	r3, L0_R3(r3)		/* restore r3 */
	mfsprg	r1, 3			/* restore r1 */

	rfi				/* Back, to the Future */

	SECTION_END(.L_trap_rtn_usr1, .L_trap_rtn_usr_end)

/*
 * Return from syscall that executes in low-memory, since we have to run
 * in physical address mode when restoring srr0, srr1.
 */

.L_syscall_rtn:
	SECTION_LIST(.L_syscall_rtns)
	SECTION_LIST(0)

.L_syscall_rtns:	
	SECTION(.L_syscall_rtns1, .L_syscall_rtns_end)

	lwz	r11, T_CPU(THREAD_REG)	/* get cpu ptr (virt addr) in r11 */
					/* XXX - should already be in r20 */

	lwz	r15,REGS_PC(r1)
	lwz	r16,REGS_MSR(r1)

	stw	r15, CPU_SRR0(r11)	/* store srr0, srr1 in cpu struct. */
	stw	r16, CPU_SRR1(r11)

	lwz	r15,REGS_LR(r1)
	mtlr	r15			/* restore LR */
	SECTION_END(.L_syscall_rtns1, .L_syscall_rtns_end)

	/*
	 * Sub-section to restore user registers.
	 *
	 * Since this is a normal return, it should not be necessary to restore
	 * the volatile registers r5-r13, but libc may be relying on some
	 * of them.
	 */
.L_sc_rtn_usr:	
	SECTION(.L_sc_rtn_usr1, .L_sc_rtn_usr_end)

	lwz	r0,REGS_R0(r1)
	lwz	r2,REGS_R2(r1)
					/* r3-r4 already restored (rvals) */
	stw	r3, CPU_R3(r11)		/* save r3 in cpu struct */
	lwz	r3,REGS_CR(r1)		/* restore CR - but clear SO */

	lwz	r5,REGS_R5(r1)		/* r5-r13 shouldn't need restoring */
	lwz	r6,REGS_R6(r1)		/* but are used by libc */
	lwz	r7,REGS_R7(r1)
	lwz	r8,REGS_R8(r1)
	lwz	r9,REGS_R9(r1)
	lwz	r10,REGS_R10(r1)
	lwz	r11,REGS_R11(r1)
	lwz	r12,REGS_R12(r1)
	lwz	r13,REGS_R13(r1)
	lwz	r14,REGS_R14(r1)	/* r14-r16 were used by sys_call */
	lwz	r15,REGS_R15(r1)
	lwz	r16,REGS_R16(r1)
					/* r17-r19 preserved by sys_call and C */
	lwz	r20,REGS_R20(r1)	/* r20-r21 were used by sys_call */
	lwz	r21,REGS_R21(r1)
	lwz	r22,REGS_R22(r1)
					/* r23-r31 preserved by sys_call and C */
	lwz	r1,REGS_R1(r1)

	mtsprg	3, r1

	mfmsr	r1
	rlwinm	r1, r1, 0, 28, 25	/* clear IR and DR */
	mtmsr	r1			/* xlations off */
	isync

	li	r1, 0
	mtspr	IBAT3U, r1		/* IBAT3U invalidate */
	mtspr	IBAT3L, r1		/* IBAT3L invalidate */

	mtsr	14, r1			/* invalidate SR14,15 since */
	mtsr	15, r1			/* returning to user */

	mtctr	r1			/* clear CTR */
	mtxer	r1			/* clear XER */

	lis	r1, CR0_SO >> 16
	andc	r3, r3, r1		/* turn off SO (error) bit */
	mtcrf	0xff, r3		/* indicate success in CR reg */
	mfsprg	r3, 2			/* get cpu ptr (phys addr) in r3 */

	lwz	r1, L0_SRR0(r3)
	mtsrr0	r1			/* restore srr0 */
	lwz	r1, L0_SRR1(r3)
	mtsrr1	r1			/* restore srr1 */
	lwz	r3, L0_R3(r3)		/* restore r3 */
	mfsprg	r1, 3			/* restore r1 */

	rfi				/* Back, to the Future */

	SECTION_END(.L_sc_rtn_usr1, .L_sc_rtn_usr_end)

#endif /* lint */



/* 
 * Level 1 interrupt (and exception) handlers
 *
 * NOTE that this code is mostly a port of the x86 ml code.
 *
 * General register usage in interrupt loops is as follows:
 *
 *	r2  - curthread (always set up by level 1 code)
 *	r14 - interrupt vector
 *	r15 - pointer to autovect structure for handler
 *	r16 - number of handlers in chain
 *	r17 - is DDI_INTR_CLAIMED status of chain
 *	r18 - autovect pointer
 *	r19 - cpu_on_intr
 *	r20 - CPU
 *	r21 - msr (with interrupts disabled)
 *	r22 - msr (with interrupts enabled)
 *	r23 - old ipl
 *	r24 - stack pointer saved before changing to interrupt stack
 */

#if defined(lint) || defined(__lint)

void
_interrupt(void)
{}

void
cmnint(void)
{}

#else   /* lint */

	ENTRY_NP2(cmnint,_interrupt)
	/* XXX removed #ifdef TRAPTRACE code -- but may need */

					/* 03789012345678912345678901lcx */
					/* ............................. */
	stw	r0,REGS_R0(r1)		/* s............................ */
	li	r0, 0
	mtspr	IBAT3U, r0		/* IBAT3U invalidate */
	mtspr	IBAT3L, r0		/* IBAT3L invalidate */
	stw	r3,REGS_R3(r1)		/* .s........................... */
	stw	r7,REGS_R7(r1)		/* ..s.......................... */
	stw	r8,REGS_R8(r1)		/* ...s......................... */
	stw	r9,REGS_R9(r1)		/* ....s........................ */
	stw	r10,REGS_R10(r1)	/* .....s....................... */
	stw	r11,REGS_R11(r1)	/* ......s...................... */
	stw	r12,REGS_R12(r1)	/* .......s..................... */
	stw	r13,REGS_R13(r1)	/* ........s.................... */
	stw	r14,REGS_R14(r1)	/* .........s................... */
	stw	r15,REGS_R15(r1)	/* ..........s.................. */
	mfctr	r9
	stw	r16,REGS_R16(r1)	/* ...........s................. */
	stw	r17,REGS_R17(r1)	/* ............s................ */
	stw	r18,REGS_R18(r1)	/* .............s............... */
	mfxer	r10
	stw	r19,REGS_R19(r1)	/* ..............s.............. */
	stw	r21,REGS_R21(r1)	/* ...............s............. */
	stw	r22,REGS_R22(r1)	/* ................s............ */
	stw	r23,REGS_R23(r1)	/* .................s........... */
	stw	r24,REGS_R24(r1)	/* ..................s.......... */
	stw	r25,REGS_R25(r1)	/* ...................s......... */
	stw	r26,REGS_R26(r1)	/* ....................s........ */
	stw	r27,REGS_R27(r1)	/* .....................s....... */
	stw	r28,REGS_R28(r1)	/* ......................s...... */
	stw	r29,REGS_R29(r1)	/* .......................s..... */
	stw	r30,REGS_R30(r1)	/* ........................s.... */
	stw	r31,REGS_R31(r1)	/* .........................s... */
	mfmsr	r21			/* msr - interrupts disabled */
	stw	r9,REGS_CTR(r1)		/* ...........................s. */
	lwz	r23,CPU_PRI(r20)	/* get current ipl */
	stw	r10,REGS_XER(r1)	/* ............................s */
	ori	r22,r21,MSR_EE		/* msr - interrupts enabled */
/*
 * NOTE: we are free to use registers freely.
 */

	lis	r5,setlvl@ha
	lwz	r5,setlvl@l(r5)
	la	r4,0(r1)		/* &vector (set by setlvl()) */
	mr	r3,r23			/* current spl level */
	mtlr	r5
	blrl				/* call  *setlvl */
	lwz	r14,0(r1)		/* setlvl can modify the vector */

	/* Check for spurious interrupt */
	cmpwi	r3,-1
	beq-	_sys_rtt_spurious

	/*
 	 * At this point we can take one of two paths.  If the new
 	 * priority level is below LOCK LEVEL, then we jump to code that
	 * will run this interrupt as a separate thread.  Otherwise the
	 * interrupt is run on the "interrupt stack" defined in the cpu
	 * structure (similar to the way interrupts always used to work).
	 */

	cmpwi	r3,LOCK_LEVEL		/* compare to highest thread level */
	stw	r3,CPU_PRI(r20)		/* update ipl */
	blt+	intr_thread		/* process as a separate thread */

	/*
	 * Handle high_priority nested interrupt on separate interrupt stack
	 */

	lwz	r19,CPU_ON_INTR(r20)
	slwi	r18,r14,3		/* vector*8 (for index into autovect below) */
	cmpwi	r19,0
	addi	r18,r18,AVH_LINK	/* adjust autovect pointer by link offset */
	li	r24, 0
	bne-	onstack			/* already on interrupt stack */
	li	r4,1
	mr	r24,r1			/* save the thread stack pointer */
	stw	r4,CPU_ON_INTR(r20)	/* mark that we're on the interrupt stack */
	lwz	r1,CPU_INTR_STACK(r20)	/* get on interrupt stack */
onstack:
	stw	r24,0(r1)		/* for use by kadb stack tracing code */
	addis	r18,r18,autovect@ha
	addi	r18,r18,autovect@l	/* r18 is now the address of the list of handlers */
	mtmsr	r22			/* sti */
	/*
	 * Get handler address
	 */
pre_loop1:
	lwz	r15,0(r18)		/* autovect for 1st handler */
	li	r16,0			/* r16 is no. of handlers in chain */
	li	r17,0			/* r17 is DDI_INTR_CLAIMED status of chain */
loop1:
 	cmpwi  	r15,0			/* if pointer is null */
 	beq-   	.intr_ret		/* then skip */
 	lwz	r0,AV_VECTOR(r15)	/* get the interrupt routine */
/*	lwz	r3,AV_INTARG(r15) not in S10 	*/ /* get argument to interrupt routine */
	cmpwi	r0,0			/* if func is null */
	mtlr	r0
	beq-	.intr_ret		/* then skip */
	addi	r16,r16,1
	blrl				/* call interrupt routine with arg */
 	lwz	r15,AV_LINK(r15)	/* get next routine on list */
	or	r17,r17,r3		/* see if anyone claims intpt. */
	b	loop1			/* keep looping until end of list */

.intr_ret:
	cmpwi	r16,1			/* if only 1 intpt in chain, it is OK */
	beq+	.intr_ret1
	cmpwi	r17,0			/* If no one claims intpt, then it is OK */
	beq-	.intr_ret1
	b	pre_loop1		/* else try again. */

.intr_ret1:
	mtmsr	r21			/* cli */
/* 	lwz	r3,CPU_SYSINFO_INTR(r20) not in S10 */
	stw	r23,CPU_PRI(r20)	/* restore old ipl */
	addi	r3,r3,1			/* cpu_sysinfo.intr++ */
/*	stw	r3,CPU_SYSINFO_INTR(r20) not is S10 */
	lis	r5,setlvlx@ha
	mr	r4,r14			/* interrupt vector */
	lwz	r5,setlvlx@l(r5)
	mr	r3,r23			/* old ipl */
	mtlr	r5
	blrl				/* call  *setlvlx */
					/* r3 contains the current ipl */
	cmpwi	r24,0
	beq-	.intr_ret2
	li	r0, 0
	stw	r0,CPU_ON_INTR(r20)
	mr	r1,r24			/* restore the previous stack pointer */
.intr_ret2:
/* XXX PPC - check for need to call clock()!!! or just wait for next tick??? */
 	b	dosoftint		/* check for softints before we return. */
	SET_SIZE(cmnint)

#endif	/* lint */

/* XXX - framework / placeholder only needs work
 * Handle an interrupt in a new thread.
 *	Entry:  traps disabled.
 *		r1	- pointer to regs
 *		r2	- curthread
 *		r3	- interrupt level for this interrupt
 *		r4	- (new) interrupt thread
 *		r14	- vector
 *		r20	- CPU
 *		r21	- msr for interrupts disabled
 *		r22	- msr for interrupts enabled
 *		r23	- old spl
 *		r25	- save for stashing unsafe entry point
 *		r26	- saved interrupt level for this interrupt
 *
 *	General register usage in interrupt loops is as follows:
 *
 *		r2  - curthread (always set up by level 1 code)
 *		r14 - interrupt vector
 *		r15 - pointer to autovect structure for handler
 *		r16 - number of handlers in chain
 *		r17 - is DDI_INTR_CLAIMED status of chain
 *		r18 - autovect pointer
 *		r19 - cpu_on_intr
 *		r20 - CPU
 *		r21 - msr (with interrupts disabled)
 *		r22 - msr (with interrupts enabled)
 *		r23 - old ipl
 *		r24 - stack pointer saved before changing to interrupt stack
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
intr_thread(void)
{}

#else	/* lint */

/*
 * Get set to run interrupt thread.
 * There should always be an interrupt thread since we allocate one
 * for each level on the CPU, and if we release an interrupt, a new
 * thread gets created.
 *
 *	t = CPU->cpu_intr_thread
 *	CPU->cpu_intr_thread = t->t_link
 *	t->t_lwp = curthread->t_lwp
 *	t->t_state = ONPROC_THREAD
 *	t->t_sp = sp
 *	t->t_intr = curthread
 *	curthread = t
 *	sp = t->t_stack
 *	t->t_pri = intr_pri + R3 (== ipl)
 */
	.globl intr_thread
intr_thread:
	mr	r24,r1			/* save the thread stack pointer */
	lwz 	r4,CPU_INTR_THREAD(r20)
	lwz 	r6,T_LWP(THREAD_REG)
	lwz	r5,T_LINK(r4)
	stw 	r6,T_LWP(r4)
	li	r7,ONPROC_THREAD
	stw 	r5,CPU_INTR_THREAD(r20) /* unlink thread from CPU's list */

	/*
	 * Threads on the interrupt thread free list could have state already
	 * set to TS_ONPROC, but it helps in debugging if they're TS_FREE
	 * Could eliminate the next two instructions with a little work.
	 */

	stw	r7,T_STATE(r4)
	/*
	 * chain the interrupted thread onto list from the interrupt thread.
	 * Set the new interrupt thread as the current one.
	 */

	slwi	r18,r14,3		/* vector*8 (for index into autovect below) */
	lis	r5,intr_pri@ha
	stw	r1,T_SP(THREAD_REG)	/* mark stack for resume */
	stw	THREAD_REG,T_INTR(r4)	/* push old thread */
	addi	r18,r18,AVH_LINK	/* adjust autovect pointer by link offset */
	lhz	r6,intr_pri@l(r5)	/* XXX Can cause probs if new class is */
					/* loaded on some other cpu. */
	mtsprg	0,r4
	mr	r26,r3			/* save IPL for possible use by intr_thread_exit */
	stw	r4,CPU_THREAD(r20)	/* set new thread */
	add	r6,r6,r3	 	/* convert level to dispatch priority */
	mr	THREAD_REG,r4
	lwz	r1,T_STACK(r4)		/* interrupt stack pointer */
	stw	r24,0(r1)		/* for use by kadb stack tracing code */
	addis	r18,r18,autovect@ha
	stw	r26,8(r1)		/* save IPL for possible use by intr_passivate */

	/*
	 * Initialize thread priority level from intr_pri
	 */

	sth	r6,T_PRI(r4)
	addi	r18,r18,autovect@l	/* r18 is now the address of the list of handlers */

	mtmsr	r22			/* sti (enable interrupts) */
pre_loop2:
	lwz	r15,0(r18)		/* autovect for 1st handler */
	li	r16,0			/* r16 is no. of handlers in chain */
	li	r17,0			/* r17 is DDI_INTR_CLAIMED status of chain */
loop2:

 	cmpwi  	r15,0			/* if pointer is null */
 	beq-   	loop_done2		/* then skip */

 	lwz	r0,AV_VECTOR(r15)	/* get the interrupt routine */
/*	lwz	r3,AV_INTARG(r15) not in s10 */	/* get argument to interrupt routine */
/* 	lwz	r4,AV_MUTEX(r15) not in S10 */
	cmpwi	r0,0			/* if func is null */
	mtlr	r0
	beq-	loop_done2		/* then skip */
	cmpwi	r4,0
	addi	r16,r16,1
	bne-	.unsafedriver2
	blrl				/* call interrupt routine with arg */
 	lwz	r15,AV_LINK(r15)	/* get next routine on list */
	or	r17,r17,r3		/* see if anyone claims intpt. */
	b	loop2			/* keep looping until end of list */

.unsafedriver2:
loop_done2:	/* XXX ADDED  labels for compilation only need to complete with final code */
dosoftint:
	b	.	/* XXX wrong label location need to complete */

#endif /* lint */

/* XXX - framework / placeholder only needs work
 * Set Cpu's base SPL level, base on which interrupt levels are active
 *	Called at spl7 or above.
 */
#if defined(lint) || defined(__lint)

void
set_base_spl(void)
{}

#else   /* lint */

	ENTRY_NP(set_base_spl)
	lwz	r3,T_CPU(THREAD_REG)
	lwz	r4,CPU_INTR_ACTV(r3)	/* load active interrupts mask */
	ori	r4,r4,1			/* force range of "cntlzw" to exclude 32 */
	/*
	 * If clock is blocked, we want cpu_base_spl to be one
	 */
	andi.	r5,r4,(1<<CLOCK_LEVEL)
	srwi	r6,r5,1			/* mask for clock-level-1 */
	or	r4,r4,r6		/* OR in clock level into clock-level-1 */
	andc	r4,r4,r5		/* clear clock level */

	cntlzw	r5,r4			/* should return 16-31 */
					/* which equate to spl 15-0 */
	subfic	r5,r5,31		/* 31-r5 */
	stw	r5,CPU_BASE_SPL(r3)	/* store base priority */
	blr
	SET_SIZE(set_base_spl)

	.comm clk_intr,4,2

	.data
	.align	2

/*
 * XXX PPC *** HACKED CONSTANT FOR NOW ******
 * We need to architect a correct method to initialize this value.
 */
	.globl	dec_incr_per_tick
dec_incr_per_tick:			/* number to add to decrementer */
	.long	100000			/* for each interrupt (603 value) */

	.globl	hrestime, hres_lock, timedelta
	.globl	hrestime_adj

#endif /* lint */

/*
 * byte offsets from hrestime for other variables, used for faster
 * address calculations.  Assumes LL_LSW/LL_MSW provide offsets
 * of least/most significant word offsets within a longlong.
 */


#if defined(lint) || defined(__lint)

void
init_dec_register(void)
{}

#else   /* lint */


#endif /* lint */

/*
 * This code assumes that the real time clock interrupts 100 times per
 * second.
 *
 * clock() is called in a special thread called the clock_thread.
 * It sort-of looks like and acts like an interrupt thread, but doesn't
 * hold the spl while it's active.  If a clock interrupt occurs and
 * the clock_thread is already active or if we're running at LOCK_LEVEL
 * or higher, the clock_pend counter is incremented, causing clock()
 * to repeat for another tick.
 *
 * Interrupts are disabled upon entry.
 */

/*
 *	General register usage in interrupt loops is as follows:
 *
 *		r2  - curthread (always set up by level 1 code)
 *		r14 - clock_thread
 *		r15 - &clock_pend
 *		r16 - &clock_lock
 *		r17 - old curthread
 *		r18 -
 *		r19 -
 *		r20 - CPU
 *		r21 - msr (with interrupts disabled)
 *		r22 - msr (with interrupts enabled)
 *		r23 - old ipl
 *		r24 - stack pointer saved before changing to interrupt stack
 *		r25 - &hrestime
 *		r26 - &clk_intr
 *		r27 - clk_intr
 *		r28 - dec_incr_per_tick
 */

#if defined(lint) || defined(__lint)

void
clock_intr(void)
{}

#else   /* lint */

	ENTRY_NP(clock_intr)
	nop
	SET_SIZE(clock_intr)


#endif	/* lint */

/*
 *	Level 1 trap code
 */
#if defined(lint)

void
cmntrap(void)
{}

#else   /* lint */

	ENTRY_NP(cmntrap)
	lwz	r5, CPU_DAR(r20)
	lwz	r6, CPU_DSISR(r20)
	lwz	r7, CPU_ID(r20)

/* #ifdef TRAPTRACE and the associated code has been removed for now */
	
	mflr	r4			/*  extract trap type from lr */
	subi	r4, r4, 4		/*  in case blrl was at end of l0 handler */
	andi.	r4, r4, 0xff00
					/*  03789012345678912345678901lcx */
					/*  ............................. */
	stw	r0,REGS_R0(r1)		/*  s............................ */
	li	r0, 0
/* XXX Still running off ODW 1:1 mappings */
#if 0
	mtspr	IBAT3U, r0		/*  IBATU3 invalidate */
	mtspr	IBAT3L, r0		/*  IBATL3 invalidate */
#endif
	mfmsr	r0
	ori	r0, r0, MSR_EE		/*  enable interrupts */
	mtmsr	r0

	stw	r3,REGS_R3(r1)		/*  .s........................... */
	stw	r7,REGS_R7(r1)		/*  ..s.......................... */
	stw	r8,REGS_R8(r1)		/*  ...s......................... */
	stw	r9,REGS_R9(r1)		/*  ....s........................ */
	stw	r10,REGS_R10(r1)	/*  .....s....................... */
	stw	r11,REGS_R11(r1)	/*  ......s...................... */
	stw	r12,REGS_R12(r1)	/*  .......s..................... */
	stw	r13,REGS_R13(r1)	/*  ........s.................... */
	stw	r14,REGS_R14(r1)	/*  .........s................... */
	stw	r15,REGS_R15(r1)	/*  ..........s.................. */
	mfctr	r9
	stw	r16,REGS_R16(r1)	/*  ...........s................. */
	stw	r17,REGS_R17(r1)	/*  ............s................ */
	stw	r18,REGS_R18(r1)	/*  .............s............... */
	mfxer	r10
	stw	r19,REGS_R19(r1)	/*  ..............s.............. */
	stw	r21,REGS_R21(r1)	/*  ...............s............. */
	stw	r22,REGS_R22(r1)	/*  ................s............ */
	stw	r23,REGS_R23(r1)	/*  .................s........... */
	stw	r24,REGS_R24(r1)	/*  ..................s.......... */
	stw	r25,REGS_R25(r1)	/*  ...................s......... */

	stw	r26,REGS_R26(r1)	/*  ....................s........ */
	stw	r27,REGS_R27(r1)	/*  .....................s....... */
	stw	r28,REGS_R28(r1)	/*  ......................s...... */

	stw	r29,REGS_R29(r1)	/*  .......................s..... */
	stw	r30,REGS_R30(r1)	/*  ........................s.... */
	stw	r31,REGS_R31(r1)	/*  .........................s... */

	stw	r9,REGS_CTR(r1)		/*  ...........................s. */
	stw	r10,REGS_XER(r1)	/*  ............................s */

	li	r10,0
	addi	r3,r1,MINFRAME		/*  init arg1, arg2-4 already set up */
	stw	r10,0(r1)
.call_trap:
	bl	trap			/*  call trap(rp, type, dar, dsisr, cpuid) */
	b	_sys_rtt
	SET_SIZE(cmntrap)

#endif	/* lint */



/*	XXX - framework / placeholder only needs work
 *	Level 1 syscall entry
 *
 *	Entered with interrupts disabled.
 *
 *	Non-volatile register usage -
 *
 *		r2  - curthread
 *		r14 - ttolwp(curthread)
 *		r21 - MSR for interrupts disabled
 *		r22 - MSR for interrupts enabled
 */
#if defined(lint) || defined(__lint)

void
sys_call(void)
{}

#else   /* lint */

	ENTRY_NP(sys_call)
	nop
	SET_SIZE(sys_call)

	/*
	 * At this point, the system call args must still be in r3-r10
	 * and the non-volatile user registers are unchanged.  This way,
	 * if no pre-syscall or post-syscall handling is needed, we can
	 * call the handler directly and return without reloading most of the
	 * non-volatile registers.
	 *
	 * The system call number should be still in r0, also.
	 *
	 * We use some non-volatiles as follows:
	 *	r14 = lwp pointer
	 *	r15 = scratch
	 *	r16 = scratch
	 *	r20 = cpu structure
	 *	r21 = disabling msr
	 *	r22 = enabling msr
	 *
	 * Test for pre-system-call handling
	 */



#endif	/* lint */

/*
 *	Return from trap
 */
#if defined(lint) || defined(__lint)

/*
 * This is a common entry point used for:
 *
 *	1. icode() calls here when proc 1 is ready to go to user mode.
 *	2. forklwp() makes this the start of all new user processes.
 *	3. syslwp_create() makes this the start of all new user lwps.
 *
 * It is assumed that this entry point is entered with interrupts enabled
 * and after initializing the stack pointer with curthread->t_stack, the
 * current thread is headed for user mode, so there is no need to check
 * for "return to kernel mode" vs. "return to user mode".
 */

void
lwp_rtt(void)
{}

#else   /* lint */

	ENTRY_NP(lwp_rtt_initial)
	lwz	r1,T_STACK(THREAD_REG)
	lwz	r3,REGS_R3(r1)
	lwz	r4,REGS_R4(r1)
	bl	__dtrace_probe___proc_start
	b	_lwp_rtt

	ENTRY_NP(lwp_rtt)
	lwz	r1,T_STACK(THREAD_REG)
	lwz	r3,REGS_R3(r1)
	lwz	r4,REGS_R4(r1)

_lwp_rtt:
	bl	__dtrace_probe___proc_lwp__start
	nop

/* 	bl	post_syscall	*/	/* post_syscall(rval1, rval2) */

	/*
	 * Return to user.
	 */

	ALTENTRY(_sys_rtt_syscall)
	mfmsr	r5
	rlwinm	r6,r5,0,17,15		/* clear MSR_EE */
	mtmsr	r6			/* disable interrupts */
/* 	lbz	r8,T_ASTFLAG(THREAD_REG) */
	cmplwi	r8,0
	beq+	set_user_regs
	ori	r5,r6,MSR_EE
	mtmsr	r5			/* enable interrupts */
/* 	li	r4,T_AST */
	addi	r3,r1,MINFRAME		/* init arg1 */
/* 	bl	trap		*/	/* AST trap */
	b	_sys_rtt_syscall

	SET_SIZE(lwp_rtt)
	SET_SIZE(lwp_rtt_initial)

#endif	/* lint */

/*
 * This is a common entry point used:
 *
 *	1. for interrupted threads that are being started after their
 *	   corresponding interrupt thread has blocked.  Note that
 *	   the interrupted thread could be either a kernel thread or
 *	   a user thread.  This entry is set up by intr_passivate().
 *	2. after all traps (user or kernel mode) to return from interrupt.
 *	3. after check for soft interrupts, when there are none to service.
 *	4. after spurious interrupts.
 */

#if defined(lint) || defined(__lint)

void
_sys_rtt(void)
{}

#else   /* lint */

	.data
	.globl	ppc_spurious
ppc_spurious:
	.long	0

	.text
	ALTENTRY(_sys_rtt_spurious)
	lis	r3,ppc_spurious@ha	/* for the heck of it, keep a */
	lwzu	r5,ppc_spurious@l(r3)	/* running count of spurious */
	addi	r5,r5,1			/* interrupts */
	stw	r5,0(r3)
	ALTENTRY(_sys_rtt)
	lwz	r5,REGS_MSR(r1)
	andi.	r5,r5,MSR_PR		/* going back to kernel or user mode? */
	bne+	_sys_rtt_syscall

	/* return to supervisor mode */
sr_sup:
	/* Check for kernel preemption  */
	lwz	r20,T_CPU(THREAD_REG)	/* CPU */
/* 	lbz	r0,CPU_KPRUNRUN(r20) */
	cmplwi	r0,0
	beq-	set_sup_regs
	li	r3,1
	bl	kpreempt		/* kpreempt(1) */
set_sup_regs:
	mfmsr	r5
	rlwinm	r6,r5,0,17,15		/* clear MSR_EE */
	mtmsr	r6			/* disable interrupts */
/*
 *	restore special purpose registers - srr0, srr1, lr, cr, ctr, xer
 *	    (stagger loads and stores for "hopefully" better performance)
 *
 *	assumes interrupts are disabled
 */

set_user_regs:
	stwcx.	THREAD_REG,0,r1		/* guarantee no "dangling reservation" */

/*
 * Actual return code is at TRAP_RTN_PADDR phys addr in low memory.
 * Therefore we need temp BAT mapping for low memory.
 */
.patch_batu4:
	li	r4, IBATU_LOMEM_MAPPING	/* 603, 604, 620 IBAT 1-1 mapping for 8M */
.patch_batl4:
	li	r5, IBATL_LOMEM_MAPPING
	mtspr	IBAT3U, r4		/* IBAT3U */
	mtspr	IBAT3L, r5		/* IBAT3L */
	isync

	ba	TRAP_RTN_PADDR		/* branch absolute to low mem */

	SET_SIZE(lwp_rtt)

#endif	/* lint */


/*
 * int
 * intr_passivate(from, to)
 *	kthread_id_t	from;		interrupt thread
 *	kthread_id_t	to;		interrupted thread
 *
 *	intr_passivate(t, itp) makes the interrupted thread "t" runnable.
 *
 *	Since t->t_sp has already been saved, t->t_pc is all that needs
 *	set in this function.  The SPARC register window architecture
 *	greatly complicates this.  We have stashed the IPL return value
 *	away in the register save area of the interrupted thread.
 *
 *	Returns interrupt level of the thread.
 */

#if defined(lint) || defined(__lint)

/*ARGSUSED*/
int
intr_passivate(kthread_id_t from, kthread_id_t to)
{ return (0); }

#else   /* lint */

	ENTRY(intr_passivate)
	lis	r6,_sys_rtt@ha
	lwz	r5,T_STACK(r3)		/* where the stack began */
	la	r6,_sys_rtt@l(r6)
	lwz	r3,8(r5)		/* where the IPL was saved */
/* 	stw	r6,T_PC(r4)	*/	/* set interrupted thread's resume pc */
	blr
	SET_SIZE(intr_passivate)


#endif	/* lint */

/*
 * Return a thread's interrupt level.
 * This isn't saved anywhere but on the interrupt stack at interrupt
 * entry at the bottom of interrupt stack.
 *
 * Caller 'swears' that this really is an interrupt thread.
 *
 * int
 * intr_level(t)
 *	kthread_id_t	t;
 */

#if defined(lint) || defined(__lint)

/*ARGSUSED*/
int
intr_level(kthread_id_t t)
{}

#else   /* lint */


/*
 * dosoftint(old_pil in eax)
 * Process software interrupts
 * Interrupts are disabled here.
 */

/*
 * Handle an interrupt in a new thread.
 *	Entry:  traps disabled.
 *		r1	- pointer to regs
 *		r2	- curthread
 *		r20	- CPU
 *		r21	- msr for interrupts disabled
 *		r22	- msr for interrupts enabled
 *		r23	- old spl
 *		r25	- save for stashing unsafe entry point
 *		r26	- saved interrupt level for this interrupt
 *
 *	General register usage in interrupt loops is as follows:
 *
 *		r2  - curthread (always set up by level 1 code)
 *		r14 - interrupt vector
 *		r15 - pointer to softvect structure for handler
 *		r16 - number of handlers in chain
 *		r17 - is DDI_INTR_CLAIMED status of chain
 *		r18 - softvect pointer
 *		r19 - cpu_on_intr
 *		r20 - CPU
 *		r21 - msr (with interrupts disabled)
 *		r22 - msr (with interrupts enabled)
 *		r23 - old ipl
 *		r24 - stack pointer saved before changing to interrupt stack
 */



#endif	/* lint */

/*
 * The following code is used to generate a 10 microsecond delay
 * routine.  It is initialized in pit.c.
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
tenmicrosec(void)
{}

#else	/* lint */
	.globl	microdata
	ENTRY(tenmicrosec)

	blr

	SET_SIZE(tenmicrosec)

	.text

	ENTRY(usec_delay)
	b	drv_usecwait		/* use the ddi routine */

/*
 *	timebase_period = (nanosecs << NSEC_SHIFT) between each increment to
 *		time base lower.
 *	tbticks_per_10usec = nanosecs between each increment to time base lower.
 *			  e.g., 100 for first 603, and 150 for first 604
 *	These are set based on OpenFirmware properties in mlsetup().
 */
	.data
	.align	2
	.globl	timebase_period
timebase_period:
	.long	100 << NSEC_SHIFT	/* set in fiximp_obp(). */
	.size	timebase_period, 4

	.globl	tbticks_per_10usec
tbticks_per_10usec:
	.long	100			/* set in fiximp_obp(). */
	.size	tbticks_per_10usec, 4

#endif /* lint */

/*
 * gethrtime() returns returns high resolution timer value
 */
#if defined(lint) || defined(__lint)
hrtime_t
gethrtime(void)
{
	return ((hrtime_t)0);
}

#else	/* lint */

	ENTRY_NP(gethrtime)
	lis	r6, timebase_period@ha
	lwz	r6, timebase_period@l(r6)
1:
	/* read the current timebase value. */
	mftbu	r8
	mftb	r7
	mftbu	r5
	cmplw	r8, r5
	bne-	1b
	/*
	 * The time in nanoseconds is (tb >> NSEC_SHIFT) * timebase_period.
	 * The shift is done partly before and partly after the multiply
	 * to avoid overflowing 64 bits on the multiply, and to avoid throwing
	 * away too much precision.
	 *	r8 = tbu
	 *	r7 = tbl
	 */
	slwi	r9, r8, 32-NSEC_SHIFT1	/* r9 = bits shifted from high  */
	srwi	r8, r8, NSEC_SHIFT1
	srwi	r7, r7, NSEC_SHIFT1
	or	r7, r7, r9		/* r8:r7 = (tb >> NSEC_SHIFT1) */

	/* 64-bit multiply by the timebase_period */

	mullw	r3, r7, r6
	mulhwu	r4, r7, r6
	mullw	r5, r8, r6
	add	r4, r4, r5		/* r3,r4 = nsecs for the timebase value */

	slwi	r9, r4, 32-NSEC_SHIFT2	/* r9 = bits shifted from high  */
	srwi	r5, r3, NSEC_SHIFT2	/* low order word */
	srwi	r3, r4, NSEC_SHIFT2	/* high order word */
	or	r4, r5, r9		/* OR bits from high-order word */
	blr				/* return 64bit nsec count in r3,r4 */
	SET_SIZE(gethrtime)
#endif /* lint */


/*
 * Read time base.
 *
 * See MPCFPE32B, Rev. 3, 2005/09, page 2-14
 * section 2.2.1, Reading the Time Base.
 */
#if defined(lint) || defined(__lint)

hrtime_t
get_time_base(void)
{}

#else /* lint */

#if defined(_BIG_ENDIAN)
#define	TBU_REG	r3
#define	TBL_REG	r4
#else
#define	TBU_REG	r4
#define	TBL_REG	r3
#endif
	ENTRY(get_time_base)
1:
	mftbu	TBU_REG
	mftb	TBL_REG
	mftbu	r5		/* read TBU again */
	cmpw	r5, TBU_REG	/* Has TBU changed due to carry? */
	bne	1b		/* Try again, if carry occurred */
	blr			/* No change; we are done. */
	SET_SIZE(get_time_base)

#endif /* lint */

/*
 * gethrvtime() returns high resolution thread virtual time
 */

#if defined(lint) || defined(__lint)
/*ARGSUSED*/
void
gethrvtime(timespec_t *tp)
{}
#else /* lint */

/* XXX In Solaris 10, micro-state accounting is always on */

	ENTRY_NP(gethrvtime)
	mflr	r0
	stwu	r1, -16(r1)
	stw	r0, 20(r1)

	bl	gethrtime

	lhz	r5, T_PROC_FLAG(THREAD_REG)
	andi.	r5, r5, TP_MSACCT
	beq+	.gethrvtime1
	lwz	r5, T_LWP(THREAD_REG)		/* micro-state acct ON */
	lwz	r6, LWP_STATE_START(r5)
	lwz	r7, LWP_STATE_START+4(r5)
	lwz	r8, LWP_ACCT_USER(r5)
	lwz	r9, LWP_ACCT_USER+4(r5)
	subfc	r3, r6, r3
	subfe	r4, r7, r4
	add	r3, r3, r8
	addc	r4, r4, r9
	b	.gethrvret
.gethrvtime1:					/* micro-state acct OFF */
	lwz	r5, T_LWP(THREAD_REG)
	lwz	r6, LWP_MS_START(r5)
	lwz	r7, LWP_MS_START+4(r5)
	subfc	r3, r6, r3
	subfe	r4, r7, r4			/* subtract process start time */
	lis	r9, 0x98
	ori	r9, r9, 0x9680			/* r9 = 0x989680 = 10,000,000 */
	mulhwu	r10, r8, r9
	mullw	r9, r8, r9
	cmplw	r4, r10
	bne	.gethrvtime2
	cmplw	r9, r3
.gethrvtime2:
	bge	.gethrvret			/* elapsed time is greater */
	mr	r3, r9
	mr	r4, r10

.gethrvret:
	lwz	r0, 20(r1)
	addi	r1, r1, 16
	mtlr	r0
	blr
	SET_SIZE(gethrvtime)


#endif /* lint */

/*
 * Fast system call wrapper to return hi-res time of day.
 * Exit:
 *	r3 = seconds.
 *	r4 = nanoseconds.
 */

#if defined(lint) || defined(__lint)
void
get_hrestime(void)
{}
#else	/* lint */

	ENTRY_NP(get_hrestime)
	mflr	r0
	stwu	r1, -16(r1)
	stw	r0, 20(r1)
	la	r3, 8(r1)		/* r3 = address for timespec result */
	bl	gethrestime
	lwz	r4, 8+4(r1)		/* nanoseconds */
	lwz	r3, 8(r1)		/* seconds */

	lwz	r0, 20(r1)
	addi	r1, r1, 16
	mtlr	r0
	blr
	SET_SIZE(get_hrestime)

#endif	/* lint */

#if defined(lint) || defined(__lint)

void
pc_reset(void)
{}

#else	/* lint */

	ENTRY(pc_reset)
.patch_batu5:
	li	r4, IBATU_LOMEM_MAPPING
.patch_batl5:
	li	r5, IBATL_LOMEM_MAPPING
	mtspr	IBAT3U, r4		/* IBAT3U mapping for low mem */
	mtspr	IBAT3L, r5		/* IBAT3L mapping for low mem */

	mfmsr	r3
	rlwinm	r3,r3,0,17,15		/* MSR_EE off - no interrupts */
	mtmsr	r3
	isync
	ba	RESET_PADDR		/* jump to low memory handler */
	SET_SIZE(pc_reset)

#endif	/* lint */

#if defined(lint) || defined(__lint)

void
return_instr(void)
{}

#else	/* lint */



#endif /* lint */
