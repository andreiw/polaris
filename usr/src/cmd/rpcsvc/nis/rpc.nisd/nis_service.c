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
 *	nis_service.c
 *
 * This module contains the dispatch functions for the NIS+ service. At one
 * time it was generated by rpcgen, however, due to the requirement that it
 * be able to compile to a 4.1/sockets version or a 5.0/tli version and the
 * desire to collect statistics about the time spent in the service functions,
 * it is now "real" source. Changes to the .x file will have to be reconciled
 * with this file.
 */

#pragma ident	"@(#)nis_service.c	1.14	05/06/08 SMI"

#include <stdio.h>
#include <stdlib.h> /* getenv, exit */
#include <syslog.h>
#include <signal.h>
#include <sys/types.h>
#include <memory.h>
#include <stropts.h>
#include <netconfig.h>
#include <sys/resource.h> /* rlimit */
#include <rpc/rpc.h>
#include <rpc/svc.h>
#include <rpcsvc/nis.h>
#include <rpcsvc/yp_prot.h>
#include <string.h>
#include <netdir.h>

#include "nis_proc.h"
#include "nis_svc.h"

#include <nisdb_mt.h>
#include <ldap_util.h>

extern int _rpcpmstart;		 /* Started by a port monitor ? */
extern int _rpcfdtype;		 /* Whether Stream or Datagram ? */
extern int _rpcsvcdirty;	 /* Still serving ? */

/*
 * Private object name checking and printing functions for NIS+
 */
#define	NIS_SVCARG_NOCHECK		0x00000000
#define	NIS_SVCARG_NULLPTR		0x00000001
#define	NIS_SVCARG_EMPTYSTRING		0x00000002
#define	NIS_SVCARG_TRAILINGDOT		0x00000004
#define	NIS_SVCARG_MAXLEN		0x00000008
#define	NIS_SVCARG_ROOTOBJECT		0x00000010

/*
 * NIS_SVCARG_ROOTOBJECT is not included in the CHECKALL set (below) as
 * in effect it is in reverse sense to the TRAILINGDOT check.
 * In addition, we only want to check for the ROOTOBJECT in a few
 * particular cases.
 */

#define	NIS_SVCARG_CHECKALL		(NIS_SVCARG_NULLPTR | \
					NIS_SVCARG_EMPTYSTRING | \
					NIS_SVCARG_TRAILINGDOT | \
					NIS_SVCARG_MAXLEN)

static bool_t		legal_object_name(nis_name, uint_t);
static nis_name		safe_object_name(nis_name);


/*
 * NIS Version 2 (YP) Dispatch table
 */
extern int *ypproc_domain_svc();
extern int *ypproc_domain_nonack_svc();
extern struct ypresp_master *ypproc_master_svc();
extern struct ypresp_val *ypproc_match_svc();
extern struct ypresp_key_val *ypproc_first_svc();
extern struct ypresp_key_val *ypproc_next_svc();
extern struct ypresp_all *ypproc_all_svc();
extern struct ypresp_maplist *ypproc_maplist_svc();
extern bool xdr_ypresp_all();

void
ypprog_svc(rqstp, transp)
	struct svc_req *rqstp;
	register SVCXPRT *transp;
{
	union {
		string_t ypproc_domain_svc_arg;
		string_t ypproc_domain_nonack_svc_arg;
		struct ypreq_key ypproc_match_svc_arg;
		struct ypreq_nokey ypproc_first_svc_arg;
		struct ypreq_key ypproc_next_svc_arg;
		struct ypreq_nokey ypproc_all_svc_arg;
		struct ypreq_nokey ypproc_master_svc_arg;
		string_t ypproc_maplist_svc_arg;
	} argument;
	char *result;
	bool (*xdr_argument)(), (*xdr_result)();
	char *(*local)();

	mark_activity();

	_rpcsvcdirty = 1;
	switch (rqstp->rq_proc) {
	case NULLPROC:
		(void) svc_sendreply(transp, xdr_void, (char *)NULL);
		_rpcsvcdirty = 0;
		return;

	case YPPROC_DOMAIN:
		xdr_argument = xdr_ypdomain_wrap_string;
		xdr_result = (bool (*)()) xdr_bool;
		local = (char *(*)()) ypproc_domain_svc;
		break;

	case YPPROC_DOMAIN_NONACK:
		xdr_argument = xdr_ypdomain_wrap_string;
		xdr_result = (bool (*)()) xdr_bool;
		local = (char *(*)()) ypproc_domain_nonack_svc;
		break;

	case YPPROC_MATCH:
		xdr_argument = xdr_ypreq_key;
		xdr_result = xdr_ypresp_val;
		local = (char *(*)()) ypproc_match_svc;
		break;

	case YPPROC_FIRST:
		xdr_argument = xdr_ypreq_nokey;
		xdr_result = xdr_ypresp_key_val;
		local = (char *(*)()) ypproc_first_svc;
		break;

	case YPPROC_NEXT:
		xdr_argument = xdr_ypreq_key;
		xdr_result = xdr_ypresp_key_val;
		local = (char *(*)()) ypproc_next_svc;
		break;

	case YPPROC_ALL:
		xdr_argument = xdr_ypreq_nokey;
		xdr_result = xdr_ypresp_all;
		local = (char *(*)()) ypproc_all_svc;
		break;

	case YPPROC_MASTER:
		xdr_argument = xdr_ypreq_nokey;
		xdr_result = xdr_ypresp_master;
		local = (char *(*)()) ypproc_master_svc;
		break;

	case YPPROC_MAPLIST:
		xdr_argument = xdr_ypdomain_wrap_string;
		xdr_result = xdr_ypresp_maplist;
		local = (char *(*)()) ypproc_maplist_svc;
		break;

	default:
		svcerr_noproc(transp);
		_rpcsvcdirty = 0;
		return;
	}
	(void) memset((char *)&argument, 0, sizeof (argument));
	if (!svc_getargs(transp, (xdrproc_t)xdr_argument, (char *)&argument)) {
		svcerr_decode(transp);
		_rpcsvcdirty = 0;
		return;
	}
	result = (*local)(&argument, rqstp);
	if (result != NULL && !svc_sendreply(transp, (xdrproc_t)xdr_result,
								result)) {
		svcerr_systemerr(transp);
	}
	if (!svc_freeargs(transp, (xdrproc_t)xdr_argument, (char *)&argument)) {
		syslog(LOG_ERR, "yp_svc: unable to free arguments");
		exit(1);
	}

	__nis_thread_cleanup(__nis_get_tsd());

	_rpcsvcdirty = 0;
}

struct ops_stats nisopstats[24];

static void
start_stat(p)
	ulong_t	p;
{
	if ((p > 0) && (p < 24)) {
		WLOCK(nisopstats);
		nisopstats[p].calls++;
		WULOCK(nisopstats);
		__start_clock(7);
	}
}

static void
stop_stat(p, e)
	ulong_t p;
	int	e;
{
	struct ops_stats	*os;
	ulong_t ndx;

	if ((p > 0) && (p < 24)) {
		WLOCK(nisopstats);
		os = &(nisopstats[p]);
		os->tsamps[os->cursamp] = __stop_clock(7);
		os->cursamp = (++(os->cursamp)%16);
		if (e)
			os->errors++;
		WULOCK(nisopstats);
	}
}

/*
 * NIS Version 3 (NIS+) dispatch function.
 */

void
nis_prog_svc(rqstp, transp)
	struct svc_req *rqstp;
	register SVCXPRT *transp;
{
	union {
		ns_request nis_lookup_svc_arg;
		ns_request nis_add_svc_arg;
		ns_request nis_modify_svc_arg;
		ns_request nis_remove_svc_arg;
		ib_request nis_iblist_svc_arg;
		ib_request nis_ibadd_svc_arg;
		ib_request nis_ibmodify_svc_arg;
		ib_request nis_ibremove_svc_arg;
		ib_request nis_ibfirst_svc_arg;
		ib_request nis_ibnext_svc_arg;
		fd_args nis_finddirectory_svc_arg;
		nis_taglist nis_status_svc_arg;
		dump_args nis_dumplog_svc_arg;
		dump_args nis_dump_svc_arg;
		netobj nis_callback_svc_arg;
		nis_name nis_cptime_svc_arg;
		nis_name nis_checkpoint_svc_arg;
		ping_args nis_ping_svc_arg;
		nis_taglist nis_servstate_svc_arg;
		nis_name nis_mkdir_svc_arg;
		nis_name nis_rmdir_svc_arg;
	} argument;
	char *result;
	bool_t (*xdr_argument)(), (*xdr_result)();
	char *(*local)();
	ulong_t	starttime;
	nis_name *nameaddr = NULL;
	uint_t docheck = NIS_SVCARG_NOCHECK;
	struct netconfig *nconfp;
	char *ipaddrp;

	mark_activity();

	_rpcsvcdirty = 1;

	start_stat(rqstp->rq_proc);

	if (verbose) {
		if ((nconfp = __rpcfd_to_nconf(rqstp->rq_xprt->xp_fd,
		    rqstp->rq_xprt->xp_type)) != NULL) {
			if ((ipaddrp = taddr2uaddr(nconfp,
			    svc_getrpccaller(rqstp->rq_xprt))) != NULL) {
				syslog(LOG_DEBUG, "nis_prog_svc: "
				    "Request %d from %s",
				    rqstp->rq_proc, ipaddrp);
				free(ipaddrp);
			}
			freenetconfigent(nconfp);
		}
	}

	switch (rqstp->rq_proc) {
	case NULLPROC:
		(void) svc_sendreply(transp, xdr_void, (char *)NULL);
		_rpcsvcdirty = 0;
		stop_stat(rqstp->rq_proc, 0);
		return;

	case NIS_LOOKUP:
		xdr_argument = xdr_ns_request;
		xdr_result = xdr_nis_result;
		local = (char *(*)()) nis_lookup_svc;
		nameaddr = &argument.nis_lookup_svc_arg.ns_name;
		docheck = NIS_SVCARG_CHECKALL;
		break;

	case NIS_ADD:
		xdr_argument = xdr_ns_request;
		xdr_result = xdr_nis_result;
		local = (char *(*)()) nis_add_svc;
		nameaddr = &argument.nis_add_svc_arg.ns_name;
		docheck = NIS_SVCARG_CHECKALL;
		break;

	case NIS_MODIFY:
		xdr_argument = xdr_ns_request;
		xdr_result = xdr_nis_result;
		local = (char *(*)()) nis_modify_svc;
		nameaddr = &argument.nis_modify_svc_arg.ns_name;
		docheck = NIS_SVCARG_CHECKALL;
		break;

	case NIS_REMOVE:
		xdr_argument = xdr_ns_request;
		xdr_result = xdr_nis_result;
		local = (char *(*)()) nis_remove_svc;
		nameaddr = &argument.nis_remove_svc_arg.ns_name;
		docheck = NIS_SVCARG_CHECKALL;
		break;

	case NIS_IBLIST:
		xdr_argument = xdr_ib_request;
		xdr_result = xdr_nis_result;
		local = (char *(*)()) nis_iblist_svc;
		nameaddr = &argument.nis_iblist_svc_arg.ibr_name;
		docheck = NIS_SVCARG_CHECKALL;
		break;

	case NIS_IBADD:
		xdr_argument = xdr_ib_request;
		xdr_result = xdr_nis_result;
		local = (char *(*)()) nis_ibadd_svc;
		nameaddr = &argument.nis_ibadd_svc_arg.ibr_name;
		docheck = NIS_SVCARG_CHECKALL;
		break;

	case NIS_IBMODIFY:
		xdr_argument = xdr_ib_request;
		xdr_result = xdr_nis_result;
		local = (char *(*)()) nis_ibmodify_svc;
		nameaddr = &argument.nis_ibmodify_svc_arg.ibr_name;
		docheck = NIS_SVCARG_CHECKALL;
		break;

	case NIS_IBREMOVE:
		xdr_argument = xdr_ib_request;
		xdr_result = xdr_nis_result;
		local = (char *(*)()) nis_ibremove_svc;
		nameaddr = &argument.nis_ibremove_svc_arg.ibr_name;
		docheck = NIS_SVCARG_CHECKALL;
		break;

	case NIS_IBFIRST:
		xdr_argument = xdr_ib_request;
		xdr_result = xdr_nis_result;
		local = (char *(*)()) nis_ibfirst_svc;
		nameaddr = &argument.nis_ibfirst_svc_arg.ibr_name;
		docheck = NIS_SVCARG_CHECKALL;
		break;

	case NIS_IBNEXT:
		xdr_argument = xdr_ib_request;
		xdr_result = xdr_nis_result;
		local = (char *(*)()) nis_ibnext_svc;
		nameaddr = &argument.nis_ibnext_svc_arg.ibr_name;
		docheck = NIS_SVCARG_CHECKALL;
		break;

	case NIS_FINDDIRECTORY:
		xdr_argument = xdr_fd_args;
		xdr_result = xdr_fd_result;
		local = (char *(*)()) nis_finddirectory_svc;
		nameaddr = &argument.nis_finddirectory_svc_arg.dir_name;
		docheck = NIS_SVCARG_CHECKALL;
		break;

	case NIS_STATUS:
		xdr_argument = xdr_nis_taglist;
		xdr_result = xdr_nis_taglist;
		local = (char *(*)()) nis_status_svc;
		break;

	case NIS_DUMPLOG:
		xdr_argument = xdr_dump_args;
		xdr_result = xdr_log_result;
		local = (char *(*)()) nis_dumplog_svc;
		nameaddr = &argument.nis_dumplog_svc_arg.da_dir;
		docheck = NIS_SVCARG_CHECKALL;
		break;

	case NIS_DUMP:
		xdr_argument = xdr_dump_args;
		xdr_result = xdr_log_result;
		local = (char *(*)()) nis_dump_svc;
		nameaddr = &argument.nis_dump_svc_arg.da_dir;
		docheck = NIS_SVCARG_CHECKALL;
		break;

	case NIS_CALLBACK:
		xdr_argument = xdr_netobj;
		xdr_result = xdr_bool;
		local = (char *(*)()) nis_callback_svc;
		break;

	case NIS_CPTIME:
		xdr_argument = xdr_nis_name;
		xdr_result = xdr_u_long;
		local = (char *(*)()) nis_cptime_svc;
		nameaddr = &argument.nis_cptime_svc_arg;
		/*
		 * Setup docheck with the NIS_SVCARG_ROOTOBJECT flag so that
		 * objects called 'root.object' can have their time checked.
		 */
		docheck = NIS_SVCARG_CHECKALL | NIS_SVCARG_ROOTOBJECT;
		break;

	case NIS_CHECKPOINT:
		xdr_argument = xdr_nis_name;
		xdr_result = xdr_cp_result;
		local = (char *(*)()) nis_checkpoint_svc;
		nameaddr = &argument.nis_checkpoint_svc_arg;
		docheck = NIS_SVCARG_NULLPTR | NIS_SVCARG_TRAILINGDOT |
		    NIS_SVCARG_MAXLEN | NIS_SVCARG_ROOTOBJECT;
		break;

	case NIS_PING:
		xdr_argument = xdr_ping_args;
		xdr_result = xdr_void;
		local = (char *(*)()) nis_ping_svc;
		nameaddr = &argument.nis_ping_svc_arg.dir;
		/*
		 * Setup docheck with the NIS_SVCARG_ROOTOBJECT flag so that
		 * objects called 'root.object' can be 'pinged'.
		 */
		docheck = NIS_SVCARG_CHECKALL | NIS_SVCARG_ROOTOBJECT;
		break;

	case NIS_SERVSTATE:
		xdr_argument = xdr_nis_taglist;
		xdr_result = xdr_nis_taglist;
		local = (char *(*)()) nis_servstate_svc;
		break;

	case NIS_MKDIR:
		xdr_argument = xdr_nis_name;
		xdr_result = xdr_nis_error;
		local = (char *(*)()) nis_mkdir_svc;
		nameaddr = &argument.nis_mkdir_svc_arg;
		docheck = NIS_SVCARG_CHECKALL;
		break;

	case NIS_RMDIR:
		xdr_argument = xdr_nis_name;
		xdr_result = xdr_nis_error;
		local = (char *(*)()) nis_rmdir_svc;
		nameaddr = &argument.nis_rmdir_svc_arg;
		docheck = NIS_SVCARG_CHECKALL;
		break;

	default:
		svcerr_noproc(transp);
		_rpcsvcdirty = 0;
		stop_stat(rqstp->rq_proc, 1);
		return;
	}
	(void) memset((char *)&argument, 0, sizeof (argument));
	if (!svc_getargs(transp, xdr_argument, (char *)&argument)) {
		svcerr_decode(transp);
		_rpcsvcdirty = 0;
		stop_stat(rqstp->rq_proc, 1);
		return;
	}
	if (nameaddr == NULL ||
	    legal_object_name(*nameaddr, docheck) == TRUE) {
		result = (*local)(&argument, rqstp);
	} else {
		/*
		 * Service routine expects an object name argument, but
		 * the name supplied was bad.
		 */
		result = (char *)nis_make_error(NIS_BADNAME, 0, 0, 0, 0);
		add_xdr_cleanup(xdr_nis_result, result, "nis_svc result");
		if (verbose)
			syslog(LOG_INFO, "nis_svc: bad name '%s'",
				safe_object_name(*nameaddr));
	}
	if (result != NULL && !svc_sendreply(transp, xdr_result, result)) {
		svcerr_systemerr(transp);
	}
	if (!svc_freeargs(transp, xdr_argument, (char *)&argument)) {
		syslog(LOG_ERR, "nis_svc: unable to free arguments");
		exit(1);
	}
	_rpcsvcdirty = 0;

	/* If the DB deferred an object deletion, perform it now */
	{
		nisdb_tsd_t	*tsd = __nisdb_get_tsd();

		if (tsd != 0) {
			nisdb_obj_del_t	*nod, *next;
			ns_request	nsr;
			nis_result	*res;
			nis_error	*err;

			for (nod = tsd->objDelList; nod != 0; nod = next) {
				next = nod->next;

				nsr.ns_name = nod->objName;
				nsr.ns_object.ns_object_len = 0;
				nsr.ns_object.ns_object_val = 0;

				res = nis_remove_svc(&nsr, 0);
				if (res == 0) {
					logmsg(MSG_NOTIMECHECK, LOG_ERR,
					"Deferred nis_remove(\"%s\") => NULL",
						nod->objName);
					nod->objType = NIS_BOGUS_OBJ;
				} else if (res->status != NIS_SUCCESS) {
					logmsg(MSG_NOTIMECHECK, LOG_ERR,
					"Deferred nis_remove(\"%s\") => %s",
						nod->objName,
						nis_sperrno(res->status));
					nod->objType = NIS_BOGUS_OBJ;
				}

				if (res != 0)
					nis_freeresult(res);

				if (nod->objType == NIS_DIRECTORY_OBJ) {
					err = nis_rmdir_svc(&nod->objName, 0);
					if (err == 0 || *err != NIS_SUCCESS)
						logmsg(MSG_NOTIMECHECK, LOG_ERR,
					"Deferred nis_rmdir(\"%s\") => %s",
						nod->objName,
						err != 0 ? nis_sperrno(*err) :
							"<NIL>");
				}

				sfree(nod->objName);
				sfree(nod);
			}

			tsd->objDelList = 0;
		}
	}

	__nis_thread_cleanup(__nis_get_tsd());

	stop_stat(rqstp->rq_proc, 0);
}

/*
 * check_object_name()
 *
 * Return TRUE if the following assertions hold for "name", FALSE otherwise:
 *
 * - Not a NULL pointer
 *
 * - Not the empty string
 *
 * - Does not exceed the maximum allowed length for a nis_name
 *
 * - Ends with trailing dot.
 *
 */
static bool_t
legal_object_name(nis_name name, uint_t docheck)
{
	size_t namelen;

	if (docheck == NIS_SVCARG_NOCHECK)
		return (TRUE);

	if (name == NULL)
		if ((docheck & NIS_SVCARG_NULLPTR) == NIS_SVCARG_NULLPTR)
			return (FALSE);
		else
			/* NULL pointer allowed and present; no more checks */
			return (TRUE);

	namelen = strlen(name);

	if (namelen == 0)
		if ((docheck & NIS_SVCARG_EMPTYSTRING) ==
		    NIS_SVCARG_EMPTYSTRING)
			return (FALSE);
		else
			/* Empty string allowed and present; no more checks */
			return (TRUE);

	if ((docheck & NIS_SVCARG_MAXLEN) == NIS_SVCARG_MAXLEN &&
		namelen >= NIS_MAXNAMELEN)
		return (FALSE);


	/*
	 * Check for ROOT_OBJECT (root.object) as a valid name. Only
	 * usually set for special cases when dealing with pinging &
	 * checking times on root objects.
	 */

	if ((docheck & NIS_SVCARG_ROOTOBJECT) == NIS_SVCARG_ROOTOBJECT &&
	    (strcmp(ROOT_OBJ, name) == 0))
		return (TRUE);

	if ((docheck & NIS_SVCARG_TRAILINGDOT) == NIS_SVCARG_TRAILINGDOT &&
	    name[namelen-1] != '.')
		return (FALSE);

	return (TRUE);
}

/*
 * safe_object_name()
 *
 * Returns a version of the nis_name argument that is safe to pass to
 * printf et al.
 *
 */
static nis_name
safe_object_name(nis_name name)
{
	static const nis_name nullname  = "<NULL pointer>";
	static const nis_name emptyname = "<Empty string>";
	static const nis_name toolongname = "<Too long>";
	size_t namelen;

	if (name == NULL)
		return (nullname);
	else if ((namelen = strlen(name)) == 0)
		return (emptyname);
	else if (namelen >= NIS_MAXNAMELEN)
		return (toolongname);
	else
		return (name);
}
