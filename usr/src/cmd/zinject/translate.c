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
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"@(#)translate.c	1.2	06/05/24 SMI"

#include <libzfs.h>

#undef verify	/* both libzfs.h and zfs_context.h want to define this */

#include <sys/zfs_context.h>

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/file.h>
#include <sys/mntent.h>
#include <sys/mnttab.h>
#include <sys/param.h>
#include <sys/stat.h>

#include <sys/dmu.h>
#include <sys/dmu_objset.h>
#include <sys/dnode.h>

#include <sys/mkdev.h>

#include "zinject.h"

extern void kernel_init(int);
extern void kernel_fini(void);

static int debug;

static void
ziprintf(const char *fmt, ...)
{
	va_list ap;

	if (!debug)
		return;

	va_start(ap, fmt);
	(void) vprintf(fmt, ap);
	va_end(ap);
}

/*
 * Given a full path to a file, translate into a dataset name and a relative
 * path within the dataset.  'dataset' must be at least MAXNAMELEN characters,
 * and 'relpath' must be at least MAXPATHLEN characters.  We also pass a stat64
 * buffer, which we need later to get the object ID.
 */
static int
parse_pathname(const char *fullpath, char *dataset, char *relpath,
    struct stat64 *statbuf)
{
	struct extmnttab mp;
	FILE *fp;
	int match;
	const char *rel;

	if (fullpath[0] != '/') {
		(void) fprintf(stderr, "invalid object '%s': must be full "
		    "path\n", fullpath);
		usage();
		return (-1);
	}

	if (strlen(fullpath) >= MAXPATHLEN) {
		(void) fprintf(stderr, "invalid object; pathname too long\n");
		return (-1);
	}

	if (stat64(fullpath, statbuf) != 0) {
		(void) fprintf(stderr, "cannot open '%s': %s\n",
		    fullpath, strerror(errno));
		return (-1);
	}

	if ((fp = fopen(MNTTAB, "r")) == NULL) {
		(void) fprintf(stderr, "cannot open /etc/mnttab\n");
		return (-1);
	}

	match = 0;
	while (getextmntent(fp, &mp, sizeof (mp)) == 0) {
		if (makedev(mp.mnt_major, mp.mnt_minor) == statbuf->st_dev) {
			match = 1;
			break;
		}
	}

	if (!match) {
		(void) fprintf(stderr, "cannot find mountpoint for '%s'\n",
		    fullpath);
		return (-1);
	}

	if (strcmp(mp.mnt_fstype, MNTTYPE_ZFS) != 0) {
		(void) fprintf(stderr, "invalid path '%s': not a ZFS "
		    "filesystem\n", fullpath);
		return (-1);
	}

	if (strncmp(fullpath, mp.mnt_mountp, strlen(mp.mnt_mountp)) != 0) {
		(void) fprintf(stderr, "invalid path '%s': mountpoint "
		    "doesn't match path\n", fullpath);
		return (-1);
	}

	(void) strcpy(dataset, mp.mnt_special);

	rel = fullpath + strlen(mp.mnt_mountp);
	if (rel[0] == '/')
		rel++;
	(void) strcpy(relpath, rel);

	return (0);
}

/*
 * Convert from a (dataset, path) pair into a (objset, object) pair.  Note that
 * we grab the object number from the inode number, since looking this up via
 * libzpool is a real pain.
 */
/* ARGSUSED */
static int
object_from_path(const char *dataset, const char *path, struct stat64 *statbuf,
    zinject_record_t *record)
{
	objset_t *os;
	int err;

	/*
	 * Before doing any libzpool operations, call sync() to ensure that the
	 * on-disk state is consistent with the in-core state.
	 */
	sync();

	if ((err = dmu_objset_open(dataset, DMU_OST_ZFS,
	    DS_MODE_STANDARD | DS_MODE_READONLY, &os)) != 0) {
		(void) fprintf(stderr, "cannot open dataset '%s': %s\n",
		    dataset, strerror(err));
		return (-1);
	}

	record->zi_objset = dmu_objset_id(os);
	record->zi_object = statbuf->st_ino;

	dmu_objset_close(os);

	return (0);
}

/*
 * Calculate the real range based on the type, level, and range given.
 */
static int
calculate_range(const char *dataset, err_type_t type, int level, char *range,
    zinject_record_t *record)
{
	objset_t *os = NULL;
	dnode_t *dn = NULL;
	int err;
	int ret = -1;

	/*
	 * Determine the numeric range from the string.
	 */
	if (range == NULL) {
		/*
		 * If range is unspecified, set the range to [0,-1], which
		 * indicates that the whole object should be treated as an
		 * error.
		 */
		record->zi_start = 0;
		record->zi_end = -1ULL;
	} else {
		char *end;

		/* XXX add support for suffixes */
		record->zi_start = strtoull(range, &end, 10);


		if (*end == '\0')
			record->zi_end = record->zi_start + 1;
		else if (*end == ',')
			record->zi_end = strtoull(end + 1, &end, 10);

		if (*end != '\0') {
			(void) fprintf(stderr, "invalid range '%s': must be "
			    "a numeric range of the form 'start[,end]'\n",
			    range);
			goto out;
		}
	}

	switch (type) {
	case TYPE_DATA:
		break;

	case TYPE_DNODE:
		/*
		 * If this is a request to inject faults into the dnode, then we
		 * must translate the current (objset,object) pair into an
		 * offset within the metadnode for the objset.  Specifying any
		 * kind of range with type 'dnode' is illegal.
		 */
		if (range != NULL) {
			(void) fprintf(stderr, "range cannot be specified when "
			    "type is 'dnode'\n");
			goto out;
		}

		record->zi_start = record->zi_object * sizeof (dnode_phys_t);
		record->zi_end = record->zi_start + sizeof (dnode_phys_t);
		record->zi_object = 0;
		break;
	}

	/*
	 * Get the dnode associated with object, so we can calculate the block
	 * size.
	 */
	if ((err = dmu_objset_open(dataset, DMU_OST_ANY,
	    DS_MODE_STANDARD | DS_MODE_READONLY, &os)) != 0) {
		(void) fprintf(stderr, "cannot open dataset '%s': %s\n",
		    dataset, strerror(err));
		goto out;
	}

	if (record->zi_object == 0) {
		dn = os->os->os_meta_dnode;
	} else {
		err = dnode_hold(os->os, record->zi_object, FTAG, &dn);
		if (err != 0) {
			(void) fprintf(stderr, "failed to hold dnode "
			    "for object %llu\n",
			    (u_longlong_t)record->zi_object);
			goto out;
		}
	}


	ziprintf("data shift: %d\n", (int)dn->dn_datablkshift);
	ziprintf(" ind shift: %d\n", (int)dn->dn_indblkshift);

	/*
	 * Translate range into block IDs.
	 */
	if (record->zi_start != 0 || record->zi_end != -1ULL) {
		record->zi_start >>= dn->dn_datablkshift;
		record->zi_end >>= dn->dn_datablkshift;
	}

	/*
	 * Check level, and then translate level 0 blkids into ranges
	 * appropriate for level of indirection.
	 */
	record->zi_level = level;
	if (level > 0) {
		ziprintf("level 0 blkid range: [%llu, %llu]\n",
		    record->zi_start, record->zi_end);

		if (level >= dn->dn_nlevels) {
			(void) fprintf(stderr, "level %d exceeds max level "
			    "of object (%d)\n", level, dn->dn_nlevels - 1);
			goto out;
		}

		if (record->zi_start != 0 || record->zi_end != 0) {
			int shift = dn->dn_indblkshift - SPA_BLKPTRSHIFT;

			for (; level > 0; level--) {
				record->zi_start >>= shift;
				record->zi_end >>= shift;
			}
		}
	}

	ret = 0;
out:
	if (dn) {
		if (dn != os->os->os_meta_dnode)
			dnode_rele(dn, FTAG);
	}
	if (os)
		dmu_objset_close(os);

	return (ret);
}

int
translate_record(err_type_t type, const char *object, const char *range,
    int level, zinject_record_t *record, char *poolname, char *dataset)
{
	char path[MAXPATHLEN];
	char *slash;
	struct stat64 statbuf;
	int ret = -1;

	kernel_init(FREAD);

	debug = (getenv("ZINJECT_DEBUG") != NULL);

	ziprintf("translating: %s\n", object);

	if (MOS_TYPE(type)) {
		/*
		 * MOS objects are treated specially.
		 */
		switch (type) {
		case TYPE_MOS:
			record->zi_type = 0;
			break;
		case TYPE_MOSDIR:
			record->zi_type = DMU_OT_OBJECT_DIRECTORY;
			break;
		case TYPE_METASLAB:
			record->zi_type = DMU_OT_OBJECT_ARRAY;
			break;
		case TYPE_CONFIG:
			record->zi_type = DMU_OT_PACKED_NVLIST;
			break;
		case TYPE_BPLIST:
			record->zi_type = DMU_OT_BPLIST;
			break;
		case TYPE_SPACEMAP:
			record->zi_type = DMU_OT_SPACE_MAP;
			break;
		case TYPE_ERRLOG:
			record->zi_type = DMU_OT_ERROR_LOG;
			break;
		}

		dataset[0] = '\0';
		(void) strcpy(poolname, object);
		return (0);
	}

	/*
	 * Convert a full path into a (dataset, file) pair.
	 */
	if (parse_pathname(object, dataset, path, &statbuf) != 0)
		goto err;

	ziprintf("   dataset: %s\n", dataset);
	ziprintf("      path: %s\n", path);

	/*
	 * Convert (dataset, file) into (objset, object)
	 */
	if (object_from_path(dataset, path, &statbuf, record) != 0)
		goto err;

	ziprintf("raw objset: %llu\n", record->zi_objset);
	ziprintf("raw object: %llu\n", record->zi_object);

	/*
	 * For the given object, calculate the real (type, level, range)
	 */
	if (calculate_range(dataset, type, level, (char *)range, record) != 0)
		goto err;

	ziprintf("    objset: %llu\n", record->zi_objset);
	ziprintf("    object: %llu\n", record->zi_object);
	if (record->zi_start == 0 &&
	    record->zi_end == -1ULL)
		ziprintf("     range: all\n");
	else
		ziprintf("     range: [%llu, %llu]\n", record->zi_start,
		    record->zi_end);

	/*
	 * Copy the pool name
	 */
	(void) strcpy(poolname, dataset);
	if ((slash = strchr(poolname, '/')) != NULL)
		*slash = '\0';

	ret = 0;

err:
	kernel_fini();
	return (ret);
}

int
translate_raw(const char *str, zinject_record_t *record)
{
	/*
	 * A raw bookmark of the form objset:object:level:blkid, where each
	 * number is a hexidecimal value.
	 */
	if (sscanf(str, "%llx:%llx:%x:%llx", (u_longlong_t *)&record->zi_objset,
	    (u_longlong_t *)&record->zi_object, &record->zi_level,
	    (u_longlong_t *)&record->zi_start) != 4) {
		(void) fprintf(stderr, "bad raw spec '%s': must be of the form "
		    "'objset:object:level:blkid'\n", str);
		return (-1);
	}

	record->zi_end = record->zi_start;

	return (0);
}

int
translate_device(const char *pool, const char *device, zinject_record_t *record)
{
	char *end;
	zpool_handle_t *zhp;
	nvlist_t *tgt;
	boolean_t isspare;

	/*
	 * Given a device name or GUID, create an appropriate injection record
	 * with zi_guid set.
	 */
	if ((zhp = zpool_open(g_zfs, pool)) == NULL)
		return (-1);

	record->zi_guid = strtoull(device, &end, 16);
	if (record->zi_guid == 0 || *end != '\0') {
		tgt = zpool_find_vdev(zhp, device, &isspare);

		if (tgt == NULL) {
			(void) fprintf(stderr, "cannot find device '%s' in "
			    "pool '%s'\n", device, pool);
			return (-1);
		}

		verify(nvlist_lookup_uint64(tgt, ZPOOL_CONFIG_GUID,
		    &record->zi_guid) == 0);
	}

	return (0);
}
