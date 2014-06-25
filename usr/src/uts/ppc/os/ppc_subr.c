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

#pragma ident	"%Z%%M%	%I%	%E% SMI"

/*
 * General assembly language routines.
 * It is the intent of this file to contain routines that are
 * independent of the specific kernel architecture, and those that are
 * common across kernel architectures.
 * As architectures diverge, and implementations of specific
 * architecture-dependent routines change, the routines should be moved
 * from this file into the respective ../`arch -k`/xxx_subr.c file.
 */

#include <sys/psw.h>
#include <sys/types.h>
#include <sys/machthread.h>
#include <sys/thread.h>
#if (defined(_KERNEL) || defined(_KMEMUSER)) && defined(_MACHDEP)
#include <sys/machcpuvar.h>
#endif
#include <sys/cpuvar.h>
#include <sys/reg.h>
#include <sys/ppc_instr.h>
#include <sys/msr.h>
#include <sys/spr.h>
#include <sys/cmn_err.h>

int catch_fault(void);
int on_fault(label_t *);
int setjmp(label_t *);
void longjmp(label_t *);

volatile int hres_lock;

int
catch_fault(void)
{
	label_t *ljb;
	kthread_t *cur_thread = curthread;

	ljb = cur_thread->t_onfault;
	cur_thread->t_onfault = NULL;
	cur_thread->t_lofault = NULL;
	longjmp(ljb);	// Let longjmp() do the rest
	return (0);
}

/*
 * on_fault()
 * Catch lofault faults.
 * Like setjmp, except it returns one
 * if code following causes uncorrectable fault.
 * Turned off by calling no_fault().
 */

int
on_fault(label_t *ljb)
{
	kthread_t *cur_thread = curthread;

	cur_thread->t_onfault = ljb;
	cur_thread->t_lofault = (uintptr_t)&catch_fault;
	setjmp(ljb);	// Let setjmp() do the rest
	return (0);
}

/*
 * Setjmp and longjmp implement non-local gotos
 * using state vectors of type label_t.
 *
 * setjmp(lp)
 * label_t *lp;
 *
 * The saved state consists of:
 *
 *		+----------------+  0
 *		|      r1 (sp)   |
 *		+----------------+  4
 *		| pc (ret addr)  |
 *		+----------------+  8
 *		|      cr        |
 *		+----------------+  12
 *		|      r13       |
 *		+----------------+  16
 *		|      ...       |
 *		+----------------+  84
 *		|      r31       |
 *		+----------------+  <------  88
 *
 * sizeof (label_t) == 22 * sizeof (uint_t) == 88 bytes.
 */

int
setjmp(label_t *lp)
{
	__asm__(
	"	stw	1, 0(%[lp])	\n"
	"	mflr	5		\n"
	"	stw	5, 4(%[lp])	\n"
	"	mfcr	5		\n"
	"	stw	5, 8(%[lp])	\n"
	"	stw	13, 12(%[lp])	\n"
	"	stw	14, 16(%[lp])	\n"
	"	stw	15, 20(%[lp])	\n"
	"	stw	16, 24(%[lp])	\n"
	"	stw	17, 28(%[lp])	\n"
	"	stw	18, 32(%[lp])	\n"
	"	stw	19, 36(%[lp])	\n"
	"	stw	20, 40(%[lp])	\n"
	"	stw	21, 44(%[lp])	\n"
	"	stw	22, 48(%[lp])	\n"
	"	stw	23, 52(%[lp])	\n"
	"	stw	24, 56(%[lp])	\n"
	"	stw	25, 60(%[lp])	\n"
	"	stw	26, 64(%[lp])	\n"
	"	stw	27, 68(%[lp])	\n"
	"	stw	28, 72(%[lp])	\n"
	"	stw	29, 76(%[lp])	\n"
	"	stw	30, 80(%[lp])	\n"
	"	stw	31, 84(%[lp])	\n"
	: /* No outputs */
	: [lp] "r" (lp)
	/* */);
	return (0);
}

/*
 * longjmp(lp)
 * label_t *lp;
 */

void
longjmp(label_t *lp)
{
	__asm__(
	"	lwz	1, 0(%[lp])	\n"
	"	lwz	5, 4(%[lp])	\n"
	"	mtlr	5		\n"
	"	lwz	5, 8(%[lp])	\n"
	"	mtcrf	0xff, 5		\n"
	"	lwz	13, 12(%[lp])	\n"
	"	lwz	14, 16(%[lp])	\n"
	"	lwz	15, 20(%[lp])	\n"
	"	lwz	16, 24(%[lp])	\n"
	"	lwz	17, 28(%[lp])	\n"
	"	lwz	18, 32(%[lp])	\n"
	"	lwz	19, 36(%[lp])	\n"
	"	lwz	20, 40(%[lp])	\n"
	"	lwz	21, 44(%[lp])	\n"
	"	lwz	22, 48(%[lp])	\n"
	"	lwz	23, 52(%[lp])	\n"
	"	lwz	24, 56(%[lp])	\n"
	"	lwz	25, 60(%[lp])	\n"
	"	lwz	26, 64(%[lp])	\n"
	"	lwz	27, 68(%[lp])	\n"
	"	lwz	28, 72(%[lp])	\n"
	"	lwz	29, 76(%[lp])	\n"
	"	lwz	30, 80(%[lp])	\n"
	"	lwz	31, 84(%[lp])	\n"
	"	li	3, 1		\n"
	: /* No outputs */
	: [lp] "r" (lp)
	/* */);
}

struct qelem {
	struct qelem *q_forw;
	struct qelem *q_back;
};

/*
 * _insque(entryp, predp)
 *
 * Insert entryp after predp in a doubly linked list.
 *
 * See usr/src/libc/port/gen/insque.c.
 * Note that the libc/port/version handles two special cases:
 *   1. insertion of the first element (pred == NULL), and
 *   2. insertion at the end of the queue.
 * No kernel implementations do this.
 */

void
_insque(caddr_t entryp, caddr_t predp)
{
	struct qelem *elem = (struct qelem *)entryp;
	struct qelem *pred = (struct qelem *)predp;

	ASSERT(elem != NULL);
	ASSERT(pred != NULL);
	ASSERT(pred->q_forw != NULL);
	elem->q_forw = pred->q_forw;
	elem->q_back = pred;
	pred->q_forw->q_back = elem;
	pred->q_forw = elem;
}

/*
 * _remque(entryp)
 *
 * Remove entryp from a doubly linked list
 */

void
_remque(caddr_t entryp)
{
	struct qelem *elem = (struct qelem *)entryp;

	ASSERT(elem != NULL);
	ASSERT(elem->q_forw != NULL);
	ASSERT(elem->q_back != NULL);
	elem->q_back->q_forw = elem->q_forw;
	elem->q_forw->q_back = elem->q_back;
}

/*
 * Get current processor interrupt level
 * ppc_subr.h inlines this for kernel machdep users,
 * but genunix and userland needs an external function to call,
 * becasue we do not want to expose machine-specific details
 * such as cpu_m.
 *
 * See usr/src/uts/common/sys/cpuvar.h.
 * At the end of the definition of struct cpu, the inclusion
 * of cpu_m is conditional.
 */

greg_t
getpil(void)
{
	kthread_t *cur_thread = curthread;

	return (cur_thread->t_cpu->cpu_m.mcpu_pri);
}

/*
 * Algorithm for spl [Solaris/PPC 2.6]:
 *
 *      Turn off interrupts
 *
 *	if (CPU->cpu_base_spl > newipl)
 *		newipl = CPU->cpu_base_spl;
 *      oldipl = CPU->cpu_pridata->c_ipl;
 *      CPU->cpu_pridata->c_ipl = newipl;
 *
 *      setspl();  // load new masks into pics
 *
 * Be careful not to set priority lower than CPU->cpu_base_pri,
 * even though it seems we're raising the priority, it could be set
 * higher at any time by an interrupt routine, so we must block interrupts
 * and look at CPU->cpu_base_pri
 */

int
spl_impl(int new_ipl)
{
	extern int (*setspl)(int);
	kthread_t *cur_thread;
	uint_t msr_save, msr_dis;
	int base_spl;
	int old_pri;

	/*
	 * Disable interrupts.
	 */
	msr_save = mfmsr();
	msr_dis = MSR_SET_EE(msr_save, 0);
	mtmsr(msr_dis);
	cur_thread = curthread;
	base_spl = cur_thread->t_cpu->cpu_base_spl;
	if (base_spl > new_ipl)
		new_ipl = base_spl;
	old_pri = cur_thread->t_cpu->cpu_m.mcpu_pri;
	cur_thread->t_cpu->cpu_m.mcpu_pri = new_ipl;
	(*setspl)(new_ipl);
	/* Restore old MSR interrupt status */
	mtmsr(msr_save);
	return (old_pri);
}

int
splr(int new_level)
{
	kthread_t *cur_thread;
	int cur_level;

	cur_thread = curthread;
	cur_level = getpil();
	if (cur_level >= new_level)
		return (cur_level);
	return (spl_impl(new_level));
}

// ------------------------------------------------------------------------
//			C callable in and out routines
// ------------------------------------------------------------------------

/*
 * Boot uses physical addresses to do ins and outs but in the kernel we
 * would like to use mapped addresses.  The following in/out routines
 * assumes PCIISA_VBASE as the virtual base (64k aligned) which is already
 * mapped in the kernel startup.
 */

#define	port_to_ioaddr(port) \
	((uintptr_t)PCIISA_VBASE | ((uintptr_t)port & 0xffff))

#define	IS_ALIGNED(addr) \
	(((uintptr_t)(addr) & (sizeof (*addr) - 1)) == 0)

#define	IS_ALIGNED_TYPE(addr, type) \
	(((uintptr_t)(addr) & (sizeof (type) - 1)) == 0)

/*
 * outl(port address, val)
 *   write val to port
 */
void
outl(int port, unsigned long val)
{
	volatile unsigned long *ioaddr;

	ioaddr = (unsigned long *)port_to_ioaddr(port);
	eieio();
	*ioaddr = val;
}

void
outw(int port, unsigned short val)
{
	volatile unsigned short *ioaddr;

	ioaddr = (unsigned short *)port_to_ioaddr(port);
	eieio();
	*ioaddr = val;
}

void
outb(int port, unsigned char val)
{
	volatile unsigned char *ioaddr;

	ioaddr = (unsigned char *)port_to_ioaddr(port);
	eieio();
	*ioaddr = val;
}

unsigned long
inl(int port)
{
	volatile unsigned long *ioaddr;

	ioaddr = (unsigned long *)port_to_ioaddr(port);
	eieio();
	return (*ioaddr);
}

unsigned short
inw(int port)
{
	volatile unsigned short *ioaddr;

	ioaddr = (unsigned short *)port_to_ioaddr(port);
	eieio();
	return (*ioaddr);
}

unsigned char
inb(int port)
{
	volatile unsigned char *ioaddr;

	ioaddr = (unsigned char *)port_to_ioaddr(port);
	eieio();
	return (*ioaddr);
}

/*
 * The following routines move strings to and from an I/O port.
 * loutw(port, addr, count);
 * linw(port, addr, count);
 * repinsw(port, addr, cnt) - input a stream of 16-bit words
 * repoutsw(port, addr, cnt) - output a stream of 16-bit words
 *	Note: addr is assumed to be half word aligned.
 */

void
loutw(int port, unsigned short *addr, uint_t count)
{
	volatile unsigned short *ioaddr;

	ASSERT(IS_ALIGNED(addr));
	ioaddr = (unsigned short *)port_to_ioaddr(port);
	while (count != 0) {
		eieio();
		*ioaddr = *addr;
		++addr;
		--count;
	}
}

/*
 * XXX There should be a portable C way to
 * XXX declare than fn1 is a synonym for fn2.
 */

void
repoutsw(int port, unsigned short *addr, int cnt)
{
	loutw(port, addr, cnt);
}

void
linw(int port, unsigned short *addr, int count)
{
	volatile unsigned short *ioaddr;

	ASSERT(IS_ALIGNED(addr));
	ioaddr = (unsigned short *)port_to_ioaddr(port);
	while (count != 0) {
		eieio();
		*addr = *ioaddr;
		++addr;
		--count;
	}
}

void
repinsw(int port, unsigned short *addr, int count)
{
	linw(port, addr, count);
}

/*
 * repinsb(port, addr, cnt) - input a stream of bytes
 */

void
repinsb(int port, unsigned char *addr, int count)
{
	volatile unsigned char *ioaddr;

	ioaddr = (unsigned char *)port_to_ioaddr(port);
	while (count != 0) {
		eieio();
		*addr = *ioaddr;
		++addr;
		--count;
	}
}

/*
 * repinsd(port, addr, cnt) - intput a stream of 32-bit words
 *	Note: addr is assumed to be word aligned.
 */

void
repinsd(int port, unsigned long *addr, int count)
{
	volatile unsigned char *ioaddr;

	ASSERT(IS_ALIGNED(addr));
	ioaddr = (unsigned char *)port_to_ioaddr(port);
	while (count != 0) {
		eieio();
		*addr = *ioaddr;
		++addr;
		--count;
	}
}

/*
 * repoutsb(port, addr, cnt) - output a stream of bytes
 *    NOTE: count is a byte count
 */
void
repoutsb(int port, unsigned char *addr, uint_t count)
{
	volatile unsigned char *ioaddr;

	ioaddr = (unsigned char *)port_to_ioaddr(port);
	while (count != 0) {
		eieio();
		*ioaddr = *addr;
		++addr;
		--count;
	}
}

/*
 * repoutsd(port, addr, cnt) - output a stream of 32-bit words
 * NOTE: count is a DWORD count. And addr is word aligned.
 */
void
repoutsd(int port, unsigned long *addr, int count)
{
	volatile unsigned long *ioaddr;

	ASSERT(IS_ALIGNED(addr));
	ioaddr = (unsigned long *)port_to_ioaddr(port);
	while (count != 0) {
		eieio();
		*ioaddr = *addr;
		++addr;
		--count;
	}
}

/*
 * DEC VAX scanc instruction
 *
 * Description:
 *   Visit successive characters of cp[].
 *   Index into table[*cp].
 *   Stop when table[*cp] & mask != 0,
 *   or we have visited 'size' characters,
 *   whichever comes first.
 *   Return relative position index where the scan stopped.
 *
 */
int
scanc(uint_t size, uchar_t *cp, uchar_t *table, uchar_t mask)
{
	uint_t pos = 0;

	while (pos < size && ((table[cp[pos]] & mask) != 0))
		++pos;
	return (pos);
}

/*
 * PowerPC can run in either big-endian or little-endian mode.
 * The hton*() and ntoh*() functions are not needed in big-endian mode,
 * because in that case, they get defined as pass-thruough macros.
 * See usr/src/uts/common/sys/byteorder.h.
 */

#if defined(_LITTLE_ENDIAN)

ulong_t
htonl(ulong_t i)
{
	return ((i << 24) | ((i << 8) & 0xff0000) |
		((i >> 8) & 0xff00) | ((i >> 16) & 0xff));
}

ushort_t
htons(ushort_t i)
{
	return (((i & 0xff) << 8) | ((i >> 8) & 0xff));
}

ulong_t
ntohl(ulong_t i)
{
	return (htonl(i));
}

ushort_t
ntohs(ushort_t i)
{
	return (htons(i));
}

#endif /* defined(_LITTLE_ENDIAN) */

/*
 * Make the memory at {addr, addr+size} valid for instruction execution.
 *
 * NOTE: It is assumed that cache blocks are no smaller than 32 bytes.
 *
 * Algorithm:
 *	Adjust the given address and length so that we sync
 *	starting on a cache block boundary and so that the adjusted
 *	length will still completely contain [addr .. addr+len].
 *
 *	Sweep through all cache blocks and do dcbst() on each block.
 *	ppc_sync().
 *	Sweep through all cache blocks and do icbi() on each block.
 *	ppc_sync().
 *	isync().
 */

#define	MIN_CACHE_BLOCKSIZE_SHIFT	5
#define	MIN_CACHE_BLOCKSIZE		(1 << MIN_CACHE_BLOCKSIZE_SHIFT)
#define	MIN_CACHE_BLOCKSIZE_MASK	(MIN_CACHE_BLOCKSIZE - 1)

void
sync_instruction_memory(caddr_t addr, size_t len)
{
	uintptr_t sva;
	uintptr_t eva;
	caddr_t a;
	size_t cnt, i;

	/* Align start va to cache block boundary */
	sva = (uintptr_t)addr & MIN_CACHE_BLOCKSIZE_MASK;
	eva = (((uintptr_t)addr + len + MIN_CACHE_BLOCKSIZE - 1) &
		MIN_CACHE_BLOCKSIZE_MASK);
	cnt = (eva - sva) >> MIN_CACHE_BLOCKSIZE_SHIFT;
	for (a = (caddr_t)sva, i = cnt; i != 0; a += MIN_CACHE_BLOCKSIZE, --i)
		dcbst(a);
	/*
	 * Guarantee dcbst() are all done before any icbi().
	 * one sync() for all the dcbst instructions.
	 */
	ppc_sync();	/* guarantee dcbst is done before icbi */
	for (a = (caddr_t)sva, i = cnt; i != 0; a += MIN_CACHE_BLOCKSIZE, --i)
		icbi(a);	/* force out of instruction cache */
	/* One sync() for all icbi() operations */
	ppc_sync();
	isync();
}

/*ARGSUSED*/
void
vpanic(const char *format, __va_list alist)
{
	prom_vprintf(format, alist);
	prom_printf("\nIn vpanic()... looping forever.\n");
	/*
	 * For now, just loop forever and let someone do what they
	 * need to do with the hardware debugger, without doing
	 * any more mischief to upset machine state, stack traces, etc.
	 * Later, we must implement the saving of all registers,
	 * panic_trigger logic, and call panic_sys().
	 *
	 * At first, it seemed like it might be a nice idea
	 * to go into sleep mode (at least on the MCP7450).
	 * But that takes a lot of work, and among other things,
	 * all TLB entries must be invalidated.  Depending on the
	 * target hardware and on the debugger, that information
	 * might be useful.  So, never mind.
	 */
	while (1) { /* Do nothing. */ }
}

/*
 * The more general Programming Environments Manual says that
 * System Call (sc), Return From Interrupt (rfi) and Instruction
 * Synchronize (isync) instructions all perform context synchronization.
 * See MPCFPE32B, Rev. 3, 2005/09, page 4-7, section 4.1.4.1,
 * Context Synchronizing Instructions.
 *
 * The MPC7450-specific manual says only that sc and rfi
 * perform context synchronization.  See MPC7450UM,
 * Rev. 5, 2005/01, page 2-73, section 2.4.2.4.1, Context Synchronization.
 * This is a bit surprising and annoying, because: 1) it seems overly
 * complex to do an rfi just to do something like set a value in HID0;
 * and 2) it is an exception to the rule that the MPC7450 is a
 * specialization of the PowerPC as described in the more general manual.
 *
 * Snippets of code provided by Genesi use rfi to perform context
 * synchronization.
 *
 * Because of the differences in the manuals, and because of the
 * recipes given by a board vendor, we have chosen rfi to implement
 * context synchronization.  But, due to a lack of confidence that
 * this is the one true implementation, we make context_sync() a
 * function call.  Come to think of it, context_sync() ought to be
 * a function anyway, just on general principle.  It may be inline
 * or not, but it should have the semantics of a function call.
 *
 * This implementation is definitely not inline, because it stores
 * the link register so that the rfi will transfer control back to
 * the caller.
 */

void
context_sync(void)
{
	ppc_sync();
	mtspr(SPR_SRR1, mfmsr());
	mtspr(SPR_SRR0, mflr());
	rfi();
}

/*
 * Find highest one bit set.
 * Returns bit number + 1 of highest bit that is set, otherwise returns 0.
 * High order bit is 31.
 */

int
highbit(ulong_t i)
{
	return (32 - cntlzw(i));
}

/*
 * Isolate the least-significant 1 bit.
 */
#define	LS1BIT(x) ((-(x)) & (x))

/*
 * Find lowest one bit set.
 * Returns bit number + 1 of lowest bit that is set, otherwise returns 0.
 * Low order bit is 0.
 */

int
lowbit(ulong_t i)
{
	ulong_t ls1 = LS1BIT(i);
	return (32 - cntlzw(ls1));
}

#if defined(XXX_OBSOLETE_SOLARIS)

/*
 * Kernel probes assembly routines
 */
int
tnfw_b_get_lock(uchar_t *lp)
{
	return (0);
}

void
tnfw_b_clear_lock(uchar_t *lp)
{
}

ulong_t
tnfw_b_atomic_swap(ulong_t *wp, ulong_t x)
{
	return (0);
}

#endif /* defined(XXX_OBSOLETE_SOLARIS) */
