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
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 *
 * Copyright 2006 Sun Microsystems Laboratories.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef _SYS_MACHSYSTM_H
#define	_SYS_MACHSYSTM_H

#pragma ident	"%Z%%M%	%I%	%E% SMI"

/*
 * Numerous platform-dependent interfaces that don't seem to belong
 * in any other header file.
 *
 * This file should not be included by code that purports to be
 * platform-independent.
 *
 */

#include <sys/varargs.h>
#include <sys/machparam.h>
#include <sys/cpuvar.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _KERNEL

extern void mp_halt(char *);

/*
 * PowerPC declares spl*() functions as static __inline__.
 * See usr/src/uts/ppc/sys/ppc_subr.h
 */

#if 0

extern int splzs(void);
#ifndef splimp
/* XXX go fix kobj.c so we can kill splimp altogether! */
extern int splimp(void);
#endif

#endif /* 0 */

extern int Cpudelay;
extern void setcpudelay(void);

struct cpu;
extern void init_intr_threads(struct cpu *);

struct _kthread;
extern struct _kthread *clock_thread;
extern void init_clock_thread(void);

extern int setup_panic(char *, va_list);

extern void siron(void);

extern void return_instr(void);

extern int pf_is_memory(pfn_t);

extern int noprintf;
extern int do_pg_coloring;
extern int use_page_coloring;
extern int pokefault;

extern u_int cpu_nodeid[];

struct memconf {
	pfn_t	mcf_spfn;	/* begin page frame number */
	pfn_t	mcf_epfn;	/* end   page frame number */
};

struct system_hardware {
	int		hd_nodes;		/* number of nodes */
	int		hd_cpus_per_node;	/* max cpus in a node */
	struct memconf	hd_mem[MAXNODES];	/* memory layout per node */
};
extern struct system_hardware system_hardware;
extern void get_system_configuration(void);

/* kpm mapping window */
extern caddr_t  kpm_vbase;
extern size_t   kpm_size;
extern uchar_t  kpm_size_shift;

#endif /* _KERNEL */

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_MACHSYSTM_H */
