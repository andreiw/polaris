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
 *	Copyright (c) 1988 AT&T
 *	  All Rights Reserved
 *
 *
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 *
 * Global include file for all sgs.
 */

#ifndef	_SGS_H
#define	_SGS_H

#pragma ident	"@(#)sgs.h	1.53	06/05/09 SMI"


#ifdef	__cplusplus
extern "C" {
#endif

/* <assert.h> keys off of NDEBUG */
#ifdef	DEBUG
#undef	NDEBUG
#else
#define	NDEBUG
#endif

#ifndef	_ASM
#include <sys/types.h>
#include <sys/machelf.h>
#include <stdlib.h>
#include <libelf.h>
#include <assert.h>
#include <alist.h>
#endif	/* _ASM */

/*
 * Software identification.
 */
#define	SGS		""
#define	SGU_PKG		"Software Generation Utilities"
#define	SGU_REL		"(SGU) Solaris-ELF (4.0)"


#ifndef _ASM

extern const char link_ver_string[];	/* Linker version id  */
					/*   see libconv/{plat}/vernote.s */
/*
 * Macro to round to next double word boundary.
 */
#define	S_DROUND(x)	(((x) + sizeof (double) - 1) & ~(sizeof (double) - 1))

/*
 * General align and round macros.
 */
#define	S_ALIGN(x, a)	((x) & ~(((a) ? (a) : 1) - 1))
#define	S_ROUND(x, a)   ((x) + (((a) ? (a) : 1) - 1) & ~(((a) ? (a) : 1) - 1))

/*
 * Bit manipulation macros; generic bit mask and is `v' in the range
 * supportable in `n' bits?
 */
#define	S_MASK(n)	((1 << (n)) -1)
#define	S_INRANGE(v, n)	(((-(1 << (n)) - 1) < (v)) && ((v) < (1 << (n))))


/*
 * Yet another definition of the OFFSETOF macro, used with the AVL routines.
 */
#define	SGSOFFSETOF(s, m)	((size_t)(&(((s *)0)->m)))

/*
 * When casting between integer and pointer types, gcc will complain
 * if the integer type used is not large enough to hold the pointer
 * value without loss. Although a dubious practice in general, this
 * is sometimes done by design. In those cases, the general solution
 * is to introduce an intermediate cast to widen the integer value. The
 * CAST_PTRINT macro does this, and its use documents the fact that
 * the programmer is doing that sort of cast.
 */
#ifdef __GNUC__
#define	CAST_PTRINT(cast, value) ((cast)(uintptr_t)value)
#else
#define	CAST_PTRINT(cast, value) ((cast)value)
#endif

/*
 * General typedefs.
 */
typedef enum {
	FALSE = 0,
	TRUE = 1
} Boolean;

/*
 * Types of errors (used by eprintf()), together with a generic error return
 * value.
 */
typedef enum {
	ERR_NONE,
	ERR_WARNING,
	ERR_FATAL,
	ERR_ELF,
	ERR_NUM				/* Must be last */
} Error;

#if defined(_LP64) && !defined(_ELF64)
#define	S_ERROR		(~(uint_t)0)
#else
#define	S_ERROR		(~(uintptr_t)0)
#endif

/*
 * LIST_TRAVERSE() is used as the only "argument" of a "for" loop to
 * traverse a linked list. The node pointer `node' is set to each node in
 * turn and the corresponding data pointer is copied to `data'.  The macro
 * is used as in
 * 	for (LIST_TRAVERSE(List *list, Listnode *node, void *data)) {
 *		process(data);
 *	}
 */
#define	LIST_TRAVERSE(L, N, D) \
	(void) (((N) = (L)->head) != NULL && ((D) = (N)->data) != NULL); \
	(N) != NULL; \
	(void) (((N) = (N)->next) != NULL && ((D) = (N)->data) != NULL)

typedef	struct listnode	Listnode;
typedef	struct list	List;

struct	listnode {			/* a node on a linked list */
	void		*data;		/* the data item */
	Listnode	*next;		/* the next element */
};

struct	list {				/* a linked list */
	Listnode	*head;		/* the first element */
	Listnode	*tail;		/* the last element */
};


#ifdef _SYSCALL32
typedef	struct listnode32	Listnode32;
typedef	struct list32		List32;

struct	listnode32 {			/* a node on a linked list */
	Elf32_Addr	data;		/* the data item */
	Elf32_Addr	next;		/* the next element */
};

struct	list32 {			/* a linked list */
	Elf32_Addr	head;		/* the first element */
	Elf32_Addr	tail;		/* the last element */
};
#endif	/* _SYSCALL32 */


/*
 * Structure to maintain rejected files elf information.  Files that are not
 * applicable to the present link-edit are rejected and a search for an
 * appropriate file may be resumed.  The first rejected files information is
 * retained so that a better error diagnostic can be given should an appropriate
 * file not be located.
 */
typedef struct {
	ushort_t	rej_type;	/* SGS_REJ_ value */
	ushort_t	rej_flag;	/* additional information */
	uint_t		rej_info;	/* numeric and string information */
	const char	*rej_str;	/*	associated with error */
	const char	*rej_name;	/* object name - expanded library */
					/*	name and archive members */
} Rej_desc;

#define	SGS_REJ_NONE		0
#define	SGS_REJ_MACH		1	/* wrong ELF machine type */
#define	SGS_REJ_CLASS		2	/* wrong ELF class (32-bit/64-bit) */
#define	SGS_REJ_DATA		3	/* wrong ELF data format (MSG/LSB) */
#define	SGS_REJ_TYPE		4	/* bad ELF type */
#define	SGS_REJ_BADFLAG		5	/* bad ELF flags value */
#define	SGS_REJ_MISFLAG		6	/* mismatched ELF flags value */
#define	SGS_REJ_VERSION		7	/* mismatched ELF/lib version */
#define	SGS_REJ_HAL		8	/* HAL R1 extensions required */
#define	SGS_REJ_US3		9	/* Sun UltraSPARC III extensions */
					/*	required */
#define	SGS_REJ_STR		10	/* generic error - info is a string */
#define	SGS_REJ_UNKFILE		11	/* unknown file type */
#define	SGS_REJ_HWCAP_1		12	/* hardware capabilities mismatch */

/*
 * For those source files used both inside and outside of the
 * libld source base (tools/common/string_table.c) we can
 * automatically switch between the allocation models
 * based off of the 'cc -DUSE_LIBLD_MALLOC' flag.
 */
#ifdef	USE_LIBLD_MALLOC
#define	calloc(x, a)		libld_malloc(((size_t)x) * ((size_t)a))
#define	free			libld_free
#define	malloc			libld_malloc
#define	realloc			libld_realloc

#define	libld_calloc(x, a)	libld_malloc(((size_t)x) * ((size_t)a))
extern void		libld_free(void *);
extern void		*libld_malloc(size_t);
extern void		*libld_realloc(void *, size_t);
#endif


/*
 * Data structures (defined in libld.h).
 */
typedef struct ent_desc		Ent_desc;
typedef	struct group_desc	Group_desc;
typedef struct ifl_desc		Ifl_desc;
typedef struct is_desc		Is_desc;
typedef struct isa_desc		Isa_desc;
typedef struct isa_opt		Isa_opt;
typedef struct mv_desc		Mv_desc;
typedef struct ofl_desc		Ofl_desc;
typedef struct os_desc		Os_desc;
typedef	struct rel_cache	Rel_cache;
typedef	struct sdf_desc		Sdf_desc;
typedef	struct sdv_desc		Sdv_desc;
typedef struct sg_desc		Sg_desc;
typedef struct sort_desc	Sort_desc;
typedef struct sec_order	Sec_order;
typedef struct sym_desc		Sym_desc;
typedef struct sym_aux		Sym_aux;
typedef	struct sym_avlnode	Sym_avlnode;
typedef	struct uts_desc		Uts_desc;
typedef struct ver_desc		Ver_desc;
typedef struct ver_index	Ver_index;
typedef	struct audit_desc	Audit_desc;
typedef	struct audit_info	Audit_info;
typedef	struct audit_list	Audit_list;

/*
 * Data structures defined in machrel.h.
 */
typedef struct rel_desc		Rel_desc;

/*
 * Data structures defined in rtld.h.
 */
typedef struct lm_list		Lm_list;
#ifdef _SYSCALL32
typedef struct lm_list32	Lm_list32;
#endif	/* _SYSCALL32 */

/*
 * For the various utilities that include sgs.h
 */
extern int	assfail(const char *, const char *, int);
extern void	eprintf(Lm_list *, Error, const char *, ...);
extern char	*sgs_demangle(char *);
extern uint_t	sgs_str_hash(const char *);
extern uint_t	findprime(uint_t);

#endif /* _ASM */

#ifdef	__cplusplus
}
#endif


#endif /* _SGS_H */
