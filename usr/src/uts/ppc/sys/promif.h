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
/*
 * Copyright 2006 Sun Microsystems Laboratories.  All rights reserved.
 * Use is subject to license terms.
 */
/*
 * XXX PPC
 * It seems that uts/intel/sys/promif.h is a copy of an early
 * uts/sun/sys/promif.h So I am just copying the intel version
 * to uts/ppc/sys/promif.h in hope that it will be OK
 */

#ifndef	_SYS_PROMIF_H
#define	_SYS_PROMIF_H

#pragma ident	"@(#)promif.h	1.33	05/10/30 SMI"

#include <sys/types.h>
#include <sys/obpdefs.h>

#if defined(_KERNEL) || defined(_KMDB)
#include <sys/va_list.h>
#endif

#ifdef	__cplusplus
extern "C" {
#endif

  /* XXX - had to add for missing define, should be in an include look for this later */
#ifndef NULL
#define NULL 0
#endif

/*
 *  These are for V0 ops only.  We sometimes have to specify
 *  to promif which type of operation we need to perform
 *  and since we can't get such a property from a V0 prom, we
 *  sometimes just assume it.  V2 and later proms do the right thing.
 */
#define	BLOCK	0
#define	NETWORK	1
#define	BYTE	2

#if defined(_KERNEL) || defined(_KMDB)

/*

extern	caddr_t		prom_map(caddr_t virthint, uint_t space,
			    uint_t phys, uint_t size);
*/

typedef void promif_preprom_f(void);
typedef void promif_postprom_f(void);

/*
 * resource allocation group: OBP and IEEE 1275-1994.
 * prom_alloc is platform dependent on SPARC.
 */
extern	caddr_t		prom_alloc(caddr_t virthint, size_t size, u_int align);
extern	void		prom_free(caddr_t virt, size_t size);

/*
 * Device tree and property group: OBP and IEEE 1275-1994.
 */
extern	pnode_t		prom_childnode(pnode_t nodeid);
extern	pnode_t		prom_nextnode(pnode_t nodeid);
extern	pnode_t		prom_optionsnode(void);
extern	pnode_t		prom_alias_node(void);
extern	pnode_t		prom_rootnode(void);

extern	int		prom_getproplen(pnode_t nodeid, caddr_t name);
extern	int		prom_getprop(pnode_t nodeid, caddr_t name,
			    caddr_t value);
extern	caddr_t		prom_nextprop(pnode_t nodeid, caddr_t previous,
			    caddr_t next);

extern	char		*prom_decode_composite_string(void *buf,
			    size_t buflen, char *prev);

/*
 * Device tree and property group: IEEE 1275-1994 Only.
 */
extern	pnode_t		prom_finddevice(char *path);

extern	int		prom_bounded_getprop(pnode_t nodeid,
			    caddr_t name, caddr_t buffer, int buflen);

/*
 * Device pathnames and pathname conversion: OBP and IEEE 1275-1994.
 */
extern	int		prom_devname_from_pathname(char *path, char *buffer);
extern	char		*prom_path_gettoken(char *from, char *to);

/*
 * Device pathnames and pathname conversion: IEEE 1275-1994 only.
 */

/*
 * Special device nodes: OBP and IEEE 1275-1994.
 */
extern	int		prom_stdin_is_keyboard(void);
extern	int		prom_stdout_is_framebuffer(void);
extern	void		prom_framebuffer_getpos(int *row, int *col);
extern	void		prom_framebuffer_getcolors(int *fg, int *bg);
extern  char    	*prom_stdinpath(void);
extern  char    	*prom_stdoutpath(void);
extern  void    	prom_strip_options(char *from, char *to);
extern  void    	prom_pathname(char *);

/*
 * Special device nodes: IEEE 1275-1994 only.
 */

/*
 * Administrative group: OBP and IEEE 1275-1994.
 */
extern	void		prom_enter_mon(void);
extern	void		prom_exit_to_mon(void)
	__NORETURN;
extern	void		prom_reboot(char *bootstr)
	__NORETURN;
extern	void		prom_panic(char *string)
	__NORETURN;

extern	int		prom_is_openprom(void);
extern	int		prom_is_p1275(void);
extern	int		prom_version_name(char *buf, int buflen);
extern	int		prom_version_boot_syscalls(void);

extern	uint_t		prom_gettime(void);

extern	char		*prom_bootpath(void);
extern	char		*prom_bootargs(void);

/*
 * Administrative group: OBP only.
 */

/*
 * Administrative group: IEEE 1275-1994 only.
 */

/*
 * Administrative group: IEEE 1275 only.
 */

/*
 * Promif support group: Generic.
 */
extern	void		prom_init(char *progname, void *prom_cookie);

extern	void		prom_set_preprom(promif_preprom_f *);
extern	void		prom_set_postprom(promif_postprom_f *);

extern  void		prom_get_tem_pos(uint32_t *, uint32_t *);
extern	void		prom_get_tem_size(size_t *, size_t *);

typedef struct          __promif_redir_arg *promif_redir_arg_t;
typedef ssize_t		(*promif_redir_t)(promif_redir_arg_t,
				uchar_t *, size_t);
extern  void		prom_set_stdout_redirect(promif_redir_t,
				promif_redir_arg_t);

extern	void		prom_suspend_prepost(void);
extern	void		prom_resume_prepost(void);

extern	void		(*prom_set_nextprop_preprom(void (*)(void)))(void);
extern	void		(*prom_set_nextprop_postprom(void (*)(void)))(void);

extern	void		prom_montrap(void (*funcptr)());

typedef uint_t		prom_generation_cookie_t;

extern	int		prom_tree_access(int (*callback)(void *arg,
				int has_changed), void *arg,
				prom_generation_cookie_t *);
extern	int		prom_tree_update(int (*callback)(void *arg), void *arg);


/*
 * I/O Group: OBP and IEEE 1275.
 */
extern	uchar_t		prom_getchar(void);
extern	void		prom_putchar(char c);
extern	int		prom_mayget(void);
extern	int		prom_mayput(char c);

extern  int		prom_open(char *name);
extern  int		prom_close(int fd);
extern  ssize_t		prom_read(ihandle_t fd, caddr_t buf, size_t len,
			    uint_t startblk, char devtype);
extern	ssize_t		prom_write(ihandle_t fd, caddr_t buf, size_t len,
			    uint_t startblk, char devtype);
extern	int		prom_seek(int fd, unsigned long long offset);

extern	void		prom_writestr(const char *buf, size_t bufsize);

/*PRINTFLIKE1*/
extern	void		prom_printf(const char *fmt, ...)
	__PRINTFLIKE(1);
#pragma	rarely_called(prom_printf)
extern	void		prom_vprintf(const char *fmt, __va_list adx)
	__VPRINTFLIKE(1);
#pragma	rarely_called(prom_vprintf)

/*PRINTFLIKE2*/
extern	char		*prom_sprintf(char *s, const char *fmt, ...)
	__PRINTFLIKE(2);
extern	char		*prom_vsprintf(char *s, const char *fmt, __va_list adx)
	__VPRINTFLIKE(2);

#define PROM_WALK_CONTINUE      0       /* keep walking to next node */
#define PROM_WALK_TERMINATE     1       /* abort walk now */


/*
 * promif tree searching routines ... OBP and IEEE 1275-1994.
 */

extern	pnode_t		prom_findnode_byname(pnode_t id, char *name);
extern	char		*prom_get_extend_name(void);

extern	int		prom_devreset(int);
extern	int		OpenCount;
extern struct ihandle	*open_devices[];

#define	PROM_STOP	{	\
	prom_printf("File %s line %d\n", __FILE__, __LINE__); \
	prom_enter_mon();	\
}

#endif /* _KERNEL || _KMDB */

#ifdef _KERNEL

/*
 * Used by wrappers which bring up console frame buffer before prom_printf()
 * and other prom calls that may output to the console.  Struct is filled in
 * in prom_env.c and in sunpm.c
 */

typedef struct promif_owrap {
	void (*preout)(void);
	void (*postout)(void);
} promif_owrap_t;

/*
 * WAN boot key storage interface
 */
int prom_set_security_key(char *keyname, caddr_t buf, int buflen, int *reslen,
    int *status);
int prom_get_security_key(char *keyname, caddr_t buf, int buflen, int *keylen,
    int *status);

#endif	/* _KERNEL */
#ifdef	__cplusplus
}
#endif

#endif /* _SYS_PROMIF_H */
