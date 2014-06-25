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

/*
 * NOTE: This is a quicky implementation of copy.S in C.  The error
 * checking, optimization and in fact functional correctness may be
 * lacking.
 *
 * I REPEAT: THERE IS MUCH BROKENNESS HERE.
 * You have been warned.
 */

#include <sys/param.h>
#include <sys/machparam.h>
#include <sys/systm.h>

/*
 * Zero out a region that is suitably aligned for operating
 * on 32-bit words at a time.
 * Restrictions:
 *   addr must be 4-byte aligned.
 * Note:
 *   wcnt is a word count, not a byte count.
 */

void
bzero32(uint32_t *addr, size_t wcnt)
{
	while (wcnt != 0) {
		*addr++ = 0;
		--wcnt;
	}
}

/*
 * Zero out a "count" number of bytes at address "addr".
 */
void
bzero(void *addr, size_t count)
{
	char *buf = (char *)addr;
	size_t wcnt, cnt;

	cnt = (uintptr_t)buf & 0x3;
	while (cnt != 0) {
		*buf++ = 0;
		--cnt;
	}
	if (cnt >= 4) {
		size_t wcnt;

		wcnt = cnt / 4;
		bzero32((uint32_t *)buf, wcnt);
		cnt -= wcnt * 4;
	}
	while (cnt != 0) {
		*buf++ = 0;
		--cnt;
	}
}

/* Same as bzero(), but make sure we've got a userspace address. */
void
uzero(void *addr, size_t count)
{
	if ((unsigned long) addr > KERNELBASE)
		panic("uzero: argument is not in user space");

	bzero(addr, count);
}

/*
 * Same as bzero(), but returning an error code if we take a kernel
 * pagefault which cannot be resolved.
 * Returns errno value on pagefault error, 0 if all ok.
 */
int
kzero(void *addr, size_t count)
{
	bzero(addr, count);

	/* FIXME: detect pagefault errors */
	return (0);
}

/*
 * Copy between regions that is suitably aligned for operating
 * on 32-bit words at a time.
 * Restrictions:
 *   'from' must be 4-byte aligned.
 *   'to' must be 4-byte aligned.
 * Note:
 *   wcnt is a word count, not a byte count.
 */
void
bcopy32(const uint32_t *from, uint32_t *to, size_t wcnt)
{
	const uint_t *myfrom;
	uint_t *myto;

	myfrom = from;
	myto = to;
	while (wcnt >= 4) {
		myto[0] = myfrom[0];
		myto[1] = myfrom[1];
		myto[2] = myfrom[2];
		myto[3] = myfrom[3];
		wcnt -= 4;
	}

	while (wcnt != 0) {
		*myto++ = *myfrom++;
		--wcnt;
	}
}

/* Copy a block of storage - allowed to overlap. */
void
ovbcopy(const void *from, void *to, size_t count)
{
	const char *myfrom = (const char *)from;
	char *myto = (char *)to;
	size_t phase_from, phase_to, cnt, wcnt;

	/* FIXME: Handle overlaps. */

	/*
	 * If both 'from' and 'to' are the same phase (4-byte granularity),
	 * then take care of any possible unaligned bytes up front, then
	 * copy 32-bit words at a time.
	 */
	phase_from = (uintptr_t)myfrom & 3;
	phase_to = (uintptr_t)myto & 3;
	if (phase_to == phase_from) {
		cnt = phase_from;
		while (cnt != 0) {
			*myto++ = *myfrom++;
			--cnt;
		}
		cnt = count - phase_from;
		if (cnt >= 4) {
			wcnt = cnt / 4;
			bcopy32((uint32_t *)myfrom, (uint32_t *)myto, wcnt);
			cnt -= wcnt * 4;
		}
	} else {
		cnt = count;
	}

	/*
	 * Now, take care of any possible remaining bytes.
	 * When 'from' and 'to' have different 4-byte phase,
	 * everything is done one byte at a time.
	 */
	while (cnt != 0) {
		*myto++ = *myfrom++;
		--cnt;
	}
}

/* Copy a block of storage - must not overlap (from + len <= to). */
void
bcopy(const void *from, void *to, size_t count)
{
	/* FIXME: Handle overlaps. */
	ovbcopy(from, to, count);
}

/* Copy a block of storage in userspace. */
void
ucopy(const void *ufrom, void *uto, size_t ulength)
{
	if (((unsigned long) ufrom > KERNELBASE) ||
				((unsigned long) uto > KERNELBASE))
		panic("ucopy: argument is not in user space");

	ovbcopy(ufrom, uto, ulength);
}

/*
 * Copy a block of storage, returning an error code if `from' or
 * `to' takes a kernel pagefault which cannot be resolved.
 * Returns errno value on pagefault error, 0 if all ok.
 */
int
kcopy(const void *from, void *to, size_t count)
{
	/* FIXME: detect pagefaults. */
	ovbcopy(from, to, count);

	return (0);
}

/*
 * Copy a block of storage.
 * Similar to kcopy but uses non-temporal instructions.
 */
int
kcopy_nta(const void *from, void *to, size_t count, int copy_cached)
{
	/* FIXME: use copy_cached */
	return (kcopy(from, to, count));
}

int
copyin(const void *uaddr, void *kaddr, size_t count)
{
	if ((unsigned long) uaddr > KERNELBASE)
		panic("copyin: argument is not in user space");

	/* FIXME: detect errors. */
	ovbcopy(uaddr, kaddr, count);
	return (0);
}

void
copyin_noerr(const void *ufrom, void *kto, size_t count)
{
	copyin(ufrom, kto, count);
}

int
xcopyin(const void *uaddr, void *kaddr, size_t count)
{
	/* FIXME: detect errors. */
	return (copyin(uaddr, kaddr, count));
}

int
xcopyin_nta(const void *uaddr, void *kaddr, size_t count, int copy_cached)
{
	/* FIXME: use copy_cached */
	return (xcopyin(uaddr, kaddr, count));
}

int
copyout(const void *kaddr, void *uaddr, size_t count)
{
	if ((unsigned long) uaddr > KERNELBASE)
		panic("copyout: argument is not in user space");

	/* FIXME: detect errors. */
	ovbcopy(kaddr, uaddr, count);

	return (0);
}

void
copyout_noerr(const void *kfrom, void *uto, size_t count)
{
	copyout(kfrom, uto, count);
}

int
xcopyout(const void *kaddr, void *uaddr, size_t count)
{
	/* FIXME: detect errors. */
	return (copyout(kaddr, uaddr, count));
}

int
xcopyout_nta(const void *kaddr, void *uaddr, size_t count, int copy_cached)
{
	/* FIXME: use copy_cached */
	return (xcopyout(kaddr, uaddr, count));
}

int
copystr(const char *from, char *to, size_t maxlength, size_t *lencopied)
{
	int i;
	char *myfrom = (char *)from;
	char *myto = to;

	for (i = 0; i < maxlength; ++i) {
		if (*myfrom == '\0')
			break;
		*myto++ = *myfrom++;
	}

	*lencopied = i;

	return (i);
}

int
copyinstr(const char *uaddr, char *kaddr, size_t maxlength, size_t *lencopied)
{
	if ((unsigned long) uaddr > KERNELBASE)
		panic("copyinstr: argument is not in user space");

	return (copystr(uaddr, kaddr, maxlength, lencopied));
}

int
copyoutstr(const char *kaddr, char *uaddr, size_t maxlength, size_t *lencopied)
{
	if ((unsigned long) uaddr > KERNELBASE)
		panic("copyoutstr: argument is not in user space");

	return (copystr(kaddr, uaddr, maxlength, lencopied));
}

#define	FUWORD(name, type)						\
	int name(const void *addr, type *dst)				\
	{								\
		*dst = (type) *(type *)addr;				\
		return (0);						\
	}								\
									\
	void name##_noerr(const void *addr, type *dst)			\
	{								\
		name(addr, dst);					\
	}

FUWORD(fuword32, uint32_t)
FUWORD(fuword16, uint16_t)
FUWORD(fuword8, uint8_t)
FUWORD(fulword, ulong_t)

#define	SUWORD(name, type)						\
	int name(void *addr, type value)				\
	{								\
		*(type *) addr = value;					\
		return (0);						\
	}								\
									\
	void name##_noerr(void *addr, type value)			\
	{								\
		name(addr, value);					\
	}

SUWORD(suword32, uint32_t)
SUWORD(suword16, uint16_t)
SUWORD(suword8, uint8_t)
SUWORD(sulword, ulong_t)
SUWORD(subyte, uchar_t)

void
hwblkpagecopy(caddr_t src_vaddr, caddr_t dst_vaddr)
{
	bcopy32((uint32_t *)src_vaddr, (uint32_t *)dst_vaddr, MMU_PAGESIZE);
}

void
hwblkpagezero(caddr_t vaddr, size_t len)
{
	bzero32((uint32_t *)vaddr, len / 4);
}

void
hwblkclr(caddr_t vaddr, size_t len)
{
	bzero(vaddr, len);
}
