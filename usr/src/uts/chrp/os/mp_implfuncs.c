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

#pragma ident	"%Z%%M%	%I%	%E% SML"


#include <sys/psm.h>

#include <sys/map.h>
#include <vm/hat_ppcmmu.h>

#include <sys/modctl.h>
#include <vm/seg_kmem.h>
#include <sys/psm.h>
#include <sys/psm_modctl.h>
#include <sys/smp_impldefs.h>

/*
 *	External reference functions
 */
extern void *get_next_mach(void *, char *);
extern void close_mach_list(void);
extern void open_mach_list(void);

int psm_add_intr(int lvl, avfunc xxintr, char *name, int vect, caddr_t arg);
int psm_add_nmintr(int lvl, avfunc xxintr, char *name, caddr_t arg);
processorid_t psm_get_cpu_id(void);
void psm_unmap(caddr_t addr, size_t len, ulong prot);
caddr_t psm_map(uint32_t addr, size_t len, int prot);
void psm_unmap_phys(caddr_t addr, long len);
caddr_t psm_map_phys(paddr_t addr, long len, u_long prot);
int psm_mod_init(void **modlpp, struct psm_info *infop);
int psm_mod_fini(void **modlpp, struct psm_info *infop);
int psm_mod_info(void **modlpp, struct psm_info *infop,
	struct modinfo *modinfop);
void psm_modloadonly(void);
void psm_install(void);
void psm_handle_memerror(uint32_t physaddr);

/*
 * Local Function Prototypes
 */
static struct modlinkage *psm_modlinkage_alloc(struct psm_info *infop);
static void psm_modlinkage_free(struct modlinkage *mlinkp);
static void psm_direct_unmap_phys(paddr_t addr, ulong len);
static void psm_direct_map_phys(paddr_t addr, ulong len, ulong prot);

static char *psm_get_impl_module(int first);

static int mod_installpsm(struct modlpsm *modl, struct modlinkage *modlp);
static int mod_removepsm(struct modlpsm *modl, struct modlinkage *modlp);
static int mod_infopsm(struct modlpsm *modl, struct modlinkage *modlp, int *p0);
struct mod_ops mod_psmops = {
	mod_installpsm, mod_removepsm, mod_infopsm
};

static struct psm_sw psm_swtab = {
	&psm_swtab, &psm_swtab, NULL, NULL
};

kmutex_t psmsw_lock;			/* lock accesses to psmsw 	*/
struct psm_sw *psmsw = &psm_swtab; 	/* start of all psm_sw		*/

static struct modlinkage *
psm_modlinkage_alloc(struct psm_info *infop)
{
	int	memsz;
	struct modlinkage *mlinkp;
	struct modlpsm *mlpsmp;
	struct psm_sw *swp;

	memsz = sizeof (struct modlinkage) + sizeof (struct modlpsm) +
		sizeof (struct psm_sw);
	mlinkp = (struct modlinkage *)kmem_zalloc(memsz, KM_NOSLEEP);
	if (!mlinkp) {
		cmn_err(CE_WARN, "!psm_mod_init: Cannot install %s",
			infop->p_mach_idstring);
		return (NULL);
	}
	mlpsmp = (struct modlpsm *)(mlinkp + 1);
	swp = (struct psm_sw *)(mlpsmp + 1);

	mlinkp->ml_rev = MODREV_1;
	mlinkp->ml_linkage[0] = (void *)mlpsmp;
	mlinkp->ml_linkage[1] = (void *)NULL;

	mlpsmp->psm_modops = &mod_psmops;
	mlpsmp->psm_linkinfo = infop->p_mach_desc;
	mlpsmp->psm_swp = swp;

	swp->psw_infop = infop;

	return (mlinkp);
}

static void
psm_modlinkage_free(struct modlinkage *mlinkp)
{
	if (!mlinkp)
		return;

	(void) kmem_free(mlinkp, (sizeof (struct modlinkage) +
		sizeof (struct modlpsm) + sizeof (struct psm_sw)));
}

int
psm_mod_init(void **handlepp, struct psm_info *infop)
{
	struct modlinkage **modlpp = (struct modlinkage **)handlepp;
	int	status;
	struct modlinkage *mlinkp;

	if (!*modlpp) {
		mlinkp = psm_modlinkage_alloc(infop);
		if (!mlinkp)
			return (ENOSPC);
	} else
		mlinkp = *modlpp;

	status = mod_install(mlinkp);
	if (status) {
		psm_modlinkage_free(mlinkp);
		*modlpp = NULL;
	} else
		*modlpp = mlinkp;

	return (status);
}

/*ARGSUSED1*/
int
psm_mod_fini(void **handlepp, struct psm_info *infop)
{
	struct modlinkage **modlpp = (struct modlinkage **)handlepp;
	int	status;

	status = mod_remove(*modlpp);
	if (status == 0) {
		psm_modlinkage_free(*modlpp);
		*modlpp = NULL;
	}
	return (status);
}

int
psm_mod_info(void **handlepp, struct psm_info *infop, struct modinfo *modinfop)
{
	struct modlinkage **modlpp = (struct modlinkage **)handlepp;
	int status;
	struct modlinkage *mlinkp;

	if (!*modlpp) {
		mlinkp = psm_modlinkage_alloc(infop);
		if (!mlinkp)
			return ((int)NULL);
	} else
		mlinkp = *modlpp;

	status =  mod_info(mlinkp, modinfop);

	if (!status) {
		psm_modlinkage_free(mlinkp);
		*modlpp = NULL;
	} else
		*modlpp = mlinkp;

	return (status);
}

int
psm_add_intr(int lvl, avfunc xxintr, char *name, int vect, caddr_t arg)
{
	return (add_avintr((void *)NULL, lvl, xxintr, name, vect, arg,
		NULL, NULL, NULL));
}

int
psm_add_nmintr(int lvl, avfunc xxintr, char *name, caddr_t arg)
{
	return (add_nmintr(lvl, xxintr, name, arg));
}

processorid_t
psm_get_cpu_id(void)
{
	return (CPU->cpu_id);
}

caddr_t
psm_map(uint32_t addr, size_t len, int prot)
{
	if (prot == PSM_PROT_READ)
		return (psm_map_phys(addr, (long)len, PROT_READ));
	else if (prot == PSM_PROT_WRITE)
		return (psm_map_phys(addr, (long)len, PROT_READ|PROT_WRITE));
	else if (prot & PSM_PROT_SAMEADDR) {
		if (prot & PSM_PROT_WRITE)
			psm_direct_map_phys(addr, len, PROT_READ|PROT_WRITE);
		else
			psm_direct_map_phys(addr, len, PROT_READ);

		return ((caddr_t)addr);
	} else
		return ((caddr_t)NULL);
}

void
psm_unmap(caddr_t addr, size_t len, ulong prot)
{
	if ((prot == PSM_PROT_WRITE) || (prot == PSM_PROT_READ))
		psm_unmap_phys(addr, (long)len);
	else if (prot & PSM_PROT_SAMEADDR)
          while (1); /* psm_direct_unmap_phys((paddr_t)addr, len); XXX - unimplemented call*/
}

caddr_t
psm_map_phys(paddr_t addr, long len, u_long prot)
{
	extern	struct seg kvseg;
	register u_int	pgoffset;
	register u_long	base;
	u_long	addridx;
	u_long	cvaddr;
	u_int	npages;

	if (len == 0)
		return (0);

	base = (u_long) addr & (~MMU_PAGEOFFSET);
	pgoffset = (u_int) addr & MMU_PAGEOFFSET;

	npages = mmu_btopr(len + pgoffset);

#if defined(XXX_OBSOLETE)
	if (!(addridx = rmalloc(kernelmap, (long)npages)))
		return (0);
	cvaddr = (u_long)kmxtob(addridx);
#endif /* XXX_OBSOLETE */

	segkmem_mapin(&kvseg, (caddr_t)cvaddr, mmu_ptob(npages), prot,
		mmu_btop(base), 0);
	return ((caddr_t)(cvaddr + pgoffset));
}

void
psm_unmap_phys(caddr_t addr, long len)
{
	extern	struct seg kvseg;
	register u_int	npages;

	if (len == 0)
		return;

	npages = mmu_btopr(len + ((u_int)addr & MMU_PAGEOFFSET));
	segkmem_mapout(&kvseg, (caddr_t)((u_int)addr&(~MMU_PAGEOFFSET)),
		mmu_ptob(npages));
#if defined(XXX_OBSOLETE)
	rmfree(kernelmap, (long)npages, (u_long)btokmx(addr));
#endif /* XXX_OBSOLETE */

}

static void
psm_direct_map_phys(paddr_t addr, ulong len, ulong prot)
{
	register int i;
	paddr_t paddr;

#if defined(XXX_OBSOLETE) /* func is not implemented in S10 */
	for (i = 0, paddr = addr; i < len; i += MMU_PAGESIZE) {
		hat_devload(kas.a_hat, (caddr_t)paddr, MMU_PAGESIZE,
			btop(paddr), prot | HAT_NOSYNC,
			HAT_LOAD_LOCK);
		paddr += MMU_PAGESIZE;
	}
#endif /* XXX_OBSOLETE */


}

static void
psm_direct_unmap_phys(paddr_t addr, ulong len)
{

#if defined(XXX_OBSOLETE) /* func is not implemented in S10 */

	hat_unload(kas.a_hat, (caddr_t)addr, len, NULL);
#endif /* XXX_OBSOLETE */

}

/*ARGSUSED1*/
static int
mod_installpsm(struct modlpsm *modl, struct modlinkage *modlp)
{
	register struct psm_sw *swp;

	swp = modl->psm_swp;
	mutex_enter(&psmsw_lock);
	psmsw->psw_back->psw_forw = swp;
	swp->psw_back = psmsw->psw_back;
	swp->psw_forw = psmsw;
	psmsw->psw_back = swp;
	swp->psw_flag |= PSM_MOD_INSTALL;
	mutex_exit(&psmsw_lock);
	return (0);
}

/*ARGSUSED1*/
static int
mod_removepsm(struct modlpsm *modl, struct modlinkage *modlp)
{
	register struct psm_sw *swp;

	swp = modl->psm_swp;
	mutex_enter(&psmsw_lock);
	if (swp->psw_flag & PSM_MOD_IDENTIFY) {
		mutex_exit(&psmsw_lock);
		return (EBUSY);
	}
	if (!(swp->psw_flag & PSM_MOD_INSTALL)) {
		mutex_exit(&psmsw_lock);
		return (0);
	}

	swp->psw_back->psw_forw = swp->psw_forw;
	swp->psw_forw->psw_back = swp->psw_back;
	mutex_exit(&psmsw_lock);
	return (0);
}

/*ARGSUSED1*/
static int
mod_infopsm(struct modlpsm *modl, struct modlinkage *modlp, int *p0)
{
	*p0 = (int)modl->psm_swp->psw_infop->p_owner;
	return (0);
}

static char *
psm_get_impl_module(int first)
{
	static char **pnamep;
	static char *psm_impl_module_list[] = {
		"uppc",
		(char *)0
	};
	static void *mhdl = NULL;
	static char machname[MAXNAMELEN];

	if (first)
		pnamep = psm_impl_module_list;

	if (*pnamep != (char *)0)
		return (*pnamep++);

	mhdl = get_next_mach(mhdl, machname);
	if (mhdl)
		return (machname);
	return ((char *)0);
}

void
psm_modload(void)
{
	register char *this;

	mutex_init(&psmsw_lock, "psmsw lock", MUTEX_DEFAULT, NULL);
	open_mach_list();

	for (this = psm_get_impl_module(1);
		this != (char *) NULL;
		this = psm_get_impl_module(0)) {
		if (modload("mach", this) == -1)
			cmn_err(CE_WARN, "!Cannot load psm %s", this);
	}
	close_mach_list();
}

void
psm_install(void)
{
	register struct psm_sw *swp, *cswp;
	struct psm_ops *opsp;
	char machstring[15];
	int err;

	mutex_enter(&psmsw_lock);
	swp = psmsw->psw_forw;
	while (swp != psmsw) {
		if ((swp->psw_infop->p_version & 0xFFF0) == PSM_INFO_VER01_X) {
			opsp = swp->psw_infop->p_ops;
			if (opsp->psm_probe) {
				if ((*opsp->psm_probe)() == PSM_SUCCESS) {
					swp->psw_flag |= PSM_MOD_IDENTIFY;
					swp = swp->psw_forw;
					continue;
				}
			}
		} else
			cmn_err(CE_WARN,
			"%s module unloaded - PSMI version %x not supported",
			swp->psw_infop->p_mach_idstring,
			swp->psw_infop->p_version);

		/* remove the unsuccessful psm modules */
		cswp = swp;
		swp = swp->psw_forw;

		mutex_exit(&psmsw_lock);
		strcpy(&machstring[0], cswp->psw_infop->p_mach_idstring);
		err = mod_remove_by_name(cswp->psw_infop->p_mach_idstring);
		if (err)
			cmn_err(CE_WARN, "%s: mod_remove_by_name failed %d",
				&machstring[0], err);
		mutex_enter(&psmsw_lock);
	}
	mutex_exit(&psmsw_lock);
	(*psminitf)();
}

void
psm_handle_memerror(uint32_t physaddr)
{
	/*
	 *  waiting for the common code to handle the memory error, so
	 *  just panic at this moment
	 */
	cmn_err(CE_PANIC,
		"psm_handle_memerror: error reported at physaddr 0x%x\n",
		physaddr);
}
