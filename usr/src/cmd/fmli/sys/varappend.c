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
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/


/*
 * Copyright  (c) 1986 AT&T
 *	All Rights Reserved
 */
#ident	"@(#)varappend.c	1.6	05/06/08 SMI"       /* SVr4.0 1.1 */

#include	<stdio.h>
#include	"wish.h"
#include	"var_arrays.h"

/*
 * add another element onto the end of a v_array
 */
struct v_array *
array_append(array, element)
struct v_array	array[];
char	*element;
{
	register struct v_array	*ptr;

	ptr = v_header(array_grow(array, 1));
	if (element != NULL)
		memcpy(ptr_to_ele(ptr, ptr->tot_used - 1), element, ptr->ele_size);
	return v_body(ptr);
}
