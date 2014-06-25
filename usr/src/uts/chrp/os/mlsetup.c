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
 */

/*
 * Copyright 2006 Sun Microsystems Laboratories.  All rights reserved.
 * Use is subject to license terms.
 */


#pragma ident   "%Z%%M% %I%	%E% CP"

#include <sys/types.h>
#include <sys/bootconf.h>
#include <sys/disp.h>
#include <sys/promif.h>
#include <sys/clock.h>
#include <sys/pte.h>
#include <sys/mmu.h>
#include <sys/cpu.h>
#include <sys/cpuvar.h>
#include <sys/stack.h>
#include <sys/thread.h>
#include <sys/ppc_instr.h>
#include <vm/as.h>
#include <vm/hat_ppcmmu.h>
#include <sys/reboot.h>
#include <sys/avintr.h>
#include <sys/vtrace.h>
#include <sys/systeminfo.h>	/* for init_cpu_info */
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/cpupart.h>
#include <sys/pset.h>
#include <sys/regset.h>

#undef	SETUP_BOOTOPS
#define	GENESI_ODW 1

#if defined(DEBUG) && defined(GENESI_ODW)
#define	ODW_ASSERT(expr) ASSERT(expr)
#else
#define	ODW_ASSERT(expr)
#endif

#define	IS_MEMLIST(name) \
	(strequal(name, "virt-avail") ||	\
	strequal(name, "phys-avail") ||		\
	strequal(name, "phys-installed"))

static int prom_debug = 1;
#define	PRM_POINT(q)	if (prom_debug)		\
	prom_printf("%s:%d: %s\n", "mlsetup.c", __LINE__, q);

/*
 * Global Routines:
 * mlsetup()
 */
void init_cpu_info(struct cpu *cp);	/* called by start_other_cpus on MP */
unsigned int get_cpu_id(void);		/* figure out what CPU we found */
extern void  copy_handlers();


/*
 * Global Data:
 */

struct _klwp	lwp0;
struct proc	p0;
struct plock	p0lock;			/* persistent p_lock for p0 */


int	dcache_size;
int	dcache_blocksize;
int	dcache_sets;
int	icache_size;
int	icache_blocksize;
int	icache_sets;
int	unified_cache;
int	clock_frequency;
int	timebase_frequency;

/*
 * Static Routines:
 */
static void fiximp_obp(void);

/*
 * Setup routine called right before main(). Interposing this function
 * before main() allows us to call it in a machine-independent fashion.
 */
void
mlsetup(struct regs *rp, void *cif)
{
	extern struct classfuncs sys_classfuncs;
	extern pri_t maxclsyspri;
	extern void setcputype(void);
	extern disp_t cpu0_disp;
	extern void *t0stack;   /* XXX ?? - had to directly reference this */

	PRM_POINT("mlsetup() starting...");

	/*
	 * initialize cpu_self
	 */
	cpu[0]->cpu_self = cpu[0];
	/*
	 * initialize t0
	 */
	t0.t_stk = (caddr_t)rp - SA(MINFRAME);
	t0.t_stkbase = t0stack;		/* S10 */
	t0.t_pri = maxclsyspri - 3;
	t0.t_schedflag = TS_LOAD | TS_DONT_SWAP;
	t0.t_procp = &p0;
	t0.t_plockp = &p0lock.pl_lock;
	t0.t_lwp = &lwp0;
	t0.t_forw = &t0;
	t0.t_back = &t0;
	t0.t_next = &t0;
	t0.t_prev = &t0;
	t0.t_cpu = cpu[0];
	t0.t_disp_queue = &cpu0_disp;
	t0.t_bind_cpu = PBIND_NONE;
	t0.t_bind_pset = PS_NONE;
	t0.t_cpupart = &cp_default;
	t0.t_clfuncs = &sys_classfuncs.thread;
	t0.t_copyops = NULL;
	THREAD_ONPROC(&t0, CPU);

	lwp0.lwp_thread = &t0;
	lwp0.lwp_procp = &p0;
	lwp0.lwp_regs = (void *)rp;
	t0.t_tid = p0.p_lwpcnt = p0.p_lwprcnt = p0.p_lwpid = 1; /* S10 */

	p0.p_exec = NULL;
	p0.p_stat = SRUN;
	p0.p_flag = SSYS;
	p0.p_tlist = &t0;
	p0.p_stksize = 2*PAGESIZE;
	p0.p_stkpageszc = 0;		/* from /sun4/os */
	p0.p_as = &kas;
	p0.p_lockp = &p0lock;
	p0.p_utraps = NULL;		/* from /sun4/os */
	p0.p_brkpageszc = 0;		/* from /sun4/os */
	sigorset(&p0.p_ignore, &ignoredefault);

	CPU->cpu_thread = &t0;
	CPU->cpu_dispthread = &t0;
	bzero(&cpu0_disp, sizeof (disp_t));	/* from /sun4/os */
	CPU->cpu_disp = &cpu0_disp;
	CPU->cpu_disp->disp_cpu = CPU;
	CPU->cpu_idle_thread = &t0;
	CPU->cpu_flags = CPU_READY | CPU_RUNNING | CPU_EXISTS | CPU_ENABLE;
	CPU->cpu_id = 0;
	CPU->cpu_mask = 1;

	/*
	 * Initialize lists of available and active CPUs.
	 */
	cpu_list_init(CPU);

#ifdef  TRACE
	cpu[0]->cpu_trace.event_map = null_event_map;
#endif  /* TRACE */

	prom_printf("Initialized t0 thread.\n");

	/*
	 * setcputype();
	 * This may not be used anymore.
	 * We use get_cpu_id() below.
	 */
	fiximp_obp();
	init_cpu_info(CPU);

	/*
	 * This is required if we don't have a boot loader to set up
	 * the bootops structure.  Eventually, this should go away, but
	 * is useful here in the early stages of development.
	 */


#ifdef XXXPPC
	/*
	 * XXX map_wellknown_devices() exists only for sun4u and sun4v.
	 * XXX See usr/src/uts/sun4[uv]/os/fillsysinfo.c
	 * XXX There has never been the equivalant for x86 or PowerPC.
	 * XXX It is unlikely that we would ever be able to count on
	 * XXX any concept of well-known devices in embedded land,
	 * XXX for PowerPC or any other processor.
	 */
	map_wellknown_devices();

	/*
	 * XXX setcpudelay() exists only for sun4u and sun4v.
	 * XXX See usr/src/uts/sun4[uv]/io/hardclk.c.
	 * XXX There has never been the equivalant for x86 or PowerPC.
	 * XXX
	 * XXX Most of the logic in setcpudelay() has to do with
	 * XXX MP systems with CPUs that might have different clock
	 * XXX frequencies, and with system clocks that are system-wide
	 * XXX frequency sources, independent of CPU clock frequencies.
	 * XXX So, we are not likely to need this stuff for embedded
	 * XXX PowerPC-based systems.
	 */
	setcpudelay();
#endif

 	/* XXX - right now we do not parse the boot command line
         *
	 * bootflags();
	 *
	 */

	/*
	 * handlers need to be copied after mmu takeover - OF uses
	 * trap handlers to load entries in its pages tables to handle
	 * some address faults.
         *
	 */
	copy_handlers(); /* XXX -  code is properly moved to addrs, not all exceptions are functional */

	PRM_POINT("Exception Handlers installed. mlsetup() done.");
}

extern int swap_int(int *);

/*
 * Set the magic constants of the implementation
 */
static void
fiximp_obp(void)
{
	pnode_t rootnode, cpunode;
	pnode_t sp[OBP_STACKDEPTH];
	extern void *cifp;
	int i, a;
	int	dummy;
	pnode_t *stk;
	static struct {
		char	*name;
		int	*var;
	} prop[] = {
		"d-cache-size",		&dcache_size,
		"d-cache-block-size",	&dcache_blocksize,
		"d-cache-sets",		&dcache_sets,
		"i-cache-size",		&icache_size,
		"i-cache-block-size",	&icache_blocksize,
		"i-cache-sets",		&icache_sets,
		"clock-frequency",	&clock_frequency,
		"timebase-frequency",	&timebase_frequency,
	};

	PRM_POINT("fiximp_obp() starting...");

	rootnode = prom_rootnode();
	/*
	 * Find the first 'cpu' node - assume that all the
	 * modules are the same type - at least when we're
	 * determining magic constants.
	 */

	cpunode = prom_findnode_bydevtype(rootnode, "cpu");

	/*
	 * cache-unified is a boolean property: true if present.
	 */
	if (prom_getprop(cpunode, "cache-unified", (caddr_t)&dummy) != -1)
		unified_cache = 1;

	/*
	 * Read integer properties.
	 */

	for (i = 0; i < sizeof (prop) / sizeof (prop[0]); i++) {
		if (prom_getintprop(cpunode, prop[i].name, &a) != -1) {
			*prop[i].var = a;
		}
	}

	if (timebase_frequency > 0) {
		dec_incr_per_tick = timebase_frequency / hz;
		timebase_period = (int)((1000000000LL << NSEC_SHIFT)
			/ timebase_frequency);
		tbticks_per_10usec = timebase_frequency / 100000;
	}

	prom_printf("Obtained timebase frequency = %u hz\n",
			timebase_frequency);
	prom_printf("Calculated timebase period = %u\n", timebase_period);
	prom_printf("Calculated timebase ticks per 10usec = %u\n",
			tbticks_per_10usec);
#ifdef XXXPPC
        /* XXX caught inbetween the ODW and PowerMac since we're bouncing back and forth*/
          ODW_ASSERT(timebase_frequency == 41659214); /* ODW = 33333333, PowerMac = 41659214 */
          ODW_ASSERT(dec_incr_per_tick == 416592); /* ODW = 333333, PowerMac = 416592 */
          ODW_ASSERT(timebase_period == 0);
          ODW_ASSERT(tbticks_per_10usec == 416); /* ODW = 333, PowerMac = 416 */
#endif

	ASSERT(timebase_frequency >= dec_incr_per_tick * hz);
	ASSERT(timebase_frequency < dec_incr_per_tick * (hz + 1));
	PRM_POINT("fiximp_obp() done.");
}

/*
 * Init CPU info - get CPU type info for processor_info system call.
 */
void
init_cpu_info(struct cpu *cp)
{
	register processor_info_t *pi = &cp->cpu_type_info;
	unsigned	clock_freq = 0;
#ifdef MP
	dnode_t		root;
	static	char	freq[] = "clock-frequency";
	dnode_t		nodeid;
	unsigned	who;

	/*
	 * Get clock-frequency property for the CPU.
	 * Find node for CPU.  The first CPU is initialized before the
	 * cpu_nodeid table is setup.
	 */
	root = prom_nextnode((dnode_t)0);	/* root node */
	for (nodeid = prom_childnode(root);
		(nodeid != OBP_NONODE) && (nodeid != OBP_BADNODE);
		nodeid = prom_nextnode(nodeid)) {

		if ((prom_getproplen(nodeid, "mid") == sizeof (who)) &&
			(prom_getprop(nodeid, "mid", (caddr_t)&who) != -1)) {

			if (who == cp->cpu_id) {
				/*
				 * get clock frequency from CPU or root node.
				 * This will be zero if the property is
				 * not there.
				 */
				size_t sz;

				sz = prom_getintprop(nodeid, freq, &clock_freq);
				if (sz == sizeof (clock_freq))
					break;
			}
		}
	}
#else /* uniprocessor */
	clock_freq = clock_frequency;	/* use value found by fiximp_obp() */
#endif /* MP */

	/*
	 * Round to nearest megahertz.
	 */
	pi->pi_clock = (clock_freq + 500000) / 1000000;

	strcpy(pi->pi_processor_type, architecture);
	strcpy(pi->pi_fputypes, architecture);
}
