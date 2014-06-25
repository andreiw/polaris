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
 * Copyright (c) 1998-2000 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_STREAMS_STDIO_H
#define	_STREAMS_STDIO_H

#pragma ident	"@(#)streams_stdio.h	1.3	05/06/08 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "fields.h"
#include "statistics.h"
#include "types.h"
#include "utility.h"

extern int stream_stdio_open_for_write(stream_t *str);
extern int stream_stdio_is_closable(stream_t *str);
extern int stream_stdio_close(stream_t *str);
extern int stream_stdio_unlink(stream_t *str);
extern int stream_stdio_free(stream_t *str);

extern ssize_t stream_stdio_fetch_overwrite(stream_t *);
extern void stream_stdio_put_line_unique(stream_t *, line_rec_t *);
extern void stream_stdio_flush(stream_t *);

extern const stream_ops_t stream_stdio_ops;

#ifdef	__cplusplus
}
#endif

#endif	/* _STREAMS_STDIO_H */
