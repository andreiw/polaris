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

#pragma ident   "%Z%%M% %I%     %E% SML"
#include <sys/types.h>
#include <sys/cmn_err.h>

/*
 * This file is a catch-all for undefined global data
 * and unimplemented functions.
 *
 * Unimplemented functions panic the system.
 *
 * Functions that are known to be MP-unsafe will work fine
 * when running on a single CPU, but panic if the system
 * is currently running with more than one CPU enabled.
 *
 */

#define	__TOSTR(x)	#x
#define	TOSTR(x)	__TOSTR(x)

#define	UNIMPLEMENTED(module, func_name, rtype)		\
	rtype __attribute__((noreturn))			\
	func_name(void) { unimplemented(TOSTR(func_name)); }

/*
 * Unimplemented global data
 */
void *heap_arena;

/* These two should go in the ppc_subr.s */
int64_t hrestime_adj;
int64_t timedelta;

extern ulong_t  cpu_ready_set;

#define	CLEAR_BSS
#ifdef CLEAR_BSS
extern char __bss_start, _end;
void
clear_bss(void)
{
	/* Zero out the BSS segment */
	bzero((void *) &__bss_start, &_end - &__bss_start);
}

void
print_bss(void)
{
	prom_printf("\nStarting Opensolaris on PowerPC...\n");
	prom_printf("Sun Labs"
		" in collaboration with the OpenSolaris PowerPC community.\n");
	prom_printf("BSS cleared from %08X - %08X (Size %X)\n",
			&__bss_start, &_end, &_end - &__bss_start);
}

void
leave_locore(void)
{
	prom_printf("Number test for 127 in o d x: %o %d %x\n", 127, 127, 127);
	prom_printf("Leaving locore.s ... executing main()...\n");
}
#else
void
clear_bss(void) { /* do nothing */ }

void
print_bss(void)
{
	prom_printf("\nStarting Solaris on PowerPC...\n");
}

void
leave_locore(void)
{
	prom_printf("Leaving locore.s ... executing main()...\n");
}
#endif /* CLEAR_BSS */

void
mp_unsafe(void)
{
	if (cpu_ready_set == 1)
		return;

	panic("MP-unsafe function (cpu_ready_set=%lx).\n",
		cpu_ready_set);
}

void
__attribute__((noreturn))
unimplemented(char *func_name)
{
	prom_printf("Unimplemented function: %s()\n", func_name);
	panic("Giving up...\n");
}

/* XXX - Other Temporary Bandaids */

UNIMPLEMENTED(AAA, hr_clock_lock, int)
UNIMPLEMENTED(AAA, hr_clock_unlock, int)
UNIMPLEMENTED(AAA, tod_get, int)
UNIMPLEMENTED(AAA, tod_set, int)
UNIMPLEMENTED(AAA, tsc_read, int)

UNIMPLEMENTED(addupc, addupc, void)

UNIMPLEMENTED(autovec, intr_get_time, int)
UNIMPLEMENTED(autovec, return_instr, int)

UNIMPLEMENTED(cbe, cbe_init, int)

UNIMPLEMENTED(cons_subr, console_char_no_output, int)
UNIMPLEMENTED(cons_subr, console_connect, void)
UNIMPLEMENTED(cons_subr, console_default_bell, void)
UNIMPLEMENTED(cons_subr, console_disconnect, void)
UNIMPLEMENTED(cons_subr, console_get_devname, void)
UNIMPLEMENTED(cons_subr, console_kadb_write_char, int)
UNIMPLEMENTED(cons_subr, stdout_is_framebuffer, int)
UNIMPLEMENTED(cons_subr, stdout_path, int)
UNIMPLEMENTED(cons_subr, use_bifont, int)

UNIMPLEMENTED(cpc_subr, kcpc_hw_init, int)
UNIMPLEMENTED(cpc_subr, kcpc_remote_stop, int)

UNIMPLEMENTED(ddi_arch, drv_usecwait, int)
UNIMPLEMENTED(ddi_arch, i_ddi_apply_range, int)
UNIMPLEMENTED(ddi_arch, i_ddi_bus_map, int)
UNIMPLEMENTED(ddi_arch, i_ddi_map_fault, int)
UNIMPLEMENTED(ddi_arch, i_ddi_rnumber_to_regspec, int)
UNIMPLEMENTED(ddi_arch, impl_ddi_prop_int_from_prom, int)

UNIMPLEMENTED(ddi_ppc, i_ddi_acc_clr_fault, int)
UNIMPLEMENTED(ddi_ppc, i_ddi_acc_set_fault, int)
UNIMPLEMENTED(ddi_ppc, impl_acc_hdl_alloc, int)
UNIMPLEMENTED(ddi_ppc, impl_acc_hdl_free, int)
UNIMPLEMENTED(ddi_ppc, impl_acc_hdl_get, int)

UNIMPLEMENTED(ddi_ppc_asm, ddi_get16, int)
UNIMPLEMENTED(ddi_ppc_asm, ddi_get32, int)
UNIMPLEMENTED(ddi_ppc_asm, ddi_get64, int)
UNIMPLEMENTED(ddi_ppc_asm, ddi_get8, int)
UNIMPLEMENTED(ddi_ppc_asm, ddi_put16, int)
UNIMPLEMENTED(ddi_ppc_asm, ddi_put32, int)
UNIMPLEMENTED(ddi_ppc_asm, ddi_put64, int)
UNIMPLEMENTED(ddi_ppc_asm, ddi_put8, int)
UNIMPLEMENTED(ddi_ppc_asm, ddi_rep_get16, int)
UNIMPLEMENTED(ddi_ppc_asm, ddi_rep_get32, int)
UNIMPLEMENTED(ddi_ppc_asm, ddi_rep_get64, int)
UNIMPLEMENTED(ddi_ppc_asm, ddi_rep_get8, int)
UNIMPLEMENTED(ddi_ppc_asm, ddi_rep_put16, int)
UNIMPLEMENTED(ddi_ppc_asm, ddi_rep_put32, int)
UNIMPLEMENTED(ddi_ppc_asm, ddi_rep_put64, int)
UNIMPLEMENTED(ddi_ppc_asm, ddi_rep_put8, int)

UNIMPLEMENTED(dtrace_subr, dtrace_safe_defer_signal, int)
UNIMPLEMENTED(dtrace_subr, dtrace_safe_synchronous_signal, int)

UNIMPLEMENTED(getcontext, getsetcontext, int)
UNIMPLEMENTED(getcontext, savecontext, void)

/*
 * Defined in usr/src/uts/prep/ml/locore.s in Solaris 2.6.
 * Needed by hat_ppcmmu.c in function ppcmmu_takeover().
 */

UNIMPLEMENTED(hat, hat_page_demote, int)
UNIMPLEMENTED(hat, hat_reserve, int)

UNIMPLEMENTED(install_utrap, install_utrap, int)

UNIMPLEMENTED(intr, cpu_create_intrstat, int)
UNIMPLEMENTED(intr, cpu_delete_intrstat, int)
UNIMPLEMENTED(intr, cpu_intr_swtch_enter, int)
UNIMPLEMENTED(intr, cpu_intr_swtch_exit, int)

UNIMPLEMENTED(krtld, hwc_parse_now, int)
UNIMPLEMENTED(krtld, kobj_addrcheck, int)
UNIMPLEMENTED(krtld, kobj_close, int)
UNIMPLEMENTED(krtld, kobj_close_file, int)
UNIMPLEMENTED(krtld, kobj_filbuf, int)
UNIMPLEMENTED(krtld, kobj_free, int)
UNIMPLEMENTED(krtld, kobj_getelfsym, int)
UNIMPLEMENTED(krtld, kobj_getmodinfo, int)
UNIMPLEMENTED(krtld, kobj_getsymname, int)
UNIMPLEMENTED(krtld, kobj_getsymvalue, int)
UNIMPLEMENTED(krtld, kobj_load_module, int)
UNIMPLEMENTED(krtld, kobj_lookup, int)
UNIMPLEMENTED(krtld, kobj_open, int)
UNIMPLEMENTED(krtld, kobj_open_file, int)
UNIMPLEMENTED(krtld, kobj_open_path, int)
UNIMPLEMENTED(krtld, kobj_path_exists, int)
UNIMPLEMENTED(krtld, kobj_read, int)
UNIMPLEMENTED(krtld, kobj_read_file, int)
UNIMPLEMENTED(krtld, kobj_stat_get, int)
UNIMPLEMENTED(krtld, kobj_sync, int)
UNIMPLEMENTED(krtld, kobj_unload_module, int)
UNIMPLEMENTED(krtld, kobj_zalloc, int)
UNIMPLEMENTED(krtld, mod_load, int)
UNIMPLEMENTED(krtld, mod_unload, int)

UNIMPLEMENTED(machdep, boot_virt_alloc, int)
UNIMPLEMENTED(machdep, console_enter, int)
UNIMPLEMENTED(machdep, console_exit, int)
UNIMPLEMENTED(machdep, get_cpu_mstate, int)
UNIMPLEMENTED(machdep, gethrestime_lasttick, int)
UNIMPLEMENTED(machdep, gethrestime_sec, int)
UNIMPLEMENTED(machdep, i_ddi_intr_redist_all_cpus, int)
UNIMPLEMENTED(machdep, lwp_stk_fini, int)
UNIMPLEMENTED(machdep, mdpreboot, int)
UNIMPLEMENTED(machdep, panic_dump_hw, int)
UNIMPLEMENTED(machdep, panic_enter_hw, int)
UNIMPLEMENTED(machdep, panic_quiesce_hw, int)
UNIMPLEMENTED(machdep, panic_stopcpus, int)
UNIMPLEMENTED(machdep, panicbuf, int)
UNIMPLEMENTED(machdep, plat_tod_fault, int)
UNIMPLEMENTED(machdep, set_idle_cpu, int)
UNIMPLEMENTED(machdep, unset_idle_cpu, int)

UNIMPLEMENTED(modstubs, modstubs_STUB_COMMON, void)
UNIMPLEMENTED(modstubs, modstubs_STUB_UNLOADABLE, void)
UNIMPLEMENTED(modstubs, modstubs_stubs_common_code, void)

UNIMPLEMENTED(mp_call, poke_cpu, int)

UNIMPLEMENTED(mp_machdep, chip_plat_define_chip, int)

UNIMPLEMENTED(mach_sysconfig, mach_sysconfig, int)
UNIMPLEMENTED(mach_sysconfig, nodename_set, int)

UNIMPLEMENTED(ppage, ppmapin, int)
UNIMPLEMENTED(ppage, ppmapout, int)

UNIMPLEMENTED(ppc_procfs, prstep, int)
UNIMPLEMENTED(ppc_procfs, prstop, int)
UNIMPLEMENTED(ppc_procfs, prnostep, int)

UNIMPLEMENTED(ppc_subr, adj_shift, int)
UNIMPLEMENTED(ppc_subr, bcmp, int)
UNIMPLEMENTED(ppc_subr, dtrace_interrupt_disable, int)
UNIMPLEMENTED(ppc_subr, dtrace_interrupt_enable, int)
UNIMPLEMENTED(ppc_subr, dtrace_membar_consumer, int)
UNIMPLEMENTED(ppc_subr, dtrace_membar_producer, int)
UNIMPLEMENTED(ppc_subr, hrestime, int)
UNIMPLEMENTED(ppc_subr, ip_ocsum, int)
UNIMPLEMENTED(ppc_subr, on_trap, int)
UNIMPLEMENTED(ppc_subr, panic_trigger, int)
UNIMPLEMENTED(ppc_subr, prefetch_page_r, int)
UNIMPLEMENTED(ppc_subr, prefetch_smap_w, int)
UNIMPLEMENTED(ppc_subr, strlen, int)

UNIMPLEMENTED(prmachdep, pr_watch_emul, int)

UNIMPLEMENTED(promif, init_callback_handler, int)

UNIMPLEMENTED(sundep, lwp_pcb_exit, int)

UNIMPLEMENTED(swtchS, resume, void)
UNIMPLEMENTED(swtchS, resume_from_intr, void)
UNIMPLEMENTED(swtchS, resume_from_zombie, void)
UNIMPLEMENTED(swtchS, thread_start, void)

UNIMPLEMENTED(syscall, indir, int)
UNIMPLEMENTED(syscall, loadable_syscall, int)
UNIMPLEMENTED(syscall, nosys, int)
UNIMPLEMENTED(syscall, realsigprof, void)
UNIMPLEMENTED(syscall, reset_syscall_args, int)
UNIMPLEMENTED(syscall, save_syscall_args, int)
UNIMPLEMENTED(syscall, set_errno, int)
UNIMPLEMENTED(syscall, set_proc_ast, int)
UNIMPLEMENTED(syscall, set_proc_post_sys, int)
UNIMPLEMENTED(syscall, syscall_ap, int)

UNIMPLEMENTED(timestamp, dtrace_gethrtime, int)

UNIMPLEMENTED(trap, panic_savetrap, int)
UNIMPLEMENTED(trap, panic_showtrap, int)

UNIMPLEMENTED(vm_depH, CHK_LPG, int)
UNIMPLEMENTED(vm_depH, FULL_REGION_CNT, int)
UNIMPLEMENTED(vm_depH, MTYPE_INIT, int)
UNIMPLEMENTED(vm_depH, PLCNT_DECR, int)
UNIMPLEMENTED(vm_depH, PLCNT_INCR, int)
UNIMPLEMENTED(vm_depH, PLCNT_MODIFY_MAX, int)
UNIMPLEMENTED(vm_depH, SZC_2_USERSZC, int)
UNIMPLEMENTED(vm_depH, USERSZC_2_SZC, int)

UNIMPLEMENTED(vm_machdep, mnoderangecnt, int)
UNIMPLEMENTED(vm_machdep, mnoderanges, int)
UNIMPLEMENTED(vm_machdep, mtype_func, int)
UNIMPLEMENTED(vm_machdep, pageout_init, int)
UNIMPLEMENTED(vm_machdep, pfn_2_mtype, int)
UNIMPLEMENTED(vm_machdep, hat_mempte_remap, void)

UNIMPLEMENTED(xxx, lwp_stk_cache_init, int)
UNIMPLEMENTED(xxx, map_shm_pgszcvec, int)
