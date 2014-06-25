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
 * Copyright 2005 Cyril Plisko.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef _SYS_DDI_ISA_H
#define	_SYS_DDI_ISA_H

#pragma ident	"%Z%%M%	%I%	%E% CP"

/*
 * Block comment that describes the contents of this file.
 */

#include <sys/isa_defs.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_KERNEL)

typedef struct ddi_acc_impl {
	ddi_acc_hdl_t	ahi_common;
	uint64_t	ahi_acc_attr;

	uint8_t
		(*ahi_get8)(struct ddi_acc_impl *handle, uint8_t *addr);
	uint16_t
		(*ahi_get16)(struct ddi_acc_impl *handle, uint16_t *addr);
	uint32_t
		(*ahi_get32)(struct ddi_acc_impl *handle, uint32_t *addr);
	uint64_t
		(*ahi_get64)(struct ddi_acc_impl *handle, uint64_t *addr);

	void	(*ahi_put8)(struct ddi_acc_impl *handle, uint8_t *addr,
			uint8_t value);
	void	(*ahi_put16)(struct ddi_acc_impl *handle, uint16_t *addr,
			uint16_t value);
	void	(*ahi_put32)(struct ddi_acc_impl *handle, uint32_t *addr,
			uint32_t value);
	void	(*ahi_put64)(struct ddi_acc_impl *handle, uint64_t *addr,
			uint64_t value);

	void	(*ahi_rep_get8)(struct ddi_acc_impl *handle,
			uint8_t *host_addr, uint8_t *dev_addr,
			size_t repcount, uint_t flags);
	void	(*ahi_rep_get16)(struct ddi_acc_impl *handle,
			uint16_t *host_addr, uint16_t *dev_addr,
			size_t repcount, uint_t flags);
	void	(*ahi_rep_get32)(struct ddi_acc_impl *handle,
			uint32_t *host_addr, uint32_t *dev_addr,
			size_t repcount, uint_t flags);
	void	(*ahi_rep_get64)(struct ddi_acc_impl *handle,
			uint64_t *host_addr, uint64_t *dev_addr,
			size_t repcount, uint_t flags);

	void	(*ahi_rep_put8)(struct ddi_acc_impl *handle,
			uint8_t *host_addr, uint8_t *dev_addr,
			size_t repcount, uint_t flags);
	void	(*ahi_rep_put16)(struct ddi_acc_impl *handle,
			uint16_t *host_addr, uint16_t *dev_addr,
			size_t repcount, uint_t flags);
	void	(*ahi_rep_put32)(struct ddi_acc_impl *handle,
			uint32_t *host_addr, uint32_t *dev_addr,
			size_t repcount, uint_t flags);
	void	(*ahi_rep_put64)(struct ddi_acc_impl *handle,
			uint64_t *host_addr, uint64_t *dev_addr,
			size_t repcount, uint_t flags);

	int	(*ahi_fault_check)(struct ddi_acc_impl *handle);
	void	(*ahi_fault_notify)(struct ddi_acc_impl *handle);
	uint32_t	ahi_fault;
	ndi_err_t *ahi_err;
} ddi_acc_impl_t;

#endif

#ifdef __cplusplus
}
#endif

#endif /* _SYS_DDI_ISA_H */
