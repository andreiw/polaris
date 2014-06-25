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
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)uname-i.c	1.10	96/10/15 SMI"

#include <sys/param.h>
#include <sys/promif.h>

#include <sys/platnames.h>

#include <sys/salib.h>

#define	MAXNMLEN	1024	/* # of chars in an impl-arch name */

/*
 * Return the implementation architecture name (uname -i) for this platform.
 *
 * Use the named rootnode property to determine the iarch.
 */
static char *
get_impl_arch_name(char *namename, int *namelen)
{
	static char iarch[MAXNMLEN];
	pnode_t n;
	int len;
	char *cp;

	/*
	 * lookup the named property in the root node
	 */
	n = (pnode_t)prom_rootnode();
	len = prom_getproplen(n, namename);
	if (len <= 0 || len >= MAXNMLEN)
		return (NULL);
	(void) prom_getprop(n, namename, iarch);
	iarch[len] = '\0';	/* broken clones don't terminate the name */

	/*
	 * Crush filesystem-awkward characters.  See PSARC/1992/170.
	 * (Convert the property to a sane directory name in UFS)
	 */
	for (cp = iarch; *cp; cp++)
		if (*cp == '/' || *cp == ' ' || *cp == '\t')
			*cp = '_';
	*namelen = len;
	return (iarch);
}

int
find_platform_dir(int (*isdirfn)(char *), char *iarch, int use_default)
{
	return 0;
}


static void
make_platform_path(char *fullpath, char *iarch, char *filename)
{
	(void) strcpy(fullpath, "/platform/");
	(void) strcat(fullpath, iarch);
	(void) strcat(fullpath, "/");
	(void) strcat(fullpath, filename);
}

/*
 * Given a filename, and a function to perform an 'open' on that file,
 * find the corresponding file in the /platform hierarchy, generating
 * the implementation architecture name on the fly.
 *
 * The routine will also set 'impl_arch_name' if non-null, and returns
 * the full pathname of the file opened.
 *
 * We allow the caller to specify the impl_arch_name.  We also allow
 * the caller to specify an absolute pathname, in which case we do
 * our best to generate an impl_arch_name.
 */
int
open_platform_file(
	char *filename,
	int (*openfn)(char *, void *),
	void *arg,
	char *fullpath,
	char *impl_arch_name)
{
	char *ia;
	int fd = -1;
	char	*next;
	int	len, namelen, s;

	/*
	 * If the caller -specifies- an absolute pathname, then we just
	 * open it after (optionally) determining the impl_arch_name.
	 *
	 * This is only here for booting non-kernel standalones (or pre-5.5
	 * kernels).  It's debateable that they would ever care what the
	 * impl_arch_name is.
	 */
	if (*filename == '/') {
		(void) strcpy(fullpath, filename);
		if (impl_arch_name) {
			ia = get_impl_arch_name("name", &namelen);
			if (ia != NULL)
				(void) strcpy(impl_arch_name, ia);
			else {
				ia = get_impl_arch_name("compatible", &namelen);
				if (ia != NULL)
					(void) strcpy(impl_arch_name, ia);
			}
			if (ia == NULL)
				(void) strcpy(impl_arch_name, "chrp");
		}
		return ((*openfn)(fullpath, arg));
	}

	/*
	 * If the caller -specifies- the impl_arch_name, then there's
	 * not much more to do than just open it.
	 *
	 * This is only here to support the '-I' flag to the boot program.
	 */
	if (impl_arch_name && *impl_arch_name != '\0') {
		make_platform_path(fullpath, impl_arch_name, filename);
		return ((*openfn)(fullpath, arg));
	}

	/*
	 * Otherwise, we must hunt the filesystem for one that works ..
	 */
	/*
	 * First try the root node's name.
	 */
	if ((ia = get_impl_arch_name("name", &namelen)) != NULL) {
		make_platform_path(fullpath, ia, filename);
		fd = (*openfn)(fullpath, arg);
		if (fd != -1) {
			if (impl_arch_name)
				(void) strcpy(impl_arch_name, ia);
			return (fd);
		}
	}

	/*
	 * Now, try the root node's compatible property.
	 */
	if ((ia = get_impl_arch_name("compatible", &namelen)) != NULL) {
		next = ia;
		len = 0;
		while (len < namelen) {
			make_platform_path(fullpath, next, filename);
			fd = (*openfn)(fullpath, arg);
			if (fd != -1) {
				if (impl_arch_name)
					(void) strcpy(impl_arch_name, next);
				return (fd);
			}
			/* skip the string and the null */
			s = strlen(next) + 1;
			len += s;
			next += s;
		}
	}

	/* default to chrp */
	make_platform_path(fullpath, "chrp", filename);
	fd = (*openfn)(fullpath, arg);
	if (impl_arch_name)
		(void) strcpy(impl_arch_name, "chrp");

	return (fd);
}

void
mod_path_uname_m(char *mod_path, char *impl_arch_name)
{
	if (strcmp(mod_path, "/platform/chrp/kernel") != 0) {
		strcat(mod_path, " /platform/chrp/kernel");
	}
}
