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
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"@(#)door.s	1.3	05/06/08 SMI"

	.file	"door.s"

#include <sys/asm_linkage.h>

	/*
	 * weak aliases for public interfaces
	 */
	ANSI_PRAGMA_WEAK(_door_bind,function)
	ANSI_PRAGMA_WEAK(_door_call,function)
	ANSI_PRAGMA_WEAK(_door_getparam,function)
	ANSI_PRAGMA_WEAK(_door_info,function)
	ANSI_PRAGMA_WEAK(_door_revoke,function)
	ANSI_PRAGMA_WEAK(_door_setparam,function)
	ANSI_PRAGMA_WEAK(_door_unbind,function)

	ANSI_PRAGMA_WEAK(door_bind,function)
	ANSI_PRAGMA_WEAK(door_call,function)
	ANSI_PRAGMA_WEAK(door_getparam,function)
	ANSI_PRAGMA_WEAK(door_info,function)
	ANSI_PRAGMA_WEAK(door_revoke,function)
	ANSI_PRAGMA_WEAK(door_setparam,function)
	ANSI_PRAGMA_WEAK(door_unbind,function)

#include <sys/door.h>
#include "SYS.h"

/*
 * Offsets within struct door_results
 */
#define	DOOR_COOKIE	_MUL(0, CLONGSIZE)
#define	DOOR_DATA_PTR	_MUL(1, CLONGSIZE)
#define	DOOR_DATA_SIZE	_MUL(2, CLONGSIZE)
#define	DOOR_DESC_PTR	_MUL(3, CLONGSIZE)
#define	DOOR_DESC_SIZE	_MUL(4, CLONGSIZE)
#define	DOOR_PC		_MUL(5, CLONGSIZE)
#define	DOOR_SERVERS	_MUL(6, CLONGSIZE)
#define	DOOR_INFO_PTR	_MUL(7, CLONGSIZE)

/*
 * All of the syscalls except door_return() follow the same pattern.  The
 * subcode goes in %r9, after all of the other arguments.
 */
#define	DOOR_SYSCALL(name, code)					\
	ENTRY(name);							\
	movq	$code, %r9;		/* subcode */			\
	SYSTRAP_RVAL1(door);						\
	SYSCERROR;							\
	RET;								\
	SET_SIZE(name)

	DOOR_SYSCALL(__door_bind,	DOOR_BIND)
	DOOR_SYSCALL(__door_call,	DOOR_CALL)
	DOOR_SYSCALL(__door_create,	DOOR_CREATE)
	DOOR_SYSCALL(__door_getparam,	DOOR_GETPARAM)
	DOOR_SYSCALL(__door_info,	DOOR_INFO)
	DOOR_SYSCALL(__door_revoke,	DOOR_REVOKE)
	DOOR_SYSCALL(__door_setparam,	DOOR_SETPARAM)
	DOOR_SYSCALL(__door_ucred,	DOOR_UCRED)
	DOOR_SYSCALL(__door_unbind,	DOOR_UNBIND)
	DOOR_SYSCALL(__door_unref,	DOOR_UNREFSYS)

/*
 * int
 * __door_return(
 *	void 			*data_ptr,
 *	size_t			data_size,	(in bytes)
 *	door_return_desc_t	*door_ptr,	(holds returned desc info)
 *	caddr_t			stack_base,
 *	size_t			stack_size)
 */
	ENTRY(__door_return)
door_restart:
	movq	$DOOR_RETURN, %r9	/* subcode */
	SYSTRAP_RVAL1(door)
	jb	2f			/* errno is set */
	/*
	 * On return, we're serving a door_call.  Our stack looks like this:
	 *
	 *		descriptors (if any)
	 *		data (if any)
	 *	 sp->	struct door_results
	 */
	movl	DOOR_SERVERS(%rsp), %eax
	andl	%eax, %eax	/* test nservers */
	jg	1f
	/*
	 * this is the last server thread - call creation func for more
	 */
	movq	_daref_(door_server_func), %rax
	movq	0(%rax), %rax
	movq	DOOR_INFO_PTR(%rsp), %rdi
	call	*%rax		/* call create function */
1:
	/* Call the door server function now */
	movq	DOOR_COOKIE(%rsp), %rdi
	movq	DOOR_DATA_PTR(%rsp), %rsi
	movq	DOOR_DATA_SIZE(%rsp), %rdx
	movq	DOOR_DESC_PTR(%rsp), %rcx
	movq	DOOR_DESC_SIZE(%rsp), %r8
	movq	DOOR_PC(%rsp), %rax
	call	*%rax
	/* Exit the thread if we return here */
	movq	$0, %rdi
	call	_thr_terminate
	/* NOTREACHED */
2:
	/*
	 * Error during door_return call.  Repark the thread in the kernel if
	 * the error code is EINTR (or ERESTART) and this lwp is still part
	 * of the same process.
	 */
	cmpl	$ERESTART, %eax		/* ERESTART is same as EINTR */
	jne	3f
	movl	$EINTR, %eax
3:
	cmpl	$EINTR, %eax		/* interrupted while waiting? */
	jne	__cerror		/* if not, return the error */

	call	_private_getpid		/* get current process id */
	movq	_daref_(door_create_pid), %rdx
	movl	0(%rdx), %edx
	cmpl	%eax, %edx		/* same process? */
	movl	$EINTR, %eax	/* if no, return EINTR (child of forkall) */
	jne	__cerror

	movq	$0, %rdi		/* clear arguments and restart */
	movq	$0, %rsi
	movq	$0, %rdx
	jmp	door_restart
	SET_SIZE(__door_return)
