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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/memlist.h>
#include <sys/pte.h>
#include <sys/bootconf.h>

/*
 * Macro to align address to the maximum alignment requirement on PPC.
 */
#define	MAXALIGN(x)	(((uint_t)(x) + _MAX_ALIGNMENT) & ~(_MAX_ALIGNMENT -1))

/*
 * Count the number of available pages and the number of
 * chunks in the list of available memory.
 */
void
size_physavail(struct memlist *physavail, uint_t *npages, int *memblocks)
{
	*npages = 0;
	*memblocks = 0;

	for (; physavail; physavail = physavail->next) {
		*npages += (uint_t)(physavail->size >> MMU_STD_PAGESHIFT);
		(*memblocks)++;
	}
}

/*
 * Returns the max contiguous physical memory present in the
 * memlist "physavail".
 */
uint32_t
get_max_phys_size(
	struct memlist	*physavail)
{
	uint32_t	max_size = 0;

	for (; physavail; physavail = physavail->next) {
		if (physavail->size > max_size)
			max_size = physavail->size;
		}

		return (max_size);
}


/*
 * Copy boot's physavail list deducting memory at "start"
 * for "size" bytes.
 */
int
copy_physavail(
	struct memlist	*src,
	struct memlist	**dstp,
	uint_t		start,
	uint_t		size)
{
	struct memlist *dst, *prev;
	uint_t end1;
	int deducted = 0;

	dst = *dstp;
	prev = dst;
	end1 = start + size;

	for (; src; src = src->next) {
		u_longlong_t addr, lsize, end2;

		addr = src->address;
		lsize = src->size;
		end2 = addr + lsize;

		if (start >= addr && end1 <= end2) {
			/* deducted range in this chunk */
			deducted = 1;
			if (start == addr) {
				/* abuts start of chunk */
				if (end1 == end2)
					/* is equal to the chunk */
					continue;
				dst->address = end1;
				dst->size = lsize - size;
			} else if (end1 == end2) {
				/* abuts end of chunk */
				dst->address = addr;
				dst->size = lsize - size;
			} else {
				/* in the middle of the chunk */
				dst->address = addr;
				dst->size = start - addr;
				dst->next = 0;
				if (prev == dst) {
					dst->prev = 0;
					dst++;
				} else {
					dst->prev = prev;
					prev->next = dst;
					dst++;
					prev++;
				}
				dst->address = end1;
				dst->size = end2 - end1;
			}
			dst->next = 0;
			if (prev == dst) {
				dst->prev = 0;
				dst++;
			} else {
				dst->prev = prev;
				prev->next = dst;
				dst++;
				prev++;
			}
		} else {
			dst->address = src->address;
			dst->size = src->size;
			dst->next = 0;
			if (prev == dst) {
				dst->prev = 0;
				dst++;
			} else {
				dst->prev = prev;
				prev->next = dst;
				dst++;
				prev++;
			}
		}
	}

	*dstp = dst;
	return (deducted);
}

/*
 * Find the page number of the highest installed physical
 * page and the number of pages installed (one cannot be
 * calculated from the other because memory isn't necessarily
 * contiguous).
 */
void
installed_top_size_memlist_array(
	u_longlong_t *list,	/* base of array */
	size_t	nelems,		/* number of elements */
	pfn_t *topp,		/* return ptr for top value */
	pgcnt_t *sumpagesp)	/* return prt for sum of installed pages */
{
	pfn_t top = 0;
	pgcnt_t sumpages = 0;
	pfn_t highp;		/* high page in a chunk */
	size_t i;

	for (i = 0; i < nelems; i += 2) {
		highp = (list[i] + list[i+1] - 1) >> PAGESHIFT;
		if (top < highp)
			top = highp;
		sumpages += (list[i+1] >> PAGESHIFT);
	}

	*topp = top;
	*sumpagesp = sumpages;
}

/*
 * Process the physical installed list for boot.
 * Finds:
 * 1) the pfn of the highest installed physical page,
 * 2) the number of pages installed
 * 3) the number of distinct contiguous regions these pages fall into.
 */
void
installed_top_size(
	struct memlist *list,	/* pointer to start of installed list */
	pfn_t *high_pfn,	/* return ptr for top value */
	pgcnt_t *pgcnt,		/* return ptr for sum of installed pages */
	uint_t	*ranges)	/* return ptr for the count of contig. ranges */
{
	pfn_t top = 0;
	pgcnt_t sumpages = 0;
	pfn_t highp;		/* high page in a chunk */
	uint_t cnt = 0;

	for (; list; list = list->next) {
		++cnt;
		highp = (list->address + list->size - 1) >> PAGESHIFT;
		if (top < highp)
			top = highp;
		sumpages += btop(list->size);
	}

	*high_pfn = top;
	*pgcnt = sumpages;
	*ranges = cnt;
}

#if defined(XXX_OBSOLETE_SOLARIS)

/*
 * Find the page number of the highest installed physical
 * page and the number of pages installed (one cannot be
 * calculated from the other because memory isn't necessarily
 * contiguous).
 */

void
installed_top_size(
	struct memlist *list,	/* pointer to start of installed list */
	int *topp,		/* return ptr for top value */
	int *sumpagesp)		/* return prt for sum of installed pages */
{
	int top = 0;
	int sumpages = 0;
	int highp;		/* high page in a chunk */

	for (; list; list = list->next) {
		highp = (list->address + list->size - 1) >> MMU_STD_PAGESHIFT;
		if (top < highp)
			top = highp;
		sumpages += (uint_t)(list->size >> MMU_STD_PAGESHIFT);
	}

	*topp = top;
	*sumpagesp = sumpages;
}

#endif /* defined(XXX_OBSOLETE_SOLARIS) */

/*
 * Copy a memory list.  Used in startup() to copy boot's
 * memory lists to the kernel.
 */
void
copy_memlist(
	struct memlist *src,
	struct memlist **dstp)
{
	struct memlist *dst, *prev;

	dst = *dstp;
	prev = dst;

	for (; src; src = src->next) {
		dst->address = src->address;
		dst->size = src->size;
		dst->next = 0;
		if (prev == dst) {
			dst->prev = 0;
			dst++;
		} else {
			dst->prev = prev;
			prev->next = dst;
			dst++;
			prev++;
		}
	}

	*dstp = dst;
}

static struct bootmem_props {
	char		*name;
	u_longlong_t	*ptr;
	size_t		nelems;		/* actual number of elements */
	size_t		bufsize;	/* length of allocated buffer */
} bootmem_props[] = {
	{ "phys-installed", NULL, 0, 0 },
	{ "phys-avail", NULL, 0, 0 },
	{ "virt-avail", NULL, 0, 0 },
	{ NULL, NULL, 0, 0 }
};

#define	PHYSINSTALLED	0
#define	PHYSAVAIL	1
#define	VIRTAVAIL	2

/*
 * Copy the memlist from boot to kernel, each list is an {addr, size} array
 * with addr and size of 64-bit (u_longlong_t). The returned length for each
 * array is 2*the number of array elements.
 *
 */
void
copy_boot_memlists(u_longlong_t **physinstalled, size_t *physinstalled_len,
    u_longlong_t **physavail, size_t *physavail_len,
    u_longlong_t **virtavail, size_t *virtavail_len)
{
	int	align = BO_ALIGN_L3;
	size_t	len;
	struct bootmem_props *tmp = bootmem_props;

tryagain:
	for (tmp = bootmem_props; tmp->name != NULL; tmp++) {
		len = BOP_GETPROPLEN(bootops, tmp->name);
		if (len == 0) {
			panic("cannot get length of \"%s\" property",
			    tmp->name);
		}
		tmp->nelems = len / sizeof (u_longlong_t);
		len = roundup(len, PAGESIZE);
		if (len <= tmp->bufsize)
			continue;
		/* need to allocate more */
		if (tmp->ptr) {
			BOP_FREE(bootops, (caddr_t)tmp->ptr, tmp->bufsize);
			tmp->ptr = NULL;
			tmp->bufsize = 0;
		}
		tmp->bufsize = len;
		tmp->ptr = (void *)BOP_ALLOC(bootops, 0, tmp->bufsize, align);
		if (tmp->ptr == NULL)
			panic("cannot allocate %lu bytes for \"%s\" property",
			    tmp->bufsize, tmp->name);

	}
	/*
	 * take the most current snapshot we can by calling mem-update
	 */
	if (BOP_GETPROPLEN(bootops, "memory-update") == 0)
		(void) BOP_GETPROP(bootops, "memory-update", NULL);

	/* did the sizes change? */
	for (tmp = bootmem_props; tmp->name != NULL; tmp++) {
		len = BOP_GETPROPLEN(bootops, tmp->name);
		tmp->nelems = len / sizeof (u_longlong_t);
		len = roundup(len, PAGESIZE);
		if (len > tmp->bufsize) {
			/* ick. Free them all and try again */
			for (tmp = bootmem_props; tmp->name != NULL; tmp++) {
				BOP_FREE(bootops, (caddr_t)tmp->ptr,
				    tmp->bufsize);
				tmp->ptr = NULL;
				tmp->bufsize = 0;
			}
			goto tryagain;
		}
	}

	/* now we can retrieve the properties */
	for (tmp = bootmem_props; tmp->name != NULL; tmp++) {
		if (BOP_GETPROP(bootops, tmp->name, tmp->ptr) == -1) {
			panic("cannot retrieve \"%s\" property",
			    tmp->name);
		}
	}
	*physinstalled = bootmem_props[PHYSINSTALLED].ptr;
	*physinstalled_len = bootmem_props[PHYSINSTALLED].nelems;

	*physavail = bootmem_props[PHYSAVAIL].ptr;
	*physavail_len = bootmem_props[PHYSAVAIL].nelems;

	*virtavail = bootmem_props[VIRTAVAIL].ptr;
	*virtavail_len = bootmem_props[VIRTAVAIL].nelems;
}
