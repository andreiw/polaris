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

#pragma ident	"@(#)tcflush.c	1.11	05/06/08 SMI"

/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/


#pragma weak tcflush = _tcflush

#include "synonyms.h"
#include <sys/termios.h>
#include <sys/types.h>
#include <unistd.h>

/*
 * flush read, write or both sides
 */

/*
 * TCIFLUSH  (0) -> flush data received but not read
 * TCOFLUSH  (1) -> flush data written but not transmitted
 * TCIOFLUSH (2) -> flush both
 */

int
tcflush(int fildes, int queue_selector)
{
	return (ioctl(fildes, TCFLSH, queue_selector));
}
