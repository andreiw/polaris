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
#pragma ident	"@(#)alloc_pbuf.c	1.6	05/06/08 SMI" 
/*
 * Copyright (c) 1987 Sun Microsystems, Inc.
 */

#include <fcntl.h>
#include <sys/mman.h>

/* allocate the buffer to be used by profil(2)  */
char *
_alloc_profil_buf(size)
	int size;
{
	char *buf;
	int fd;

	if((fd = open("/dev/zero",O_RDONLY)) == -1 ) {
		return (char*) -1;
	}
	buf = (char*) mmap(0, size, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, 0);
	close(fd);
	return buf;
}
