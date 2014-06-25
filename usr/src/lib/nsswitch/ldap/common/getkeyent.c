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
 * Copyright 2003 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"@(#)getkeyent.c	1.7	05/06/08 SMI"

#include <pwd.h>
#include <ctype.h>
#include "ldap_common.h"

/* publickey attributes filters */
#define	_KEY_CN			"cn"
#define	_KEY_NISPUBLICKEY	"nisPublickey"
#define	_KEY_NISSECRETKEY	"nisSecretkey"
#define	_KEY_UIDNUMBER		"uidnumber"

#define	_F_GETKEY_USER		"(&(objectClass=nisKeyObject)(uidNumber=%s))"
#define	_F_GETKEY_USER_SSD	"(&(%%s)(uidNumber=%s))"
#define	_F_GETKEY_HOST		"(&(objectClass=nisKeyObject)(cn=%s))"
#define	_F_GETKEY_HOST_SSD	"(&(%%s)(cn=%s))"

static const char *keys_attrs[] = {
	_KEY_NISPUBLICKEY,
	_KEY_NISSECRETKEY,
	(char *)NULL
};


/*
 * _nss_ldap_key2ent is the data marshaling method for the publickey getXbyY
 * (e.g., getpublickey() and getsecretkey()) backend processes. This method
 * is called after a successful ldap search has been performed. This method
 * will parse the ldap search values into "public:secret" key string =
 * argp->buf.buffer which the frontend process expects. Three error
 * conditions are expected and returned to nsswitch.
 */

static int
_nss_ldap_key2ent(ldap_backend_ptr be, nss_XbyY_args_t *argp)
{
	int		nss_result;
	char		*keytype = (char *)argp->key.pkey.keytype;
	int		keytypelen = strlen(keytype);
	char		*key_start = NULL;
	int		key_len;
	int		buflen = (size_t)argp->buf.buflen;
	char		*buffer = (char *)argp->buf.buffer;
	char		*ceiling = (char *)NULL;
	ns_ldap_result_t	*result = be->result;
	char		**key_array;

#ifdef DEBUG
	(void) fprintf(stdout, "\n[getpublikey.c: _nss_ldap_passwd2ent]\n");
#endif /* DEBUG */

	if (!argp->buf.result) {
		nss_result = (int)NSS_STR_PARSE_ERANGE;
		goto result_key2ent;
	}
	ceiling = buffer + buflen;
	nss_result = (int)NSS_STR_PARSE_SUCCESS;
	(void) memset(buffer, 0, buflen);

	/* get the publickey */
	key_array = __ns_ldap_getAttr(result->entry, _KEY_NISPUBLICKEY);
	if (key_array == NULL) {
		nss_result = (int)NSS_STR_PARSE_PARSE;
		goto result_key2ent;
	}
	while (*key_array) {
		if (strncasecmp(*key_array, keytype, keytypelen) == NULL)
			break;
		key_array++;
	}
	if (*key_array == NULL) {
		nss_result = (int)NSS_STR_PARSE_PARSE;
		goto result_key2ent;
	}

	key_start = *(key_array) + keytypelen;
	key_len = strlen(key_start) + 1;
	if (buffer + key_len + 2 > ceiling) {
		nss_result = (int)NSS_STR_PARSE_ERANGE;
		goto result_key2ent;
	}
	(void) strncpy(buffer, key_start, key_len);
	(void) strcat(buffer, ":");
	buffer += strlen(buffer);

	/* get the secretkey */
	key_array = __ns_ldap_getAttr(result->entry, _KEY_NISSECRETKEY);
	if (key_array == NULL) {
		/*
		 * if we got this far, it's possible that the secret
		 * key is actually missing or no permission to read it.
		 * For the current implementation, we assume that the
		 * clients have read permission to the secret key.  So,
		 * the only possibility of reaching this here is due to
		 * missing secret key.
		 */
		nss_result = (int)NSS_STR_PARSE_PARSE;
		goto result_key2ent;
	}
	while (*key_array) {
		if (strncasecmp(*key_array, keytype, keytypelen) == NULL)
			break;
		key_array++;
	}
	if (*key_array == NULL) {
		nss_result = (int)NSS_STR_PARSE_PARSE;
		goto result_key2ent;
	}

	key_start = *(key_array) + keytypelen;
	key_len = strlen(key_start);
	if (buffer + key_len + 1 > ceiling) {
		nss_result = (int)NSS_STR_PARSE_ERANGE;
		goto result_key2ent;
	}
	(void) strcat(buffer, key_start);

#ifdef DEBUG
	(void) fprintf(stdout, "\n[getkeys.c: _nss_ldap_key2ent]\n");
	(void) fprintf(stdout, "\treturn: %s\n", buffer);
#endif /* DEBUG */

result_key2ent:

	(void) __ns_ldap_freeResult(&be->result);
	return ((int)nss_result);
}


/*
 * getkeys gets both the public and secret keys from publickey entry by either
 * uid name or host name. This function constructs an ldap search filter using
 * the name invocation parameter and the getpwnam search filter defined. Once
 * the filter is constructed, we search for a matching entry and marshal the
 * data results into struct passwd for the frontend process. The function
 * _nss_ldap_key2ent performs the data marshaling.
 * The lookups will be done using the proxy credential.  We don't want to use
 * the user's credential for lookup at this point because we don't have any
 * secure transport.
 */

static nss_status_t
getkeys(ldap_backend_ptr be, void *a)
{
	nss_XbyY_args_t	*argp = (nss_XbyY_args_t *)a;
	char	searchfilter[SEARCHFILTERLEN];
	char	userdata[SEARCHFILTERLEN];
	char	netname[SEARCHFILTERLEN];
	char	*name, *domain, *p;
	nss_status_t	rc;
	int	ret;

#ifdef DEBUG
	(void) fprintf(stdout, "\n[getpwnam.c: getbyname]\n");
#endif /* DEBUG */

	/*
	 * We need to break it down to find if this is a netname for host
	 * or user.  We'll pass the domain as is to the LDAP call.
	 */
	if (_ldap_filter_name(netname, argp->key.pkey.name, sizeof (netname))
			!= 0)
		return ((nss_status_t)NSS_NOTFOUND);
	domain = strchr(netname, '@');
	if (!domain)
		return ((nss_status_t)NSS_NOTFOUND);

	*domain++ = '\0';
	if ((p = strchr(netname, '.')) == NULL)
		return ((nss_status_t)NSS_NOTFOUND);

	name = ++p;
	if (isdigit(*name)) {
		/* user keys lookup */
		ret = snprintf(searchfilter, sizeof (searchfilter),
		    _F_GETKEY_USER, name);
		if (ret >= sizeof (searchfilter) || ret < 0)
			return ((nss_status_t)NSS_NOTFOUND);

		ret = snprintf(userdata, sizeof (userdata),
		    _F_GETKEY_USER_SSD, name);
		if (ret >= sizeof (userdata) || ret < 0)
			return ((nss_status_t)NSS_NOTFOUND);

		rc = (nss_status_t)_nss_ldap_lookup(be, argp,
				_PASSWD, searchfilter, domain,
				_merge_SSD_filter, userdata);
	} else {
		/* host keys lookup */
		ret = snprintf(searchfilter, sizeof (searchfilter),
		    _F_GETKEY_HOST, name);
		if (ret >= sizeof (searchfilter) || ret < 0)
			return ((nss_status_t)NSS_NOTFOUND);

		ret = snprintf(userdata, sizeof (userdata),
		    _F_GETKEY_HOST_SSD, name);
		if (ret >= sizeof (userdata) || ret < 0)
			return ((nss_status_t)NSS_NOTFOUND);

		rc = (nss_status_t)_nss_ldap_lookup(be, argp,
				_HOSTS, searchfilter, domain,
				_merge_SSD_filter, userdata);
	}
	return (rc);
}


static ldap_backend_op_t keys_ops[] = {
	_nss_ldap_destr,
	0,
	0,
	0,
	getkeys
};


/*
 * _nss_ldap_publickey_constr is where life begins. This function calls the
 * generic ldap constructor function to define and build the abstract
 * data types required to support ldap operations.
 */

/*ARGSUSED0*/
nss_backend_t *
_nss_ldap_publickey_constr(const char *dummy1, const char *dummy2,
			const char *dummy3)
{

#ifdef DEBUG
	(void) fprintf(stdout, "\n[getkeys.c: _nss_ldap_keys_constr]\n");
#endif /* DEBUG */

	return ((nss_backend_t *)_nss_ldap_constr(keys_ops,
		    sizeof (keys_ops)/sizeof (keys_ops[0]),
		    _PUBLICKEY, keys_attrs, _nss_ldap_key2ent));
}
