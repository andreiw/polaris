/*
 * Copyright 2002 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (c) 2001 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

#pragma ident	"@(#)hex.c	1.1	03/01/14 SMI"

#include <port_before.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <isc/misc.h>
#include <port_after.h>

static const char hex[17] = "0123456789abcdef";

int
isc_gethexstring(unsigned char *buf, size_t len, int count, FILE *fp,
		 int *multiline)
{
	int c, n;
	unsigned char x;
	char *s;
	int result = count;
	
	x = 0; /* silence compiler */
	n = 0;
	while (count > 0) {
		c = fgetc(fp);

		if ((c == EOF) ||
		    (c == '\n' && !*multiline) ||
		    (c == '(' && *multiline) ||
		    (c == ')' && !*multiline))
			goto formerr;
		/* comment */
		if (c == ';') {
			while ((c = fgetc(fp)) != EOF && c != '\n')
				/* empty */
			if (c == '\n' && *multiline)
				continue;
			goto formerr;
		}
		/* white space */
		if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
			continue;
		/* multiline */
		if ('(' == c || c == ')') {
			*multiline = (c == '(' /*)*/);
			continue;
		}
		if ((s = strchr(hex, tolower(c))) == NULL)
			goto formerr;
		x = (x<<4) | (s - hex);
		if (++n == 2) {
			if (len > 0) {
				*buf++ = x;
				len--;
			} else
				result = -1;
			count--;
			n = 0;
		}
	}
	return (result);

 formerr:
	if (c == '\n')
		ungetc(c, fp);
	return (-1);
}

void
isc_puthexstring(FILE *fp, const unsigned char *buf, size_t buflen,
		 size_t len1, size_t len2, const char *sep)
{
	size_t i = 0;

	if (len1 < 4)
		len1 = 4;
	if (len2 < 4)
		len2 = 4;
	while (buflen > 0) {
		fputc(hex[(buf[0]>>4)&0xf], fp);
		fputc(hex[buf[0]&0xf], fp);
		i += 2;
		buflen--;
		buf++;
		if (i >= len1 && sep != NULL) {
			fputs(sep, fp);
			i = 0;
			len1 = len2;
		}
	}
}

void
isc_tohex(const unsigned char *buf, size_t buflen, char *t) {
	while (buflen > 0) {
		*t++ = hex[(buf[0]>>4)&0xf];
		*t++ = hex[buf[0]&0xf];
		buf++;
		buflen--;
	}
	*t = '\0';
}
