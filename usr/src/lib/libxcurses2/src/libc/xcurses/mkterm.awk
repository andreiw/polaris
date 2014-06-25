#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License, Version 1.0 only
# (the "License").  You may not use this file except in compliance
# with the License.
#
# You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
# or http://www.opensolaris.org/os/licensing.
# See the License for the specific language governing permissions
# and limitations under the License.
#
# When distributing Covered Code, include this CDDL HEADER in each
# file and include the License file at usr/src/OPENSOLARIS.LICENSE.
# If applicable, add the following below this CDDL HEADER, with the
# fields enclosed by brackets "[]" replaced with your own identifying
# information: Portions Copyright [yyyy] [name of copyright owner]
#
# CDDL HEADER END
#
# Copyright (c) 1995-1998 by Sun Microsystems, Inc.
# All rights reserved.
#
# ident	"@(#)mkterm.awk	1.9	05/06/10 SMI"
#
# mkterm.awk
#
# XCurses Library
#
# Copyright 1990, 1995 by Mortice Kern Systems Inc.  All rights reserved.
#
# $Header: /team/ps/sun_xcurses/archive/local_changes/xcurses/src/lib/libxcurses/src/libc/xcurses/rcs/mkterm.awk 1.7 1998/06/04 18:43:42 cbates Exp $
#
# USAGE:
# 	awk -f mkterm.awk caps >term.h
# 

BEGIN {
print "/*"
print " * Copyright (c) 1998 by Sun Microsystems, Inc."
print " * All rights reserved."
print " */"
print 
print "#ifndef	_TERM_H"
print "#define	_TERM_H"
print
print "#pragma ident	\"@(#)term.h	1.9	05/06/10 SMI\""
print
print "/*"
print " * term.h"
print " *"
print " * XCurses Library"
print " *"
print " * **** THIS FILE IS MACHINE GENERATED."
print " * **** DO NOT EDIT THIS FILE."
print " *"
print " * Copyright 1990, 1995 by Mortice Kern Systems Inc.  All rights reserved."
print " *"
printf " * $Header%s\n", "$"
print " */"
print
print
print "#ifdef	__cplusplus"
print "extern \"C\" {"
print "#endif"
print
print "#define	__TERM cur_term->"
}

$4 == "bool" {
	printf "#define	%s\t\t__TERM _bool[%d]\n", $1, BoolCount++
}

$4 == "number" {
	printf "#define	%s\t\t__TERM _num[%d]\n", $1, NumberCount++
}

$4 == "str" {
	printf "#define	%s\t\t__TERM _str[%d]\n", $1, StringCount++
}

END {
print
printf "#define	__COUNT_BOOL\t\t%d\n", BoolCount
printf "#define	__COUNT_NUM\t\t%d\n", NumberCount
printf "#define	__COUNT_STR\t\t%d\n", StringCount
print
#print "/*"
#print " * MKS Header format for terminfo database files."
#print " *"
#print " * The header consists of six short integers, stored using VAX/PDP style"
#print " * byte swapping (least-significant byte first).  The integers are"
#print " *"
#print " *  1) magic number (octal 0432);"
#print " *  2) the size, in bytes, of the names sections;"
#print " *  3) the number of bytes in the boolean section;"
#print " *  4) the number of short integers in the numbers section;"
#print " *  5) the number of offsets (short integers) in the strings section;"
#print " *  6) the size, in bytes, of the string table."
#print " *"
#print " * Between the boolean and number sections, a null byte is inserted, if"
#print " * necessary, to ensure that the number section begins on an even byte"
#print " * offset.  All short integers are aligned on a short word boundary."
#print " */"
#print
#print "#define	__TERMINFO_MAGIC\t\t0432"
#print
#print "typedef struct {"
#print "\tshort magic;"
#print "\tshort name_size;"
#print "\tshort bool_count;"
#print "\tshort num_count;"
#print "\tshort str_count;"
#print "\tshort str_size;"
#print "} terminfo_header_t;"
#print
print "/*"
print " * The following __MOVE_ constants are indices into the _move[] member"
print " * of a SCREEN structure.  The array is used by m_mvcur() for cursor"
print " * motion costs and initialized by newterm()."
print " *"
print " * The following indices refer to relative cursor motion actions that"
print " * have a base-cost times the distance/count."
print " */"
print "#define	__MOVE_UP\t\t0"
print "#define	__MOVE_DOWN\t\t1"
print "#define	__MOVE_LEFT\t\t2"
print "#define	__MOVE_RIGHT\t\t3"
print "#define	__MOVE_TAB\t\t4"
print "#define	__MOVE_BACK_TAB\t\t5"
print
print "#define	__MOVE_MAX_RELATIVE\t6"
print
print "/*"
print " * These should have fixed costs."
print " */"
print "#define	__MOVE_RETURN\t\t6"
print "#define	__MOVE_HOME\t\t7"
print "#define	__MOVE_LAST_LINE\t8"
print
print "/*"
print " * These have worst case cost based on moving the maximum possible"
print " * value for a parameter given the screen size."
print " */"
print "#define	__MOVE_N_UP\t\t9"
print "#define	__MOVE_N_DOWN\t\t10"
print "#define	__MOVE_N_LEFT\t\t11"
print "#define	__MOVE_N_RIGHT\t\t12"
print "#define	__MOVE_ROW\t\t13"
print "#define	__MOVE_COLUMN\t\t14"
print "#define	__MOVE_ROW_COLUMN\t15"
print
print "#define	__MOVE_MAX\t\t16"
print
print "/*"
print " * For a cursor motion to be used there must be a base-cost of at least 1."
print " */"
print "#define	__MOVE_INFINITY\t\t1000"
print
print "#define	__TERM_ISATTY_IN\t0x0001\t/* Input is a terminal */"
print "#define	__TERM_ISATTY_OUT\t0x0002\t/* Output is a terminal */"
print "#define	__TERM_HALF_DELAY\t0x0004\t/* halfdelay() has priority. */"
print "#define	__TERM_INSERT_MODE\t0x0008\t/* Terminal is in insert mode. */"
print "#define	__TERM_NL_IS_CRLF\t0x8000\t/* Newline is mapped on output. */"
print
print "/*"
print " * Opaque data type.  Keep your grubby mits off."
print " */"
print "typedef struct {"
print "\tint	_ifd;\t/* Input file descriptor */"
print "\tint	_ofd;\t/* Output file descriptor */"
#print "\tstruct termios	_prog;"
#print "\tstruct termios	_shell;"
#print "\tstruct termios	_save;"
#print "\tstruct termios	_actual;\t/* What has actually been set in the terminal */"
print "\tvoid	*_prog;"
print "\tvoid	*_shell;"
print "\tvoid	*_save;"
print "\tvoid	*_actual;\t/* What has actually been set in the terminal */"
print "\tshort	_co;\t/* Current color-pair. */"
print "\tunsigned short	_at;\t/* Current attribute state. */"
print "\tshort	(*_pair)[2];"
print "\tshort	(*_color)[3];"
print "\tunsigned short	_flags;"
print "\tchar	_bool[__COUNT_BOOL];"
print "\tshort	_num[__COUNT_NUM];"
print "\tchar	*_str[__COUNT_STR];\t/* Pointers into _str_table. */"
print "\tchar	*_str_table;"
print "\tchar	*_names;\t/* Terminal alias in _str_table. */"
print "\tchar	*_term;\t/* TERM name loaded. */"
print "\tstruct {"
print "\t\tchar	*_seq;"
print "\t\tshort	_cost;"
print "\t} _move[__MOVE_MAX];"
print "} TERMINAL;"
print
print "extern TERMINAL *cur_term;"
print
print "#if !(defined(__cplusplus) && defined(_BOOL))"
print "#ifndef _BOOL_DEFINED"
print "typedef short	bool;"
print "#define	_BOOL_DEFINED"
print "#endif"
print "#endif"
print
print "/*"
print " * Globals"
print " */"
print "extern int del_curterm(TERMINAL *);"
print "extern int putp(const char *);"
print "extern int restartterm(char *, int, int *);"
print "extern TERMINAL *set_curterm(TERMINAL *);"
print "extern int setupterm(char *, int, int *);"
print "extern int tgetent(char *, const char *);"
print "extern int tgetflag(char *);"
print "extern int tgetnum(char *);"
print "extern char *tgetstr(char *, char **);"
print "extern char *tgoto(char *, int, int);"
print "extern int tigetflag(char *);"
print "extern int tigetnum(char *);"
print "extern char *tigetstr(char *);"
print "extern char *tparm("
print "\tchar *, long, long, long, long, long, long, long, long, long);"
print "extern int tputs(const char *, int, int (*)(int));"
print
print "#ifdef	__cplusplus"
print "}"
print "#endif"
print
print "#endif /* _TERM_H */"
}