/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */
/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1994 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * Identity and host key generation and maintenance.
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 */

#include "includes.h"
RCSID("$OpenBSD: ssh-keygen.c,v 1.101 2002/06/23 09:39:55 deraadt Exp $");

#pragma ident	"@(#)ssh-keygen.c	1.9	04/09/28 SMI"

#include <openssl/evp.h>
#include <openssl/pem.h>

#include "xmalloc.h"
#include "key.h"
#include "rsa.h"
#include "authfile.h"
#include "uuencode.h"
#include "buffer.h"
#include "bufaux.h"
#include "pathnames.h"
#include "log.h"
#include "readpass.h"
#include <langinfo.h>

#ifdef SMARTCARD
#include "scard.h"
#endif

/* Number of bits in the RSA/DSA key.  This value can be changed on the command line. */
int bits = 1024;

/*
 * Flag indicating that we just want to change the passphrase.  This can be
 * set on the command line.
 */
int change_passphrase = 0;

/*
 * Flag indicating that we just want to change the comment.  This can be set
 * on the command line.
 */
int change_comment = 0;

int quiet = 0;

/* Flag indicating that we just want to see the key fingerprint */
int print_fingerprint = 0;
int print_bubblebabble = 0;

/* The identity file name, given on the command line or entered by the user. */
char identity_file[1024];
int have_identity = 0;

/* This is set to the passphrase if given on the command line. */
char *identity_passphrase = NULL;

/* This is set to the new passphrase if given on the command line. */
char *identity_new_passphrase = NULL;

/* This is set to the new comment if given on the command line. */
char *identity_comment = NULL;

/* Dump public key file in format used by real and the original SSH 2 */
int convert_to_ssh2 = 0;
int convert_from_ssh2 = 0;
int print_public = 0;

char *key_type_name = NULL;

/* argv0 */
#ifdef HAVE___PROGNAME
extern char *__progname;
#else
char *__progname;
#endif

char hostname[MAXHOSTNAMELEN];

static void
ask_filename(struct passwd *pw, const char *prompt)
{
	char buf[1024];
	char *name = NULL;

	if (key_type_name == NULL)
		name = _PATH_SSH_CLIENT_ID_RSA;
	else
		switch (key_type_from_name(key_type_name)) {
		case KEY_RSA1:
			name = _PATH_SSH_CLIENT_IDENTITY;
			break;
		case KEY_DSA:
			name = _PATH_SSH_CLIENT_ID_DSA;
			break;
		case KEY_RSA:
			name = _PATH_SSH_CLIENT_ID_RSA;
			break;
		default:
			(void) fprintf(stderr, gettext("bad key type"));
			exit(1);
			break;
		}

	(void) snprintf(identity_file, sizeof(identity_file), "%s/%s", pw->pw_dir, name);
	(void) fprintf(stderr, "%s (%s): ", gettext(prompt), identity_file);
	(void) fflush(stderr);
	if (fgets(buf, sizeof(buf), stdin) == NULL)
		exit(1);
	if (strchr(buf, '\n'))
		*strchr(buf, '\n') = 0;
	if (strcmp(buf, "") != 0)
		(void) strlcpy(identity_file, buf, sizeof(identity_file));
	have_identity = 1;
}

static Key *
load_identity(char *filename)
{
	char *pass;
	Key *prv;

	prv = key_load_private(filename, "", NULL);
	if (prv == NULL) {
		if (identity_passphrase)
			pass = xstrdup(identity_passphrase);
		else
			pass = read_passphrase(gettext("Enter passphrase: "),
			    RP_ALLOW_STDIN);
		prv = key_load_private(filename, pass, NULL);
		(void) memset(pass, 0, strlen(pass));
		xfree(pass);
	}
	return prv;
}

#define SSH_COM_PUBLIC_BEGIN		"---- BEGIN SSH2 PUBLIC KEY ----"
#define SSH_COM_PUBLIC_END		"---- END SSH2 PUBLIC KEY ----"
#define SSH_COM_PRIVATE_BEGIN		"---- BEGIN SSH2 ENCRYPTED PRIVATE KEY ----"
#define	SSH_COM_PRIVATE_KEY_MAGIC	0x3f6ff9eb

static void
do_convert_to_ssh2(struct passwd *pw)
{
	Key *k;
	u_int len;
	u_char *blob;
	struct stat st;

	if (!have_identity)
		ask_filename(pw, gettext("Enter file in which the key is"));
	if (stat(identity_file, &st) < 0) {
		perror(identity_file);
		exit(1);
	}
	if ((k = key_load_public(identity_file, NULL)) == NULL) {
		if ((k = load_identity(identity_file)) == NULL) {
			(void) fprintf(stderr, gettext("load failed\n"));
			exit(1);
		}
	}
	if (key_to_blob(k, &blob, &len) <= 0) {
		(void) fprintf(stderr, gettext("key_to_blob failed\n"));
		exit(1);
	}
	(void) fprintf(stdout, "%s\n", SSH_COM_PUBLIC_BEGIN);
	(void) fprintf(stdout, gettext(
	    "Comment: \"%u-bit %s, converted from OpenSSH by %s@%s\"\n"),
	    key_size(k), key_type(k),
	    pw->pw_name, hostname);
	dump_base64(stdout, blob, len);
	(void) fprintf(stdout, "%s\n", SSH_COM_PUBLIC_END);
	key_free(k);
	xfree(blob);
	exit(0);
}

static void
buffer_get_bignum_bits(Buffer *b, BIGNUM *value)
{
	int bits = buffer_get_int(b);
	int bytes = (bits + 7) / 8;

	if (buffer_len(b) < bytes)
		fatal("buffer_get_bignum_bits: input buffer too small: "
		    "need %d have %d", bytes, buffer_len(b));
	(void) BN_bin2bn(buffer_ptr(b), bytes, value);
	buffer_consume(b, bytes);
}

static Key *
do_convert_private_ssh2_from_blob(u_char *blob, u_int blen)
{
	Buffer b;
	Key *key = NULL;
	char *type, *cipher;
	u_char *sig, data[] = "abcde12345";
	int magic, rlen, ktype, i1, i2, i3, i4;
	u_int slen;
	u_long e;

	buffer_init(&b);
	buffer_append(&b, blob, blen);

	magic  = buffer_get_int(&b);
	if (magic != SSH_COM_PRIVATE_KEY_MAGIC) {
		error("bad magic 0x%x != 0x%x", magic, SSH_COM_PRIVATE_KEY_MAGIC);
		buffer_free(&b);
		return NULL;
	}
	i1 = buffer_get_int(&b);
	type   = buffer_get_string(&b, NULL);
	cipher = buffer_get_string(&b, NULL);
	i2 = buffer_get_int(&b);
	i3 = buffer_get_int(&b);
	i4 = buffer_get_int(&b);
	debug("ignore (%d %d %d %d)", i1,i2,i3,i4);
	if (strcmp(cipher, "none") != 0) {
		error("unsupported cipher %s", cipher);
		xfree(cipher);
		buffer_free(&b);
		xfree(type);
		return NULL;
	}
	xfree(cipher);

	if (strstr(type, "dsa")) {
		ktype = KEY_DSA;
	} else if (strstr(type, "rsa")) {
		ktype = KEY_RSA;
	} else {
		xfree(type);
		return NULL;
	}
	key = key_new_private(ktype);
	xfree(type);

	switch (key->type) {
	case KEY_DSA:
		buffer_get_bignum_bits(&b, key->dsa->p);
		buffer_get_bignum_bits(&b, key->dsa->g);
		buffer_get_bignum_bits(&b, key->dsa->q);
		buffer_get_bignum_bits(&b, key->dsa->pub_key);
		buffer_get_bignum_bits(&b, key->dsa->priv_key);
		break;
	case KEY_RSA:
		e  = buffer_get_char(&b);
		debug("e %lx", e);
		if (e < 30) {
			e <<= 8;
			e += buffer_get_char(&b);
			debug("e %lx", e);
			e <<= 8;
			e += buffer_get_char(&b);
			debug("e %lx", e);
		}
		if (!BN_set_word(key->rsa->e, e)) {
			buffer_free(&b);
			key_free(key);
			return NULL;
		}
		buffer_get_bignum_bits(&b, key->rsa->d);
		buffer_get_bignum_bits(&b, key->rsa->n);
		buffer_get_bignum_bits(&b, key->rsa->iqmp);
		buffer_get_bignum_bits(&b, key->rsa->q);
		buffer_get_bignum_bits(&b, key->rsa->p);
		rsa_generate_additional_parameters(key->rsa);
		break;
	}
	rlen = buffer_len(&b);
	if (rlen != 0)
		error("do_convert_private_ssh2_from_blob: "
		    "remaining bytes in key blob %d", rlen);
	buffer_free(&b);

	/* try the key */
	(void) key_sign(key, &sig, &slen, data, sizeof(data));
	key_verify(key, sig, slen, data, sizeof(data));
	xfree(sig);
	return key;
}

static void
do_convert_from_ssh2(struct passwd *pw)
{
	Key *k;
	int blen;
	u_int len;
	char line[1024], *p;
	u_char blob[8096];
	char encoded[8096];
	struct stat st;
	int escaped = 0, private = 0, ok;
	FILE *fp;

	if (!have_identity)
		ask_filename(pw, gettext("Enter file in which the key is"));
	if (stat(identity_file, &st) < 0) {
		perror(identity_file);
		exit(1);
	}
	fp = fopen(identity_file, "r");
	if (fp == NULL) {
		perror(identity_file);
		exit(1);
	}
	encoded[0] = '\0';
	while (fgets(line, sizeof(line), fp)) {
		if (!(p = strchr(line, '\n'))) {
			(void) fprintf(stderr, gettext("input line too long.\n"));
			exit(1);
		}
		if (p > line && p[-1] == '\\')
			escaped++;
		if (strncmp(line, "----", 4) == 0 ||
		    strstr(line, ": ") != NULL) {
			if (strstr(line, SSH_COM_PRIVATE_BEGIN) != NULL)
				private = 1;
			if (strstr(line, " END ") != NULL) {
				break;
			}
			/* fprintf(stderr, "ignore: %s", line); */
			continue;
		}
		if (escaped) {
			escaped--;
			/* fprintf(stderr, "escaped: %s", line); */
			continue;
		}
		*p = '\0';
		(void) strlcat(encoded, line, sizeof(encoded));
	}
	len = strlen(encoded);
	if (((len % 4) == 3) &&
	    (encoded[len-1] == '=') &&
	    (encoded[len-2] == '=') &&
	    (encoded[len-3] == '='))
		encoded[len-3] = '\0';
	blen = uudecode(encoded, blob, sizeof(blob));
	if (blen < 0) {
		(void) fprintf(stderr, gettext("uudecode failed.\n"));
		exit(1);
	}
	k = private ?
	    do_convert_private_ssh2_from_blob(blob, blen) :
	    key_from_blob(blob, blen);
	if (k == NULL) {
		(void) fprintf(stderr, gettext("decode blob failed.\n"));
		exit(1);
	}
	ok = private ?
	    (k->type == KEY_DSA ?
		 PEM_write_DSAPrivateKey(stdout, k->dsa, NULL, NULL, 0, NULL, NULL) :
		 PEM_write_RSAPrivateKey(stdout, k->rsa, NULL, NULL, 0, NULL, NULL)) :
	    key_write(k, stdout);
	if (!ok) {
		(void) fprintf(stderr, gettext("key write failed"));
		exit(1);
	}
	key_free(k);
	if (!private)
		(void) fprintf(stdout, "\n");
	(void) fclose(fp);
	exit(0);
}

static void
do_print_public(struct passwd *pw)
{
	Key *prv;
	struct stat st;

	if (!have_identity)
		ask_filename(pw, gettext("Enter file in which the key is"));
	if (stat(identity_file, &st) < 0) {
		perror(identity_file);
		exit(1);
	}
	prv = load_identity(identity_file);
	if (prv == NULL) {
		(void) fprintf(stderr, gettext("load failed\n"));
		exit(1);
	}
	if (!key_write(prv, stdout))
		(void) fprintf(stderr, gettext("key_write failed"));
	key_free(prv);
	(void) fprintf(stdout, "\n");
	exit(0);
}

#ifdef SMARTCARD
static void
do_upload(struct passwd *pw, const char *sc_reader_id)
{
	Key *prv = NULL;
	struct stat st;
	int ret;

	if (!have_identity)
		ask_filename(pw, gettext("Enter file in which the key is"));
	if (stat(identity_file, &st) < 0) {
		perror(identity_file);
		exit(1);
	}
	prv = load_identity(identity_file);
	if (prv == NULL) {
		error("load failed");
		exit(1);
	}
	ret = sc_put_key(prv, sc_reader_id);
	key_free(prv);
	if (ret < 0)
		exit(1);
	log("loading key done");
	exit(0);
}

static void
do_download(struct passwd *pw, const char *sc_reader_id)
{
	Key **keys = NULL;
	int i;

	keys = sc_get_keys(sc_reader_id, NULL);
	if (keys == NULL)
		fatal("cannot read public key from smartcard");
	for (i = 0; keys[i]; i++) {
		key_write(keys[i], stdout);
		key_free(keys[i]);
		(void) fprintf(stdout, "\n");
	}
	xfree(keys);
	exit(0);
}
#endif /* SMARTCARD */

static void
do_fingerprint(struct passwd *pw)
{
	FILE *f;
	Key *public;
	char *comment = NULL, *cp, *ep, line[16*1024], *fp;
	int i, skip = 0, num = 1, invalid = 1;
	enum fp_rep rep;
	enum fp_type fptype;
	struct stat st;

	fptype = print_bubblebabble ? SSH_FP_SHA1 : SSH_FP_MD5;
	rep =    print_bubblebabble ? SSH_FP_BUBBLEBABBLE : SSH_FP_HEX;

	if (!have_identity)
		ask_filename(pw, gettext("Enter file in which the key is"));
	if (stat(identity_file, &st) < 0) {
		perror(identity_file);
		exit(1);
	}
	public = key_load_public(identity_file, &comment);
	if (public != NULL) {
		fp = key_fingerprint(public, fptype, rep);
		(void) printf("%u %s %s\n", key_size(public), fp, comment);
		key_free(public);
		xfree(comment);
		xfree(fp);
		exit(0);
	}
	if (comment)
		xfree(comment);

	f = fopen(identity_file, "r");
	if (f != NULL) {
		while (fgets(line, sizeof(line), f)) {
			i = strlen(line) - 1;
			if (line[i] != '\n') {
				error("line %d too long: %.40s...", num, line);
				skip = 1;
				continue;
			}
			num++;
			if (skip) {
				skip = 0;
				continue;
			}
			line[i] = '\0';

			/* Skip leading whitespace, empty and comment lines. */
			for (cp = line; *cp == ' ' || *cp == '\t'; cp++)
				;
			if (!*cp || *cp == '\n' || *cp == '#')
				continue ;
			i = strtol(cp, &ep, 10);
			if (i == 0 || ep == NULL || (*ep != ' ' && *ep != '\t')) {
				int quoted = 0;
				comment = cp;
				for (; *cp && (quoted || (*cp != ' ' &&
				    *cp != '\t')); cp++) {
					if (*cp == '\\' && cp[1] == '"')
						cp++;	/* Skip both */
					else if (*cp == '"')
						quoted = !quoted;
				}
				if (!*cp)
					continue;
				*cp++ = '\0';
			}
			ep = cp;
			public = key_new(KEY_RSA1);
			if (key_read(public, &cp) != 1) {
				cp = ep;
				key_free(public);
				public = key_new(KEY_UNSPEC);
				if (key_read(public, &cp) != 1) {
					key_free(public);
					continue;
				}
			}
			comment = *cp ? cp : comment;
			fp = key_fingerprint(public, fptype, rep);
			(void) printf("%u %s %s\n", key_size(public), fp,
			    comment ? comment : gettext("no comment"));
			xfree(fp);
			key_free(public);
			invalid = 0;
		}
		(void) fclose(f);
	}
	if (invalid) {
		(void) printf(gettext("%s is not a public key file.\n"),
		       identity_file);
		exit(1);
	}
	exit(0);
}

/*
 * Perform changing a passphrase.  The argument is the passwd structure
 * for the current user.
 */
static void
do_change_passphrase(struct passwd *pw)
{
	char *comment;
	char *old_passphrase, *passphrase1, *passphrase2;
	struct stat st;
	Key *private;

	if (!have_identity)
		ask_filename(pw, gettext("Enter file in which the key is"));
	if (stat(identity_file, &st) < 0) {
		perror(identity_file);
		exit(1);
	}
	/* Try to load the file with empty passphrase. */
	private = key_load_private(identity_file, "", &comment);
	if (private == NULL) {
		if (identity_passphrase)
			old_passphrase = xstrdup(identity_passphrase);
		else
			old_passphrase =
			    read_passphrase(gettext("Enter old passphrase: "),
			    RP_ALLOW_STDIN);
		private = key_load_private(identity_file, old_passphrase,
		    &comment);
		(void) memset(old_passphrase, 0, strlen(old_passphrase));
		xfree(old_passphrase);
		if (private == NULL) {
			(void) printf(gettext("Bad passphrase.\n"));
			exit(1);
		}
	}
	(void) printf(gettext("Key has comment '%s'\n"), comment);

	/* Ask the new passphrase (twice). */
	if (identity_new_passphrase) {
		passphrase1 = xstrdup(identity_new_passphrase);
		passphrase2 = NULL;
	} else {
		passphrase1 =
			read_passphrase(gettext("Enter new passphrase (empty"
			    " for no passphrase): "), RP_ALLOW_STDIN);
		passphrase2 = read_passphrase(gettext("Enter same "
			    "passphrase again: "), RP_ALLOW_STDIN);

		/* Verify that they are the same. */
		if (strcmp(passphrase1, passphrase2) != 0) {
			(void) memset(passphrase1, 0, strlen(passphrase1));
			(void) memset(passphrase2, 0, strlen(passphrase2));
			xfree(passphrase1);
			xfree(passphrase2);
			(void) printf(gettext("Pass phrases do not match.  Try "
				       "again.\n"));
			exit(1);
		}
		/* Destroy the other copy. */
		(void) memset(passphrase2, 0, strlen(passphrase2));
		xfree(passphrase2);
	}

	/* Save the file using the new passphrase. */
	if (!key_save_private(private, identity_file, passphrase1, comment)) {
		(void) printf(gettext("Saving the key failed: %s.\n"), identity_file);
		(void) memset(passphrase1, 0, strlen(passphrase1));
		xfree(passphrase1);
		key_free(private);
		xfree(comment);
		exit(1);
	}
	/* Destroy the passphrase and the copy of the key in memory. */
	(void) memset(passphrase1, 0, strlen(passphrase1));
	xfree(passphrase1);
	key_free(private);		 /* Destroys contents */
	xfree(comment);

	(void) printf(gettext("Your identification has been saved with the new "
		       "passphrase.\n"));
	exit(0);
}

/*
 * Change the comment of a private key file.
 */
static void
do_change_comment(struct passwd *pw)
{
	char new_comment[1024], *comment, *passphrase;
	Key *private;
	Key *public;
	struct stat st;
	FILE *f;
	int fd;

	if (!have_identity)
		ask_filename(pw, gettext("Enter file in which the key is"));
	if (stat(identity_file, &st) < 0) {
		perror(identity_file);
		exit(1);
	}
	private = key_load_private(identity_file, "", &comment);
	if (private == NULL) {
		if (identity_passphrase)
			passphrase = xstrdup(identity_passphrase);
		else if (identity_new_passphrase)
			passphrase = xstrdup(identity_new_passphrase);
		else
			passphrase =
			    read_passphrase(gettext("Enter passphrase: "),
				    RP_ALLOW_STDIN);
		/* Try to load using the passphrase. */
		private = key_load_private(identity_file, passphrase, &comment);
		if (private == NULL) {
			(void) memset(passphrase, 0, strlen(passphrase));
			xfree(passphrase);
			(void) printf(gettext("Bad passphrase.\n"));
			exit(1);
		}
	} else {
		passphrase = xstrdup("");
	}
	if (private->type != KEY_RSA1) {
		(void) fprintf(stderr, gettext("Comments are only supported for "
				    "RSA1 keys.\n"));
		key_free(private);
		exit(1);
	}
	(void) printf(gettext("Key now has comment '%s'\n"), comment);

	if (identity_comment) {
		(void) strlcpy(new_comment, identity_comment, sizeof(new_comment));
	} else {
		(void) printf(gettext("Enter new comment: "));
		(void) fflush(stdout);
		if (!fgets(new_comment, sizeof(new_comment), stdin)) {
			(void) memset(passphrase, 0, strlen(passphrase));
			key_free(private);
			exit(1);
		}
		if (strchr(new_comment, '\n'))
			*strchr(new_comment, '\n') = 0;
	}

	/* Save the file using the new passphrase. */
	if (!key_save_private(private, identity_file, passphrase, new_comment)) {
		(void) printf(gettext("Saving the key failed: %s.\n"), identity_file);
		(void) memset(passphrase, 0, strlen(passphrase));
		xfree(passphrase);
		key_free(private);
		xfree(comment);
		exit(1);
	}
	(void) memset(passphrase, 0, strlen(passphrase));
	xfree(passphrase);
	public = key_from_private(private);
	key_free(private);

	(void) strlcat(identity_file, ".pub", sizeof(identity_file));
	fd = open(identity_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd == -1) {
		(void) printf(gettext("Could not save your public key in %s\n"),
		       identity_file);
		exit(1);
	}
	f = fdopen(fd, "w");
	if (f == NULL) {
		(void) printf(gettext("fdopen %s failed"), identity_file);
		exit(1);
	}
	if (!key_write(public, f))
		(void) fprintf(stderr, gettext("write key failed"));
	key_free(public);
	(void) fprintf(f, " %s\n", new_comment);
	(void) fclose(f);

	xfree(comment);

	(void) printf(gettext("The comment in your key file has been changed.\n"));
	exit(0);
}

static void
usage(void)
{
	(void) fprintf(stderr, gettext(
	"Usage: %s [options]\n"
	"Options:\n"
	"  -b bits     Number of bits in the key to create.\n"
	"  -c          Change comment in private and public key files.\n"
	"  -e          Convert OpenSSH to IETF SECSH key file.\n"
	"  -f filename Filename of the key file.\n"
	"  -i          Convert IETF SECSH to OpenSSH key file.\n"
	"  -l          Show fingerprint of key file.\n"
	"  -p          Change passphrase of private key file.\n"
	"  -q          Quiet.\n"
	"  -y          Read private key file and print public key.\n"
	"  -t type     Specify type of key to create.\n"
	"  -B          Show bubblebabble digest of key file.\n"
	"  -C comment  Provide new comment.\n"
	"  -N phrase   Provide new passphrase.\n"
	"  -P phrase   Provide old passphrase.\n"
#ifdef SMARTCARD
	"  -D reader   Download public key from smartcard.\n"
	"  -U reader   Upload private key to smartcard.\n"
#endif /* SMARTCARD */
	), __progname);

	exit(1);
}

/*
 * Main program for key management.
 */
int
main(int ac, char **av)
{
	char dotsshdir[MAXPATHLEN], comment[1024], *passphrase1, *passphrase2;
	char *reader_id = NULL;
	Key *private, *public;
	struct passwd *pw;
	struct stat st;
	int opt, type, fd;
#ifdef SMARTCARD
	int download = 0;
#endif /* SMARTCARD */
	FILE *f;

	extern int optind;
	extern char *optarg;

	__progname = get_progname(av[0]);

	(void) g11n_setlocale(LC_ALL, "");

	SSLeay_add_all_algorithms();
	init_rng();
	seed_rng();

	/* we need this for the home * directory.  */
	pw = getpwuid(getuid());
	if (!pw) {
		(void) printf(gettext("You don't exist, go away!\n"));
		exit(1);
	}
	if (gethostname(hostname, sizeof(hostname)) < 0) {
		perror("gethostname");
		exit(1);
	}

#ifdef SMARTCARD
#define GETOPT_ARGS "deiqpclBRxXyb:f:t:U:D:P:N:C:"
#else
#define GETOPT_ARGS "deiqpclBRxXyb:f:t:P:N:C:"
#endif /* SMARTCARD */
	while ((opt = getopt(ac, av, GETOPT_ARGS)) != -1) {
		switch (opt) {
		case 'b':
			bits = atoi(optarg);
			if (bits < 512 || bits > 32768) {
				(void) printf(gettext("Bits has bad value.\n"));
				exit(1);
			}
			break;
		case 'l':
			print_fingerprint = 1;
			break;
		case 'B':
			print_bubblebabble = 1;
			break;
		case 'p':
			change_passphrase = 1;
			break;
		case 'c':
			change_comment = 1;
			break;
		case 'f':
			(void) strlcpy(identity_file, optarg, sizeof(identity_file));
			have_identity = 1;
			break;
		case 'P':
			identity_passphrase = optarg;
			break;
		case 'N':
			identity_new_passphrase = optarg;
			break;
		case 'C':
			identity_comment = optarg;
			break;
		case 'q':
			quiet = 1;
			break;
		case 'R':
			/* unused */
			exit(0);
			break;
		case 'e':
		case 'x':
			/* export key */
			convert_to_ssh2 = 1;
			break;
		case 'i':
		case 'X':
			/* import key */
			convert_from_ssh2 = 1;
			break;
		case 'y':
			print_public = 1;
			break;
		case 'd':
			key_type_name = "dsa";
			break;
		case 't':
			key_type_name = optarg;
			break;
#ifdef SMARTCARD
		case 'D':
			download = 1;
		case 'U':
			reader_id = optarg;
			break;
#endif
		case '?':
		default:
			usage();
		}
	}
	if (optind < ac) {
		(void) printf(gettext("Too many arguments.\n"));
		usage();
	}
	if (change_passphrase && change_comment) {
		(void) printf(gettext("Can only have one of -p and -c.\n"));
		usage();
	}
	if (print_fingerprint || print_bubblebabble)
		do_fingerprint(pw);
	if (change_passphrase)
		do_change_passphrase(pw);
	if (change_comment)
		do_change_comment(pw);
	if (convert_to_ssh2)
		do_convert_to_ssh2(pw);
	if (convert_from_ssh2)
		do_convert_from_ssh2(pw);
	if (print_public)
		do_print_public(pw);
	if (reader_id != NULL) {
#ifdef SMARTCARD
		if (download)
			do_download(pw, reader_id);
		else
			do_upload(pw, reader_id);
#else /* SMARTCARD */
		fatal("no support for smartcards.");
#endif /* SMARTCARD */
	}

	arc4random_stir();

	if (key_type_name == NULL) {
		(void) printf(gettext("You must specify a key type (-t).\n"));
		usage();
	}
	type = key_type_from_name(key_type_name);
	if (type == KEY_UNSPEC) {
		(void) fprintf(stderr, gettext("unknown key type %s\n"),
			key_type_name);
		exit(1);
	}
	if (!quiet)
		(void) printf(gettext("Generating public/private %s key pair.\n"),
			key_type_name);
	private = key_generate(type, bits);
	if (private == NULL) {
		(void) fprintf(stderr, gettext("key_generate failed"));
		exit(1);
	}
	public  = key_from_private(private);

	if (!have_identity)
		ask_filename(pw, gettext("Enter file in which to save the key"));

	/* Create ~/.ssh directory if it doesn\'t already exist. */
	(void) snprintf(dotsshdir, sizeof dotsshdir, "%s/%s", pw->pw_dir, _PATH_SSH_USER_DIR);
	if (strstr(identity_file, dotsshdir) != NULL &&
	    stat(dotsshdir, &st) < 0) {
		if (mkdir(dotsshdir, 0700) < 0)
			error("Could not create directory '%s'.", dotsshdir);
		else if (!quiet)
			(void) printf(gettext("Created directory '%s'.\n"), dotsshdir);
	}
	/* If the file already exists, ask the user to confirm. */
	if (stat(identity_file, &st) >= 0) {
		char yesno[128];
		(void) printf(gettext("%s already exists.\n"), identity_file);
		(void) printf(gettext("Overwrite (%s/%s)? "),
				      nl_langinfo(YESSTR), nl_langinfo(NOSTR));
		(void) fflush(stdout);
		if (fgets(yesno, sizeof(yesno), stdin) == NULL)
			exit(1);
		if (strcasecmp(yesno, nl_langinfo(YESSTR)) != 0)
			exit(1);
	}
	/* Ask for a passphrase (twice). */
	if (identity_passphrase)
		passphrase1 = xstrdup(identity_passphrase);
	else if (identity_new_passphrase)
		passphrase1 = xstrdup(identity_new_passphrase);
	else {
passphrase_again:
		passphrase1 =
			read_passphrase(gettext("Enter passphrase (empty "
			"for no passphrase): "), RP_ALLOW_STDIN);
		passphrase2 = read_passphrase(gettext("Enter same "
			    "passphrase again: "), RP_ALLOW_STDIN);
		if (strcmp(passphrase1, passphrase2) != 0) {
			/*
			 * The passphrases do not match.  Clear them and
			 * retry.
			 */
			(void) memset(passphrase1, 0, strlen(passphrase1));
			(void) memset(passphrase2, 0, strlen(passphrase2));
			xfree(passphrase1);
			xfree(passphrase2);
			(void) printf(gettext("Passphrases do not match.  Try "
				    "again.\n"));
			goto passphrase_again;
		}
		/* Clear the other copy of the passphrase. */
		(void) memset(passphrase2, 0, strlen(passphrase2));
		xfree(passphrase2);
	}

	if (identity_comment) {
		(void) strlcpy(comment, identity_comment, sizeof(comment));
	} else {
		/* Create default commend field for the passphrase. */
		(void) snprintf(comment, sizeof comment, "%s@%s", pw->pw_name, hostname);
	}

	/* Save the key with the given passphrase and comment. */
	if (!key_save_private(private, identity_file, passphrase1, comment)) {
		(void) printf(gettext("Saving the key failed: %s.\n"), identity_file);
		(void) memset(passphrase1, 0, strlen(passphrase1));
		xfree(passphrase1);
		exit(1);
	}
	/* Clear the passphrase. */
	(void) memset(passphrase1, 0, strlen(passphrase1));
	xfree(passphrase1);

	/* Clear the private key and the random number generator. */
	key_free(private);
	arc4random_stir();

	if (!quiet)
		(void) printf(gettext("Your identification has been saved in %s.\n"),
			identity_file);

	(void) strlcat(identity_file, ".pub", sizeof(identity_file));
	fd = open(identity_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd == -1) {
		(void) printf(gettext("Could not save your public key in %s\n"),
			identity_file);
		exit(1);
	}
	f = fdopen(fd, "w");
	if (f == NULL) {
		(void) printf(gettext("fdopen %s failed"), identity_file);
		exit(1);
	}
	if (!key_write(public, f))
		(void) fprintf(stderr, gettext("write key failed"));
	(void) fprintf(f, " %s\n", comment);
	(void) fclose(f);

	if (!quiet) {
		char *fp = key_fingerprint(public, SSH_FP_MD5, SSH_FP_HEX);
		(void) printf(gettext("Your public key has been saved in %s.\n"),
		    identity_file);
		(void) printf(gettext("The key fingerprint is:\n"));
		(void) printf("%s %s\n", fp, comment);
		xfree(fp);
	}

	key_free(public);
	return(0);
	/* NOTREACHED */
}
