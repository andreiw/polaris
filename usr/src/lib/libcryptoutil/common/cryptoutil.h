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

#ifndef _CRYPTOUTIL_H
#define	_CRYPTOUTIL_H

#pragma ident	"@(#)cryptoutil.h	1.8	05/06/08 SMI"

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <syslog.h>
#include <security/cryptoki.h>
#include <sys/param.h>

#define	LOG_STDERR	-1
#define	SUCCESS		0
#define	FAILURE		1
#define	MECH_ID_HEX_LEN	11	/* length of mechanism id in hex form */

#define	_PATH_PKCS11_CONF	"/etc/crypto/pkcs11.conf"
#define	_PATH_KCFD_LOCK		"/var/run/kcfd.lock"

/* $ISA substitution for parsing pkcs11.conf data */
#define	PKCS11_ISA	"/$ISA/"
#if defined(_LP64)
#define	PKCS11_ISA_DIR	"/64/"
#else	/* !_LP64 */
#define	PKCS11_ISA_DIR	"/"
#endif

/* keywords and delimiters for parsing configuration files */
#define	SEP_COLON	":"
#define	SEP_SEMICOLON	";"
#define	SEP_EQUAL	"="
#define	SEP_COMMA	","
#define	METASLOT_KEYWORD	"metaslot"
#define	EF_DISABLED	"disabledlist="
#define	EF_ENABLED	"enabledlist="
#define	EF_NORANDOM	"NO_RANDOM"
#define	METASLOT_TOKEN	"metaslot_token="
#define	METASLOT_SLOT	"metaslot_slot="
#define	METASLOT_STATUS	"metaslot_status="
#define	METASLOT_AUTO_KEY_MIGRATE	"metaslot_auto_key_migrate="
#define	METASLOT_ENABLED	"enabled"
#define	METASLOT_DISABLED	"disabled"
#define	SLOT_DESCRIPTION_SIZE	64
#define	TOKEN_LABEL_SIZE	32

/*
 * Define the following softtoken values that are used by softtoken
 * library, cryptoadm and pktool command.
 */
#define	SOFT_SLOT_DESCRIPTION	\
			"Sun Crypto Softtoken            " \
			"                                "
#define	SOFT_TOKEN_LABEL	"Sun Software PKCS#11 softtoken  "
#define	SOFT_TOKEN_SERIAL	"                "
#define	SOFT_MANUFACTURER_ID	"Sun Microsystems, Inc.          "
#define	SOFT_DEFAULT_PIN	"changeme"

typedef char libname_t[MAXPATHLEN];
typedef char midstr_t[MECH_ID_HEX_LEN];

typedef struct umechlist {
	midstr_t		name;	/* mechanism name in hex form */
	struct umechlist	*next;
} umechlist_t;

typedef struct uentry {
	libname_t	name;
	boolean_t	flag_norandom; /* TRUE if random is disabled */
	boolean_t	flag_enabledlist; /* TRUE if an enabledlist */
	umechlist_t	*policylist; /* disabledlist or enabledlist */
	boolean_t	flag_metaslot_enabled; /* TRUE if metaslot's enabled */
	boolean_t	flag_metaslot_auto_key_migrate;
	CK_UTF8CHAR	metaslot_ks_slot[SLOT_DESCRIPTION_SIZE + 1];
	CK_UTF8CHAR	metaslot_ks_token[TOKEN_LABEL_SIZE + 1];
	int 		count;
} uentry_t;

typedef struct uentrylist {
	uentry_t	*puent;
	struct uentrylist	*next;
} uentrylist_t;

extern void cryptodebug(const char *fmt, ...);
extern void cryptoerror(int priority, const char *fmt, ...);
extern void cryptodebug_init(const char *prefix);

extern char *pkcs11_mech2str(CK_MECHANISM_TYPE mech);
extern CK_RV pkcs11_str2mech(char *mech_str, CK_MECHANISM_TYPE_PTR mech);

extern int get_pkcs11conf_info(uentrylist_t **);
extern umechlist_t *create_umech(char *);
extern void free_umechlist(umechlist_t *);
extern void free_uentrylist(uentrylist_t *);
extern void free_uentry(uentry_t *);

extern void tohexstr(uchar_t *bytes, size_t blen, char *hexstr, size_t hexlen);
extern CK_RV pkcs11_mech2keytype(CK_MECHANISM_TYPE mech_type,
    CK_KEY_TYPE *ktype);
extern char *pkcs11_strerror(CK_RV rv);

#ifdef __cplusplus
}
#endif

#endif /* _CRYPTOUTIL_H */
