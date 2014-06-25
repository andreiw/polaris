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

/*
 * Copyright 2006 Cyril Plisko.  All rights reserved.
 * Use is subject to license terms.
 */

/* XXX - The lack of CTF merge/stabs has forced us to return to the pre 2.9 way of generating
 * these defines from the OFFSET macros. This needs to be updated when we have this added
 * functionality in place. 
 */

#pragma ident	"%Z%%M%	%I%	%E% CP"

/*
 * Generates fixed constants.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/machparam.h>
#include <sys/machcpuvar.h>
/* #include <sys/systm.h> */
/* #include <sys/elf_notes.h> */
#include <sys/thread.h>
/* #include <sys/rwlock.h> */
/* #include <sys/proc.h> */
#include <sys/cpuvar.h>
#include <sys/clock.h>
#include <sys/lwp.h>
/* #include <sys/trap.h> */
/* #include <sys/modctl.h> */
/* #include <sys/traptrace.h> */
/* #include <vm/seg.h> */
#include <sys/avintr.h>
/* #include <sys/pic.h>
#include <sys/pit.h> */
/* #include <sys/fp.h> */
#include <sys/archsystm.h>
/* #include <sys/sunddi.h> */

extern int printf(const char *restrict format, /* args*/ ...);

#define OFFSET(type, field)     ((int)(&((type *)0)->field))


int
main(int argc, char *argv[])
{

	register struct cpu *cpu = (struct cpu *)0;
	register kthread_id_t tid = (kthread_id_t)0;
	register klwp_id_t lwpid = (klwp_id_t)0;
	register struct av_head *avh = (struct av_head *)0;
	register struct autovec *av = (struct autovec *)0;

	printf("#define\tAVH_LINK 0x%x\n", &avh->avh_link);

	printf("#define\tAV_VECTOR 0x%x\n", &av->av_vector);
        /* 	printf("#define\tAV_INTARG 0x%x\n", &av->av_intarg); */
	printf("#define\tAV_LINK 0x%x\n", &av->av_link);

	printf("#define\tPCIISA_VBASE %d\n", PCIISA_VBASE);
	printf("#define\tT0STKSZ 0x%x\n", T0STKSZ);
	printf("#define\tTHREAD_SIZE %d\n", sizeof (kthread_t));
	printf("#define\tREGSIZE %d\n", sizeof (struct regs));

	printf("#define\tCPU_PRI 0x%x\n", &cpu->cpu_m.mcpu_pri);
	printf("#define\tCPU_CLOCKINTR 0x%x\n",
		OFFSET(struct cpu, cpu_clockintr));
	printf("#define\tCPU_CMNINT 0x%x\n", OFFSET(struct cpu, cpu_cmnint));
	printf("#define\tCPU_CMNTRAP 0x%x\n", OFFSET(struct cpu, cpu_cmntrap));
	printf("#define\tCPU_MSR_DISABLED 0x%x\n",
		OFFSET(struct cpu, cpu_msr_disabled));
	printf("#define\tCPU_MSR_ENABLED 0x%x\n",
		OFFSET(struct cpu, cpu_msr_enabled));
	printf("#define\tCPU_R3 0x%x\n", OFFSET(struct cpu, cpu_r3));
	printf("#define\tCPU_SRR0 0x%x\n", OFFSET(struct cpu, cpu_srr0));
	printf("#define\tCPU_SRR1 0x%x\n", OFFSET(struct cpu, cpu_srr1));
	printf("#define\tCPU_SYSCALL 0x%x\n", OFFSET(struct cpu, cpu_syscall));
	printf("#define\tCPU_BASE_SPL 0x%x\n",
		OFFSET(struct cpu, cpu_base_spl));
	printf("#define\tCPU_THREAD 0x%x\n", OFFSET(struct cpu, cpu_thread));

	printf("#define\tCPU_DSISR 0x%x\n", OFFSET(struct cpu, cpu_dsisr));
	printf("#define\tCPU_DAR 0x%x\n", OFFSET(struct cpu, cpu_dar));
	printf("#define\tCPU_ID 0x%x\n", OFFSET(struct cpu, cpu_id));
	printf("#define\tCPU_KPRUNRUN 0x%x\n",
			 OFFSET(struct cpu, cpu_kprunrun));

	printf("#define\tCPU_INTR_ACTV 0x%x\n",     /* XXX - the same element ?? */
		OFFSET(struct cpu, cpu_intr_actv));
	printf("#define\tCPU_ON_INTR 0x%x\n", OFFSET(struct cpu, cpu_intr_actv));

	printf("#define\tCPU_INTR_STACK 0x%x\n",
	    OFFSET(struct cpu, cpu_intr_stack));

	printf("#define\tCPU_INTR_THREAD 0x%x\n", &cpu->cpu_intr_thread);

	printf("#define\tT_STACK 0x%x\n", &tid->t_stk);
	printf("#define\tT_CPU 0x%x\n", &tid->t_cpu);
	printf("#define\tT_OLDSPL 0x%x\n", &tid->t_oldspl);
	printf("#define\tT_STATE 0x%x\n", &tid->t_state);
	printf("#define\tT_LOCKP 0x%x\n", &tid->t_lockp);
	printf("#define\tT_LWP 0x%x\n", &tid->t_lwp);
	printf("#define\tT_PROC_FLAG 0x%x\n", &tid->t_proc_flag);
	printf("#define\tT_SP 0x%x\n", &tid->t_sp);
	printf("#define\tT_INTR 0x%x\n", &tid->t_intr);
	printf("#define\tT_LINK 0x%x\n", &tid->t_link);
	printf("#define\tT_PRI 0x%x\n", &tid->t_pri);



	printf("#define\tLOCK_LEVEL 0x%x\n", LOCK_LEVEL);
	printf("#define\tCLOCK_LEVEL 0x%x\n", CLOCK_LEVEL);

	printf("#define\tLWP_PCB 0x%x\n", &lwpid->lwp_pcb);
	printf("#define\tLWP_PCB_FLAGS 0x%x\n", &lwpid->lwp_pcb.pcb_flags);
	printf("#define\tLWP_STATE_START 0x%x\n",
	    &lwpid->lwp_mstate.ms_state_start);
	printf("#define\tLWP_ACCT_USER 0x%x\n",
	    &lwpid->lwp_mstate.ms_acct[LMS_USER]);
	printf("#define\tLWP_MS_START 0x%x\n", &lwpid->lwp_mstate.ms_start);
        /* XXX - will deal with later  printf("#define\tLWP_UTIME 0x%x\n", &lwpid->lwpinfo.lwp_utime); */

	printf("#define\tTP_MSACCT 0x%x\n", TP_MSACCT);

	printf("#define\tONPROC_THREAD 0x%x\n", TS_ONPROC);


	return (0);
}
