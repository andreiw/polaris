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

#pragma ident	"@(#)main.c	1.4	05/10/25 SMI"

#include <sys/stat.h>
#include <locale.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#include "dconf.h"
#include "minfree.h"
#include "utils.h"

static const char USAGE[] = "\
Usage: %s [-nuy] [-c kernel | curproc | all ] [-d dump-device | swap ]\n\
	[-m min {k|m|%%} ] [-s savecore-dir] [-r root-dir]\n";

static const char OPTS[] = "nuyc:d:m:s:r:";

static const char PATH_DEVICE[] = "/dev/dump";
static const char PATH_CONFIG[] = "/etc/dumpadm.conf";

int
main(int argc, char *argv[])
{
	const char *pname = getpname(argv[0]);

	u_longlong_t minf;
	struct stat st;
	int c;
	int dflag = 0;			/* for checking in use during -d ops */
	int dcmode = DC_CURRENT;	/* kernel settings override unless -u */
	int modified = 0;		/* have we modified the dump config? */
	char *minfstr = NULL;		/* string value of -m argument */
	dumpconf_t dc;			/* current configuration */

	(void) setlocale(LC_ALL, "");
	(void) textdomain(TEXT_DOMAIN);

	/*
	 * Take an initial lap through argv hunting for -r root-dir,
	 * so that we can chroot before opening the configuration file.
	 * We also handle -u and any bad options at this point.
	 */
	while (optind < argc) {
		while ((c = getopt(argc, argv, OPTS)) != (int)EOF) {
			if (c == 'r' && chroot(optarg) == -1)
				die(gettext("failed to chroot to %s"), optarg);
			else if (c == 'u')
				dcmode = DC_OVERRIDE;
			else if (c == '?') {
				(void) fprintf(stderr, gettext(USAGE), pname);
				return (E_USAGE);
			}
		}

		if (optind < argc) {
			warn(gettext("illegal argument -- %s\n"), argv[optind]);
			(void) fprintf(stderr, gettext(USAGE), pname);
			return (E_USAGE);
		}
	}

	if (geteuid() != 0)
		die(gettext("you must be root to use %s\n"), pname);

	/*
	 * If no config file exists yet, we're going to create an empty one,
	 * so set the modified flag to force writing out the file.
	 */
	if (access(PATH_CONFIG, F_OK) == -1)
		modified++;

	/*
	 * Now open and read in the initial values from the config file.
	 * If it doesn't exist, we create an empty file and dc is
	 * initialized with the default values.
	 */
	if (dconf_open(&dc, PATH_DEVICE, PATH_CONFIG, dcmode) == -1)
		return (E_ERROR);

	/*
	 * Take another lap through argv, processing options and
	 * modifying the dumpconf_t as appropriate.
	 */
	for (optind = 1; optind < argc; optind++) {
		while ((c = getopt(argc, argv, OPTS)) != (int)EOF) {
			switch (c) {
			case 'c':
				if (dconf_str2content(&dc, optarg) == -1)
					return (E_USAGE);
				modified++;
				break;
			case 'd':
				if (dconf_str2device(&dc, optarg) == -1)
					return (E_USAGE);
				dflag++;
				modified++;
				break;

			case 'm':
				minfstr = optarg;
				break;

			case 'n':
				dc.dc_enable = DC_OFF;
				modified++;
				break;

			case 's':
				if (stat(optarg, &st) == -1 ||
				    !S_ISDIR(st.st_mode)) {
					warn(gettext("%s is missing or not a "
					    "directory\n"), optarg);
					return (E_USAGE);
				}

				if (dconf_str2savdir(&dc, optarg) == -1)
					return (E_USAGE);
				modified++;
				break;

			case 'y':
				dc.dc_enable = DC_ON;
				modified++;
				break;
			}
		}
	}

	if (minfstr != NULL) {
		if (minfree_compute(dc.dc_savdir, minfstr, &minf) == -1)
			return (E_USAGE);
		if (minfree_write(dc.dc_savdir, minf) == -1)
			return (E_ERROR);
	}

	if (dcmode == DC_OVERRIDE) {
		/*
		 * In override mode, we try to force an update.  If this
		 * fails, we re-load the kernel configuration and write that
		 * out to the file in order to force the file in sync.
		 */
		if (dconf_update(&dc, 0) == -1)
			(void) dconf_getdev(&dc);
		if (dconf_write(&dc) == -1)
			return (E_ERROR);

	} else if (modified) {
		/*
		 * If we're modifying the configuration, then try
		 * to update it, and write out the file if successful.
		 */
		if (dconf_update(&dc, dflag) == -1 ||
		    dconf_write(&dc) == -1)
			return (E_ERROR);
	}

	if (dcmode == DC_CURRENT)
		dconf_print(&dc, stdout);

	if (dconf_close(&dc) == -1)
		warn(gettext("failed to close configuration file"));

	return (E_SUCCESS);
}