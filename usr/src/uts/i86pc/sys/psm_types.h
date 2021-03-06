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

#ifndef	_SYS_PSM_TYPES_H
#define	_SYS_PSM_TYPES_H

#pragma ident	"@(#)psm_types.h	1.17	05/11/11 SMI"

/*
 * Platform Specific Module Types
 */

#include <sys/types.h>
#include <sys/cpuvar.h>
#include <sys/time.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * PSM_OPS definitions
 */
typedef enum psm_intr_op_e {
	PSM_INTR_OP_ALLOC_VECTORS = 0,	/* 0.  Allocate vectors */
	PSM_INTR_OP_FREE_VECTORS,	/* 1.  Free vectors */
	PSM_INTR_OP_NAVAIL_VECTORS,	/* 2.  Get # of available vectors */
	PSM_INTR_OP_XLATE_VECTOR,	/* 3.  Translate vector */
	PSM_INTR_OP_GET_PENDING,	/* 4.  Get pending information */
	PSM_INTR_OP_CLEAR_MASK,		/* 5.  Clear interrupt mask */
	PSM_INTR_OP_SET_MASK,		/* 6.  Set interrupt mask */
	PSM_INTR_OP_GET_CAP,		/* 7.  Get devices's capabilities */
	PSM_INTR_OP_SET_CAP,		/* 8.  Set devices's capabilities */
	PSM_INTR_OP_SET_PRI,		/* 9.  Set the interrupt priority */
	PSM_INTR_OP_GET_SHARED,		/* 10. Get the shared intr info */
	PSM_INTR_OP_CHECK_MSI,		/* 11. Chk if device supports MSI */
	PSM_INTR_OP_SET_CPU,		/* 12. Set vector's CPU */
	PSM_INTR_OP_GET_INTR		/* 13. Get vector's info */
} psm_intr_op_t;

struct 	psm_ops {
	int	(*psm_probe)(void);

	void	(*psm_softinit)(void);
	void	(*psm_picinit)(void);
	int	(*psm_intr_enter)(int ipl, int *vectorp);
	void	(*psm_intr_exit)(int ipl, int irqno);
	void	(*psm_setspl)(int ipl);
	int	(*psm_addspl)(int irqno, int ipl, int min_ipl, int max_ipl);
	int	(*psm_delspl)(int irqno, int ipl, int min_ipl, int max_ipl);
	int	(*psm_disable_intr)(processorid_t cpun);
	void	(*psm_enable_intr)(processorid_t cpun);
	int	(*psm_softlvl_to_irq)(int ipl);
	void	(*psm_set_softintr)(int ipl);
	void	(*psm_set_idlecpu)(processorid_t cpun);
	void	(*psm_unset_idlecpu)(processorid_t cpun);

#if defined(PSMI_1_3) || defined(PSMI_1_4) || defined(PSMI_1_5)
	int	(*psm_clkinit)(int hertz);
#else
	void	(*psm_clkinit)(int hertz);
#endif

	int	(*psm_get_clockirq)(int ipl);
	void	(*psm_hrtimeinit)(void);
	hrtime_t (*psm_gethrtime)(void);

	processorid_t (*psm_get_next_processorid)(processorid_t cpu_id);
	void	(*psm_cpu_start)(processorid_t cpun, caddr_t rm_code);
	int	(*psm_post_cpu_start)(void);
#if defined(PSMI_1_2) || defined(PSMI_1_3) || defined(PSMI_1_4) || \
    defined(PSMI_1_5)
	void	(*psm_shutdown)(int cmd, int fcn);
#else
	void	(*psm_shutdown)(void);
#endif
	int	(*psm_get_ipivect)(int ipl, int type);
	void	(*psm_send_ipi)(processorid_t cpun, int ipl);

	int	(*psm_translate_irq)(dev_info_t *dip, int irqno);

	int	(*psm_tod_get)(todinfo_t *tod);
	int	(*psm_tod_set)(todinfo_t *tod);

	void	(*psm_notify_error)(int level, char *errmsg);
#if defined(PSMI_1_2) || defined(PSMI_1_3) || defined(PSMI_1_4) || \
    defined(PSMI_1_5)
	void	(*psm_notify_func)(int msg);
#endif
#if defined(PSMI_1_3) || defined(PSMI_1_4) || defined(PSMI_1_5)
	void 	(*psm_timer_reprogram)(hrtime_t time);
	void	(*psm_timer_enable)(void);
	void 	(*psm_timer_disable)(void);
	void 	(*psm_post_cyclic_setup)(void *arg);
#endif
#if defined(PSMI_1_4) || defined(PSMI_1_5)
	void	(*psm_preshutdown)(int cmd, int fcn);
#endif
#if defined(PSMI_1_5)
	int	(*psm_intr_ops)(dev_info_t *dip, ddi_intr_handle_impl_t *handle,
		    psm_intr_op_t op, int *result);
#endif
};


struct 	psm_info {
	ushort_t p_version;
	ushort_t p_owner;
	struct 	psm_ops	*p_ops;
	char	*p_mach_idstring;	/* machine identification string */
	char	*p_mach_desc;		/* machine descriptions		 */
};

/*
 * version
 * 0x86vm where v = (version no. - 1) and m = (minor no. + 1)
 * i.e. psmi 1.0 has v=0 and m=1, psmi 1.1 has v=0 and m=2
 * also, 0x86 in the high byte is the signature of the psmi
 */
#define	PSM_INFO_VER01		0x8601
#define	PSM_INFO_VER01_1	0x8602
#define	PSM_INFO_VER01_2	0x8603
#define	PSM_INFO_VER01_3	0x8604
#define	PSM_INFO_VER01_4	0x8605
#define	PSM_INFO_VER01_5	0x8606
#define	PSM_INFO_VER01_X	(PSM_INFO_VER01_1 & 0xFFF0)	/* ver 1.X */

/*
 *	owner field definitions
 */
#define	PSM_OWN_SYS_DEFAULT	0x0001
#define	PSM_OWN_EXCLUSIVE	0x0002
#define	PSM_OWN_OVERRIDE	0x0003

#define	PSM_NULL_INFO		-1

/*
 *	Arg to psm_notify_func
 */
#define	PSM_DEBUG_ENTER		1
#define	PSM_DEBUG_EXIT		2
#define	PSM_PANIC_ENTER		3

/*
 *	Soft-level to interrupt vector
 */
#define	PSM_SV_SOFTWARE		-1
#define	PSM_SV_MIXED		-2

/*
 *	Inter-processor interrupt type
 */
#define	PSM_INTR_IPI_HI		0x01
#define	PSM_INTR_IPI_LO		0x02
#define	PSM_INTR_POKE		0x03

/*
 *	Get INTR flags
 */
#define	PSMGI_CPU_USER_BOUND	0x80000000 /* user requested bind if set */
#define	PSMGI_CPU_FLAGS		0x80000000 /* all possible flags */

/*
 *	return code
 */
#define	PSM_SUCCESS		DDI_SUCCESS
#define	PSM_FAILURE		DDI_FAILURE

#define	PSM_INVALID_IPL		0
#define	PSM_INVALID_CPU		-1


struct 	psm_ops_ver01 {
	int	(*psm_probe)(void);

	void	(*psm_softinit)(void);
	void	(*psm_picinit)(void);
	int	(*psm_intr_enter)(int ipl, int *vectorp);
	void	(*psm_intr_exit)(int ipl, int irqno);
	void	(*psm_setspl)(int ipl);
	int	(*psm_addspl)(int irqno, int ipl, int min_ipl, int max_ipl);
	int	(*psm_delspl)(int irqno, int ipl, int min_ipl, int max_ipl);
	int	(*psm_disable_intr)(processorid_t cpun);
	void	(*psm_enable_intr)(processorid_t cpun);
	int	(*psm_softlvl_to_irq)(int ipl);
	void	(*psm_set_softintr)(int ipl);
	void	(*psm_set_idlecpu)(processorid_t cpun);
	void	(*psm_unset_idlecpu)(processorid_t cpun);

	void	(*psm_clkinit)(int hertz);
	int	(*psm_get_clockirq)(int ipl);
	void	(*psm_hrtimeinit)(void);
	hrtime_t (*psm_gethrtime)(void);

	processorid_t (*psm_get_next_processorid)(processorid_t cpu_id);
	void	(*psm_cpu_start)(processorid_t cpun, caddr_t rm_code);
	int	(*psm_post_cpu_start)(void);
	void	(*psm_shutdown)(void);
	int	(*psm_get_ipivect)(int ipl, int type);
	void	(*psm_send_ipi)(processorid_t cpun, int ipl);
};

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PSM_TYPES_H */
