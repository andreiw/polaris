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

#pragma ident	"@(#)metaconf.c	1.10	05/06/08 SMI"


#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/mkdev.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <limits.h>
#include <string.h>
#include <libsvm.h>
#include <svm.h>
#include <errno.h>


#define	VERSION "1.0"
#define	DISK_DIR "/dev/rdsk"

extern int _map_to_effective_dev();

int
is_blankline(char *buf)
{
	for (; *buf != 0; buf++) {
		if (!isspace(*buf))
			return (0);
	}
	return (1);
}

/*
 * FUNCTION: write_targ_nm_table
 *	creates a tuple table of <driver name, major number > in md.conf
 * INPUT: rootpath
 *
 * RETURN VALUES:
 *	RET_SUCCESS
 *	RET_ERROR
 */

int
write_targ_nm_table(char *path)
{
	FILE	*targfp = NULL;
	FILE	*mdfp = NULL;
	char	buf[PATH_MAX], *cp;
	int	retval = RET_SUCCESS;
	int	first_entry = 1;

	if ((mdfp = fopen(MD_CONF, "a")) == NULL)
		return (RET_ERROR);

	(void) snprintf(buf, sizeof (buf), "%s%s", path, NAME_TO_MAJOR);

	if ((targfp = fopen(buf, "r")) == NULL) {
		(void) fclose(mdfp);
		return (RET_ERROR);
	}

	while (fgets(buf, PATH_MAX, targfp) != NULL &&
				(retval == RET_SUCCESS)) {
		cp = strrchr(buf, '\n');
		*cp = 0;
		if (is_blankline(buf))
			continue;
		if (first_entry) {
			if (fprintf(mdfp, "md_targ_nm_table=\"%s\"", buf) < 0)
				retval = RET_ERROR;
			first_entry = 0;
		}
		if (fprintf(mdfp, ",\"%s\"", buf) < 0)
				retval = RET_ERROR;
	}
	if (!first_entry)
		if (fprintf(mdfp, ";\n") < 0)
			retval = RET_ERROR;
	(void) fclose(mdfp);
	(void) fclose(targfp);
	return (retval);
}

/*
 * FUNCTION: write_xlate_to_mdconf
 *	creates a tuple table of <miniroot devt, target devt> in md.conf
 * INPUT: rootpath
 *
 * RETURN VALUES:
 *	RET_SUCCESS
 *	RET_ERROR
 */

int
write_xlate_to_mdconf(char *path)
{
	FILE		*fptr = NULL;
	struct dirent	*dp;
	DIR		*dirp;
	struct stat	statb_dev;
	struct stat	statb_edev;
	char		*devname;
	char		edevname[PATH_MAX];
	char		targname[PATH_MAX];
	char		diskdir[PATH_MAX];
	int		first_devid = 1;
	int		ret = RET_SUCCESS;

	if ((fptr = fopen(MD_CONF, "a")) == NULL) {
		return (RET_ERROR);
	}


	(void) snprintf(diskdir, sizeof (diskdir), "%s%s", path, DISK_DIR);
	if ((dirp = opendir(diskdir)) == NULL) {
		(void) fclose(fptr);
		return (RET_ERROR);
	}

	/* special case to write the first tuple in the table */
	while (((dp = readdir(dirp)) != (struct dirent *)0) &&
						(ret != RET_ERROR)) {
		if ((strcmp(dp->d_name, ".") == 0) ||
		    (strcmp(dp->d_name, "..") == 0))
			continue;

		if ((strlen(diskdir) + strlen(dp->d_name) + 2) > PATH_MAX) {
		    continue;
		}

		(void) snprintf(targname, sizeof (targname), "%s/%s",
		    diskdir, dp->d_name);

		if (stat(targname, &statb_dev) != 0) {
		    continue;
		}

		if ((devname = strstr(targname, DISK_DIR)) == NULL) {
			continue;
		}

		if (_map_to_effective_dev((char *)devname, (char *)&edevname)
		    != 0) {
			continue;
		}

		if (stat(edevname, &statb_edev) != 0) {
			continue;
		}

		if (first_devid) {
			if (fprintf(fptr, "md_xlate_ver=\"%s\";\n"
				"md_xlate=%lu,%lu", VERSION,
				statb_edev.st_rdev, statb_dev.st_rdev) < 0)
				ret = RET_ERROR;
			first_devid = 0;
		}
		if (fprintf(fptr, ",%lu,%lu", statb_edev.st_rdev,
			statb_dev.st_rdev) < 0)
			ret = RET_ERROR;
	} /* end while */

	if (!first_devid)
		if (fprintf(fptr, ";\n") < 0)
			ret = RET_ERROR;
	(void) fclose(fptr);
	(void) closedir(dirp);
	return (ret);
}
