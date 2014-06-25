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
#include <sys/time.h>
#include <sys/machparam.h>
#include <sys/ppc_subr.h>
#include <sys/hid0.h>

extern hrtime_t get_time_base(void);

extern uint_t hid0;

/*
 * Before we get all fancy with the high-level stuff, let's start
 * with simple unit tests of the basic building blocks.  These are
 * not stress tests.  They are not sophisticated or thorough.
 * They are just an Army physical.  If these don't work, then there
 * is no sense in going further.
 */

static uint_t test_ppc_subr(void);
static uint_t test_getpc(void);
static uint_t test_getsp(void);

static uint_t test_timebase(void);

/*
 * Perform a series of unit tests.
 * Failure of a unit test does not cause testing to stop,
 * but failure is returned if any component test failed.
 */

uint_t
unit_test(void)
{
	uint_t err;
	uint_t tb_err;

	err = 0;
	err |= !test_ppc_subr();
	if ((hid0 & HID0_TBEN_FLD_MASK) != 0) {
		tb_err |= !test_timebase();
		if (tb_err) {
			(void) test_timebase();
			err |= tb_err;
		}
	}
	return (err == 0);
}

/*
 * Basic tests of the internal consistency of the functions in ppc_subr.h
 * and ppc_subr.c.  Tests of any functions with side effects are not done
 * here.  So, for example, no I/O functions are tested.
 */

static uint_t
test_ppc_subr(void)
{
	uint_t err;

	err = 0;
	err |= !test_getpc();
	return (err == 0);
}

/*
 * Test get_time_base().
 *
 * The time base should be monotonically increasing.
 */

static uint_t
test_timebase(void)
{
	hrtime_t t0, t1;
	uint_t i;
	uint_t err;

	err = 0;
	t0 = get_time_base();
	for (i = 0; i < 100; ++i) {
		t1 = get_time_base();
		if (t0 >= t1) {
			err = 1;
			break;
		}
		t0 = t1;
	}
	if (err == 0)
		return (1);
	if (i == 1 && t0 == 0 && t1 == 0) {
		prom_printf("Warning: timebase is not enabled.\n");
		return (1);
	}
	prom_printf("test_timebase failed: t0=0x%llx, t1=0x%llx\n", t0, t1);
	return (0);
}

static uint_t
test_getpc(void)
{
	uintptr_t pc;
	uint_t err;

	err = 0;
	pc = getpc();
	if (!(pc > (uintptr_t)test_getpc)) {
		prom_printf("test_getpc: getpc() not in current function.\n");
		err = 1;
	}
	return (err == 0);
}

/*
 * At the time of this test, the stack pointer and frame pointer
 * should both be in the kernel's t0stack.
 *
 * The stack grows down, so stack pointer should be < frame pointer.
 *
 */

static uint_t
test_getsp(void)
{
	extern uintptr_t t0stack[];
	uintptr_t s_t0stack;
	uintptr_t e_t0stack;
	uintptr_t fp;
	uintptr_t sp;
	uint_t err;

	err = 0;
	sp = (uintptr_t)getsp();
	fp = (uintptr_t)getfp();
	s_t0stack = (uintptr_t)t0stack;
	e_t0stack = s_t0stack + T0STKSZ;
	if (sp >= fp) {
		prom_printf("test_getsp: sp >= fp\n");
		err = 1;
	}
	if (!(sp >= s_t0stack && sp < e_t0stack)) {
		prom_printf("test_getsp: sp not in t0stack\n");
		err = 1;
	}
	if (!(fp >= s_t0stack && fp < e_t0stack)) {
		prom_printf("test_getsp: fp not in t0stack\n");
		err = 1;
	}
	return (err == 0);
}
