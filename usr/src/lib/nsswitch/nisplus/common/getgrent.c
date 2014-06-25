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
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * nisplus/getgrent.c -- NIS+ backend for nsswitch "group" database
 */

#pragma ident	"@(#)getgrent.c	1.28	05/06/08 SMI"

#include <grp.h>
#include <string.h>
#include <stdlib.h>
#include <pwd.h>
#include "nisplus_common.h"
#include "nisplus_tables.h"

struct memdata {
	nss_status_t	nss_err;
	const	char *uname;
	size_t	unamelen;
	gid_t	*gid_array;
	int	numgids;
	int	maxgids;
};

extern uint_t __nis_force_hard_lookups;

extern nis_result *__nis_list_localcb(nis_name, uint_t, int (*)(), void *);
static int gr_cback();

static nss_status_t netid_lookup(struct memdata *, nisplus_backend_ptr_t);
static int netid_cback(nis_name, nis_object*, struct memdata *);

static nss_status_t
getbynam(be, a)
	nisplus_backend_ptr_t	be;
	void			*a;
{
	nss_XbyY_args_t		*argp = (nss_XbyY_args_t *)a;

	return (_nss_nisplus_lookup(be, argp, GR_TAG_NAME, argp->key.name));
}

static nss_status_t
getbygid(be, a)
	nisplus_backend_ptr_t	be;
	void			*a;
{
	nss_XbyY_args_t		*argp = (nss_XbyY_args_t *)a;
	char			gidstr[12];	/* More than enough */

	sprintf(gidstr, "%d", argp->key.gid);
	return (_nss_nisplus_lookup(be, argp, GR_TAG_GID, gidstr));
}

static nss_status_t
getbymember(be, a)
	nisplus_backend_ptr_t	be;
	void			*a;
{
	struct	nss_groupsbymem  *argp = (struct nss_groupsbymem *)a;
	struct	memdata grdata;
	nis_result	*r;
	char buf[NIS_MAXNAMELEN];

	if (strcmp(argp->username, "") == 0 ||
	    strcmp(argp->username, "root") == 0) {
		/*
		 * Assume that "root" can only sensibly be in /etc/group,
		 *   not in NIS or NIS+
		 * If we don't do this, a hung name-service may cause
		 *   a root login or su to hang.
		 * 1251680: Also, we should not allow empty strings to be
		 *   passed to NIS+ backend or application will dump core.
		 */
		return (NSS_NOTFOUND);
	}

	grdata.uname	= argp->username;
	grdata.unamelen	= strlen(argp->username);
	grdata.gid_array = argp->gid_array;
	grdata.maxgids	= argp->maxgids;
	grdata.numgids	= argp->numgids;

	/*
	 * If force_slow_way is not set then we can use the local creds
	 * for the group information.  This assumes that there are admin
	 * procedures in place to keep the local creds and group table
	 * in sync with each other.
	 */
	if (argp->force_slow_way != 1) {
		switch (netid_lookup(&grdata, be)) {
		case NSS_SUCCESS:
			/*
			 * Return SUCESS only if array is full.
			 * Explained in <nss_dbdefs.h>.
			 */
			argp->numgids = grdata.numgids;
			return ((argp->numgids == argp->maxgids)
				? NSS_SUCCESS
				: NSS_NOTFOUND);
		case NSS_NOTFOUND:
		case NSS_UNAVAIL:
			/*
			 * Failover to group map seach if no luck with local
			 * creds (netid).
			 */
			break;
		case NSS_TRYAGAIN:
			return (NSS_TRYAGAIN);
		}
	}

	/*
	 *  Newer versions of rpc.nisd will let us do a "multivalue" search
	 *  on the members column of the group table.  The server will return
	 *  any entries that have the member name listed somewhere in the
	 *  list of members.  Since that is fairly efficient, we try that
	 *  first.  If the server returns NIS_BADATTRIBUTE, then it doesn't
	 *  support this feature and we fail over to the slow way of
	 *  dumping all of the group entries and searching from the member
	 *  name on our own.
	 */
	sprintf(buf, "[members=%s],%s", argp->username, be->table_name);
	r = __nis_list_localcb(buf, NIS_LIST_COMMON | ALL_RESULTS |
			__nis_force_hard_lookups, gr_cback, &grdata);
	if (r && r->status != NIS_BADATTRIBUTE) {
		if (r)
			nis_freeresult(r);
		argp->numgids = grdata.numgids;
		return (NSS_SUCCESS);
	}
	r = __nis_list_localcb(be->table_name, NIS_LIST_COMMON | ALL_RESULTS |
				__nis_force_hard_lookups, gr_cback, &grdata);
	if (r == 0)
		return (NSS_NOTFOUND);

	nis_freeresult(r);
	argp->numgids = grdata.numgids;
	return (NSS_SUCCESS);
}


/*
 * place the results from the nis_object structure into argp->buf.result
 * Returns NSS_STR_PARSE_{SUCCESS, ERANGE, PARSE}
 *
 * This routine does not tolerate non-numeric gr_gid. It
 * will immediately flag a PARSE error and return.
 */
/*ARGSUSED*/
static int
nis_object2ent(nobj, obj, argp)
	int		nobj;
	nis_object	*obj;
	nss_XbyY_args_t	*argp;
{
	char	*buffer, *limit, *val, *endnum, *grstart;
	int		buflen = argp->buf.buflen;
	struct 	group *gr;
	struct	entry_col *ecol;
	int		len;
	char	**memlist;

	limit = argp->buf.buffer + buflen;
	gr = (struct group *)argp->buf.result;
	buffer = argp->buf.buffer;

	/*
	 * If we got more than one nis_object, we just ignore it.
	 * Although it should never have happened.
	 *
	 * ASSUMPTION: All the columns in the NIS+ tables are
	 * null terminated.
	 */

	if (obj->zo_data.zo_type != NIS_ENTRY_OBJ ||
		obj->EN_data.en_cols.en_cols_len < GR_COL) {
		/* namespace/table/object is curdled */
		return (NSS_STR_PARSE_PARSE);
	}
	ecol = obj->EN_data.en_cols.en_cols_val;

	/*
	 * gr_name: group name
	 */
	EC_SET(ecol, GR_NDX_NAME, len, val);
	if (len < 2)
		return (NSS_STR_PARSE_PARSE);
	gr->gr_name = buffer;
	buffer += len;
	if (buffer >= limit)
		return (NSS_STR_PARSE_ERANGE);
	strcpy(gr->gr_name, val);

	/*
	 * gr_passwd: group passwd
	 *
	 * POLICY:  The empty password ("") is gladly accepted.
	 */
	EC_SET(ecol, GR_NDX_PASSWD, len, val);
	if (len == 0) {
		len = 1;
		val = "";
	}
	gr->gr_passwd = buffer;
	buffer += len;
	if (buffer >= limit)
		return (NSS_STR_PARSE_ERANGE);
	strcpy(gr->gr_passwd, val);

	/*
	 * gr_gid: group id
	 */
	EC_SET(ecol, GR_NDX_GID, len, val);
	if (len == 0) {
		return (NSS_STR_PARSE_PARSE);
	} else {
		gr->gr_gid = strtol(val, &endnum, 10);
		if (*endnum != 0) {
			return (NSS_STR_PARSE_PARSE);
		}
	}

	/*
	 * gr_mem: gid list
	 *
	 * We first copy the field that looks like "grp1,grp2,..,grpn\0"
	 * into the buffer and advance the buffer in order to allocate
	 * for the gr_mem vector. We work on the group members in place
	 * in the buffer by replacing the "commas" with \0 and simulataneously
	 * advance the vector and point to a member.
	 *
	 * POLICY: We happily accept a null gid list. NIS+ tables store
	 * that as a single null character.
	 */
	EC_SET(ecol, GR_NDX_MEM, len, val);
	if (len == 0) {
		len = 1;
		val = "";
	}
	grstart = buffer;
	buffer += len;
	if (buffer >= limit)
		return (NSS_STR_PARSE_ERANGE);
	strcpy(grstart, val);

	gr->gr_mem = memlist = (char **)ROUND_UP(buffer, sizeof (char **));
	limit = (char *)ROUND_DOWN(limit, sizeof (char **));

	while ((char *)memlist < limit) {
		char c;
		char *p = grstart;

		if (*p != '\0')		/* avoid empty string */
			*memlist++ = p;
		while ((c = *p) != '\0' && c != ',') {
			p++;
		}
		if (*p == '\0') {	/* all done */
			*memlist = 0;
			break;
		}
		*p++ = '\0';
		grstart = p;
	}
	if ((char *)memlist >= limit)
		return (NSS_STR_PARSE_ERANGE);

	return (NSS_STR_PARSE_SUCCESS);
}

static nisplus_backend_op_t gr_ops[] = {
	_nss_nisplus_destr,
	_nss_nisplus_endent,
	_nss_nisplus_setent,
	_nss_nisplus_getent,
	getbynam,
	getbygid,
	getbymember
};

/*ARGSUSED*/
nss_backend_t *
_nss_nisplus_group_constr(dummy1, dummy2, dummy3)
	const char	*dummy1, *dummy2, *dummy3;
{
	return (_nss_nisplus_constr(gr_ops,
				    sizeof (gr_ops) / sizeof (gr_ops[0]),
				    GR_TBLNAME, nis_object2ent));
}

#define	NEXT	0
#define	DONE	1	/* Stop callback iteration */

/*ARGSUSED*/
static int
gr_cback(objname, obj, g)
	nis_name	objname;
	nis_object	*obj;
	struct memdata	*g;
{
	struct entry_col *ecol;
	char		*val;
	int		len;
	char		*p;
	char		*endp;
	int		i;
	gid_t		gid;

	if (obj->zo_data.zo_type != NIS_ENTRY_OBJ ||
	    /* ==== should check entry type? */
	    obj->EN_data.en_cols.en_cols_len < GR_COL) {
		/*
		 * We've found one bad entry, so throw up our hands
		 *   and say that we don't like any of the entries (?!)
		 */
		g->nss_err = NSS_NOTFOUND;
		return (DONE);
	}

	ecol = obj->EN_data.en_cols.en_cols_val;
	EC_SET(ecol, GR_NDX_MEM, len, val);
	if (len == 0) {
		return (NEXT);
	}
	for (p = val;  (p = strstr(p, g->uname)) != 0;  p++) {
		if ((p == val || p[-1] == ',') &&
		    (p[g->unamelen] == ',' || p[g->unamelen] == '\0')) {
			/* Found uname */
			break;
		}
	}
	if (p == 0) {
		return (NEXT);
	}

	EC_SET(ecol, GR_NDX_GID, len, val);
	if (len == 0) {
		return (NEXT);	/* Actually pretty curdled, but not enough */
				/*   for us to abort the callback sequence */
	}
	val[len - 1] = '\0';
	gid = strtol(val, &endp, 10);
	if (*endp != '\0') {
		return (NEXT);	/* Again, somewhat curdled */
	}
	i = g->numgids;
	while (i-- > 0) {
		if (g->gid_array[i] == gid) {
			return (NEXT);	/* We've already got it, presumably */
					/*   from another name service (!?) */
		}
	}
	if (g->numgids < g->maxgids) {
		g->gid_array[g->numgids++] = gid;
		return (NEXT);
	} else {
		return (DONE);	/* Filled up gid_array, so quit iterating */
	}
}

static nss_status_t
netid_lookup(struct memdata *grdata, nisplus_backend_ptr_t be)
{
	struct passwd	pw;
	char		pwbuf[NSS_BUFLEN_PASSWD];
	nis_result	*r;
	char		buf[NIS_MAXNAMELEN];

	/*
	 * We need to get the users uid given their name as we can't
	 * assume that their NIS+ principle name is username.`domainname`
	 *
	 * We also need to workout which cred.org_dir table to use but the
	 * backend pointer (be) we have is to a group.org_dir table.
	 */

	if ((getpwnam_r(grdata->uname, &pw, pwbuf, sizeof (pwbuf)) == NULL) ||
	    (pw.pw_uid == 0)) {
		return (NSS_NOTFOUND);
	}

	snprintf(buf, NIS_MAXNAMELEN,
	    "[auth_name=%d,auth_type=LOCAL],cred.%s", pw.pw_uid,
	    nis_domain_of(be->table_name));

	/*
	 * NOTE: We are explicitly NOT saying ALL_RESULTS here because
	 * the cred table must NEVER be linked or pathed this is documented
	 * in the Answerbook.
	 */
	r = __nis_list_localcb(buf,
			NIS_LIST_COMMON | __nis_force_hard_lookups,
			netid_cback, grdata);

	if (r == 0)
		return (NSS_NOTFOUND);

	switch (r->status) {
	case NIS_SUCCESS:
	case NIS_CBRESULTS:
		nis_freeresult(r);
		return (NSS_SUCCESS);
	case NIS_TRYAGAIN:
		return (NSS_TRYAGAIN);
	case NIS_UNAVAIL:
		return (NSS_UNAVAIL);
	case NIS_NOTFOUND:
		return (NSS_NOTFOUND);
	default:
		return (NSS_NOTFOUND);
	}
}

static int
netid_cback(nis_name objname, nis_object *obj, struct memdata *g)
{
	struct entry_col *ecol;
	char		*val;
	char		*val_next;
	int		vallen;
	int		i;
	gid_t		gid;
	long		value;

	if (obj->zo_data.zo_type != NIS_ENTRY_OBJ) {
		/*
		 * Badly formed LOCAL cred entry
		 */
		g->nss_err = NSS_NOTFOUND;
		return (DONE);
	}

	ecol = obj->EN_data.en_cols.en_cols_val;
	EC_SET(ecol, CRED_NDX_GROUPLIST, vallen, val);
	if (vallen == 0) {
		return (NEXT);
	}

	/* val should now point to a comma-separated list of gids */

	while (*val != '\0') {
		errno = 0;
		value = strtol(val, &val_next, 10);

		if (val == val_next) {
			return (NSS_STR_PARSE_PARSE);
		} else if ((value == LONG_MAX && errno == ERANGE) ||
		    (ulong_t)value > UINT_MAX) {
			return (NSS_STR_PARSE_ERANGE);
		}

		gid = (gid_t)value;
		if (g->numgids < g->maxgids) {
			i = g->numgids;
			while (i-- > 0)	 {
				if (g->gid_array[i] == gid)
					goto do_next_entry;
			}
			g->gid_array[g->numgids++] = gid;
		} else {
			return (DONE);
		}
do_next_entry:
		val = val_next;
		if (*val == ',') {
			val++;
		}
	}
	if (g->numgids < g->maxgids) {
		return (NEXT);
	} else {
		return (DONE);
	}
}
