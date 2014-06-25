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
 * Copyright (c) 1995-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prom_printf.c	1.17	05/06/08 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>
#include <sys/varargs.h>

#define	NUMTEST

static void _doprint(const char *, va_list, char **);
#ifndef NUMTEST
static void _printn(uint64_t, int, int, int, char **);
#else
static void _printoct64(uint64_t, int, int, char **);
static void _printdec64(uint64_t, int, int, char **);
static void _printhex64(uint64_t, int, int, char **);
static void _printdec32(uint32_t, int, int, char **);
#endif

/*
 * Emit character functions...
 */

static void
_pput_flush(char *start, char *end)
{
	size_t len;

	len = end - start;
	while (prom_write(prom_stdout_ihandle(), start, len, 0, BYTE) == -1)
		;
}

static void
_sput(char c, char **p)
{
	**p = c;
	*p += 1;
}

/*VARARGS1*/
void
prom_printf(const char *fmt, ...)
{
	va_list adx;

	va_start(adx, fmt);
	_doprint(fmt, adx, (char **)0);
	va_end(adx);
}

void
prom_vprintf(const char *fmt, va_list adx)
{
	_doprint(fmt, adx, (char **)0);
}

/*VARARGS2*/
char *
prom_sprintf(char *s, const char *fmt, ...)
{
	char *bp = s;
	va_list adx;

	va_start(adx, fmt);
	_doprint(fmt, adx, &bp);
	*bp++ = (char)0;
	va_end(adx);
	return (s);
}

char *
prom_vsprintf(char *s, const char *fmt, va_list adx)
{
	char *bp = s;

	_doprint(fmt, adx, &bp);
	*bp++ = (char)0;
	return (s);
}

static void
_doprint(const char *fmt, va_list adx, char **bp)
{
	int b, c, i, pad, width, ells;
	char *s, *start;
	char localbuf[100], *lbp;
	int64_t l;
	uint64_t ul;

	if (bp == 0) {
		bp = &lbp;
		lbp = &localbuf[0];
	}
	start = *bp;
loop:
	width = 0;
	while ((c = *fmt++) != '%') {
		if (c == '\0')
			goto out;
		if (c == '\n') {
			_sput('\r', bp);
			_sput('\n', bp);
			if (start == localbuf) {
				_pput_flush(start, *bp);
				lbp = &localbuf[0];
			}
		} else
			_sput((char)c, bp);
		if (start == localbuf && (*bp - start > 80)) {
			_pput_flush(start, *bp);
			lbp = &localbuf[0];
		}
	}

	c = *fmt++;
	for (pad = ' '; c == '0'; c = *fmt++)
		pad = '0';

	for (width = 0; c >= '0' && c <= '9'; c = *fmt++)
		width = width * 10 + c - '0';

	for (ells = 0; c == 'l'; c = *fmt++)
		ells++;

	switch (c) {
	case 'd':
	case 'D':
		b = 10;
		if (ells == 0)
			l = (int64_t)va_arg(adx, int);
		else if (ells == 1)
			l = (int64_t)va_arg(adx, long);
		else
			l = (int64_t)va_arg(adx, int64_t);
		if (l < 0) {
			_sput('-', bp);
			width--;
			ul = -l;
		} else
			ul = l;
		goto number;

	case 'p':
		ells = 1;
		/*FALLTHROUGH*/
	case 'x':
	case 'X':
		b = 16;
		goto u_number;

	case 'u':
		b = 10;
		goto u_number;

	case 'o':
	case 'O':
		b = 8;
u_number:
		if (ells == 0)
			ul = (uint64_t)va_arg(adx, uint_t);
		else if (ells == 1)
			ul = (uint64_t)va_arg(adx, ulong_t);
		else
			ul = (uint64_t)va_arg(adx, uint64_t);
number:
#ifndef NUMTEST
		_printn(ul, b, width, pad, bp);
#else
		switch (b) {
		case 8:
			_printoct64(ul, width, pad, bp);
			break;
		case 10:
			if (ul <= UINT_MAX)
				_printdec32(ul, width, pad, bp);
			else
				_printdec64(ul, width, pad, bp);
			break;
		case 16:
			_printhex64(ul, width, pad, bp);
			break;
		}
#endif
		break;

	case 'c':
		b = va_arg(adx, int);
		for (i = 24; i >= 0; i -= 8)
			if ((c = ((b >> i) & 0x7f)) != 0) {
				if (c == '\n')
					_sput('\r', bp);
				_sput((char)c, bp);
			}
		break;

	case 's':
		s = va_arg(adx, char *);
		while ((c = *s++) != 0) {
			if (c == '\n')
				_sput('\r', bp);
			_sput((char)c, bp);
			if (start == localbuf && (*bp - start > 80)) {
				_pput_flush(start, *bp);
				lbp = &localbuf[0];
			}
		}
		break;

	case '%':
		_sput('%', bp);
		break;
	}
	if (start == localbuf && (*bp - start > 80)) {
		_pput_flush(start, *bp);
		lbp = &localbuf[0];
	}
	goto loop;
out:
	if (start == localbuf && (*bp - start > 0))
		_pput_flush(start, *bp);
}

#ifndef NUMTEST
/*
 * Printn prints a number n in base b.
 * We don't use recursion to avoid deep kernel stacks.
 */
static void
_printn(uint64_t n, int b, int width, int pad, char **bp)
{
	char prbuf[40];
	char *cp;

	cp = prbuf;
	do {
		*cp++ = "0123456789abcdef"[n%b];
		n /= b;
		width--;
	} while (n);
	while (width-- > 0)
		_sput(pad, bp);
	do {
		_sput(*--cp, bp);
	} while (cp > prbuf);
}

#else /* NUMTEST */

static void
_printhex64(uint64_t n, int width, int pad, char **bp)
{
	uint64_t q, r;
	char prbuf[40];
	char *cp;

	cp = prbuf;
	do {
		q = n >> (uint64_t)4;
		r = n & (uint64_t)0xf;
		*cp++ = "0123456789abcdef"[r];
		n = q;
		width--;
	} while (n);
	while (width-- > 0)
		_sput(pad, bp);
	do {
		_sput(*--cp, bp);
	} while (cp > prbuf);
}

static void
_printoct64(uint64_t n, int width, int pad, char **bp)
{
	uint64_t q, r;
	char prbuf[40];
	char *cp;

	cp = prbuf;
	do {
		q = n >> (uint64_t)3;
		r = n & (uint64_t)0x7;
		*cp++ = '0' + r;
		n = q;
		width--;
	} while (n);
	while (width-- > 0)
		_sput(pad, bp);
	do {
		_sput(*--cp, bp);
	} while (cp > prbuf);
}

static void
_printdec32(uint32_t n, int width, int pad, char **bp)
{
	uint32_t q, r;
	char prbuf[40];
	char *cp;

	cp = prbuf;
	do {
		q = n / 10;
		r = n - (q * 10);
		*cp++ = '0' + r;
		n = q;
		width--;
	} while (n);
	while (width-- > 0)
		_sput(pad, bp);
	do {
		_sput(*--cp, bp);
	} while (cp > prbuf);
}

/*
 * 64-bit representation of 1/10, scaled by 32 bits.
 */
#define	S32_1_DIV_10 ((uint64_t)((1ULL << 32) / 10ULL))

static void
_printdec64(uint64_t n64, int width, int pad, char **bp)
{
	uint64_t q64, r64;
	uint32_t n32, q32, r32;
	char prbuf[40];
	char *cp;

	cp = prbuf;
	while (n64 > (uint64_t)UINT_MAX) {
		uint64_t diff;
		uint32_t adj;

		// Approximation of q64/10, truncated.
		q64 = (n64 >> 32) * S32_1_DIV_10;
		diff = n64 - (q64 * 10ULL);
		while (diff > (uint64_t)UINT_MAX) {
			q64 += (diff >> 32) * S32_1_DIV_10;
			diff = n64 - (q64 * 10ULL);
		}
		adj = (uint32_t)diff / 10;
		q64 += adj;
		r64 = n64 - (q64 * 10LL);
		*cp++ = '0' + r64;
		n64 = q64;
		width--;
	}
	n32 = (uint32_t)n64;
	do {
		q32 = n32 / 10;
		r32 = n32 - (q32 * 10);
		*cp++ = '0' + r32;
		n32 = q32;
		width--;
	} while (n32);
	while (width-- > 0)
		_sput(pad, bp);
	do {
		_sput(*--cp, bp);
	} while (cp > prbuf);
}

#endif /* NUMTEST */
