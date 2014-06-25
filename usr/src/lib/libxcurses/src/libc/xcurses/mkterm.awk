#
# Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
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
# ident	"@(#)mkterm.awk	1.6	05/06/08 SMI"
#
# mkterm.awk
#
# XCurses Library
#
# Copyright 1990, 1995 by Mortice Kern Systems Inc.  All rights reserved.
#
# $Header: /rd/src/libc/xcurses/rcs/mkterm.awk 1.12 1995/07/26 17:40:26 ant Exp $
#
# USAGE:
# 	awk -f mkterm.awk caps >term.h
# 

BEGIN {
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
print "#ifndef __term_h__"
print "#define __term_h__\t1"
print
print "#define _XOPEN_CURSES"
print
print "#ifndef NCCS"
print "#include <termios.h>"
print "#endif"
print "#ifndef STDIN_FILENO"
print "#include <sys/types.h>"
print "#include <unistd.h>"
print "#endif"
print
print "#define __TERM cur_term->"
}

$4 == "bool" {
	printf "#define %-30s __TERM _bool[%d]\n", $1, BoolCount++
}

$4 == "number" {
	printf "#define %-30s __TERM _num[%d]\n", $1, NumberCount++
}

$4 == "str" {
	printf "#define %-30s __TERM _str[%d]\n", $1, StringCount++
}

END {
print
printf "#define __COUNT_BOOL\t\t%d\n", BoolCount
printf "#define __COUNT_NUM\t\t%d\n", NumberCount
printf "#define __COUNT_STR\t\t%d\n", StringCount
print
print "/*"
print " * MKS Header format for terminfo database files."
print " *"
print " * The header consists of six short integers, stored using VAX/PDP style"
print " * byte swapping (least-significant byte first).  The integers are"
print " *"
print " *  1) magic number (octal 0432);"
print " *  2) the size, in bytes, of the names sections;"
print " *  3) the number of bytes in the boolean section;"
print " *  4) the number of short integers in the numbers section;"
print " *  5) the number of offsets (short integers) in the strings section;"
print " *  6) the size, in bytes, of the string table."
print " *"
print " * Between the boolean and number sections, a null byte is inserted, if"
print " * necessary, to ensure that the number section begins on an even byte"
print " * offset.  All short integers are aligned on a short word boundary."
print " */"
print
print "#define __TERMINFO_MAGIC\t\t0432"
print
print "typedef struct {"
print "\tshort magic;"
print "\tshort name_size;"
print "\tshort bool_count;"
print "\tshort num_count;"
print "\tshort str_count;"
print "\tshort str_size;"
print "} terminfo_header_t;"
print
print "/*"
print " * The following __MOVE_ constants are indices into the _move[] member"
print " * of a SCREEN structure.  The array is used by m_mvcur() for cursor"
print " * motion costs and initialized by newterm()."
print " *"
print " * The following indices refer to relative cursor motion actions that"
print " * have a base-cost times the distance/count."
print " */"
print "#define __MOVE_UP\t\t0"
print "#define __MOVE_DOWN\t\t1"
print "#define __MOVE_LEFT\t\t2"
print "#define __MOVE_RIGHT\t\t3"
print "#define __MOVE_TAB\t\t4"
print "#define __MOVE_BACK_TAB\t\t5"
print
print "#define __MOVE_MAX_RELATIVE\t6"
print
print "/*"
print " * These should have fixed costs."
print " */"
print "#define __MOVE_RETURN\t\t6"
print "#define __MOVE_HOME\t\t7"
print "#define __MOVE_LAST_LINE\t8"
print
print "/*"
print " * These have worst case cost based on moving the maximum possible"
print " * value for a parameter given the screen size."
print " */"
print "#define __MOVE_N_UP\t\t9"
print "#define __MOVE_N_DOWN\t\t10"
print "#define __MOVE_N_LEFT\t\t11"
print "#define __MOVE_N_RIGHT\t\t12"
print "#define __MOVE_ROW\t\t13"
print "#define __MOVE_COLUMN\t\t14"
print "#define __MOVE_ROW_COLUMN\t15"
print
print "#define __MOVE_MAX\t\t16"
print
print "/*"
print " * For a cursor motion to be used there must be a base-cost of at least 1."
print " */"
print "#define __MOVE_INFINITY\t\t1000"
print
print "#define __TERM_ISATTY_IN\t0x0001\t/* Input is a terminal */"
print "#define __TERM_ISATTY_OUT\t0x0002\t/* Output is a terminal */"
print "#define __TERM_HALF_DELAY\t0x0004\t/* halfdelay() has priority. */"
print "#define __TERM_INSERT_MODE\t0x0008\t/* Terminal is in insert mode. */"
print "#define __TERM_NL_IS_CRLF\t0x8000\t/* Newline is mapped on output. */"
print
print "/***"
print " *** Opaque data type.  Keep your grubby mits off."
print " ***/"
print "typedef struct {"
print "\tint _ifd;\t\t\t/* Input file descriptor */"
print "\tint _ofd;\t\t\t/* Output file descriptor */"
print "\tstruct termios _prog;"
print "\tstruct termios _shell;"
print "\tstruct termios _save;"
print "\tshort _co;\t\t\t/* Current colour-pair. */"
print "\tunsigned short _at;\t\t/* Current attribute state. */"
print "\tshort (*_pair)[2];"
print "\tshort (*_color)[3];"
print "\tunsigned short _flags;"
print "\tchar _bool[__COUNT_BOOL];"
print "\tshort _num[__COUNT_NUM];"
print "\tchar *_str[__COUNT_STR];\t/* Pointers into _str_table. */"
print "\tchar *_str_table;"
print "\tchar *_names;\t\t\t/* Terminal alias in _str_table. */"
print "\tchar *_term;\t\t\t/* TERM name loaded. */"
print "\tstruct {"
print "\t\tchar *_seq;"
print "\t\tshort _cost;"
print "\t} _move[__MOVE_MAX];"
print "} TERMINAL;"
print
print "extern TERMINAL *cur_term;"
print
print "extern char *__m_boolnames[];"
print "extern char *__m_boolcodes[];"
print "extern char *__m_boolfnames[];"
print "extern char *__m_numnames[];"
print "extern char *__m_numcodes[];"
print "extern char *__m_numfnames[];"
print "extern char *__m_strnames[];"
print "extern char *__m_strcodes[];"
print "extern char *__m_strfnames[];"
print
print "#ifndef _XOPEN_SOURCE"
print "/*"
print " * Old System V array names."
print " */"
print "#define boolnames\t__m_boolnames"
print "#define boolcodes\t__m_boolcodes"
print "#define boolfnames\t__m_boolfnames"
print "#define numnames\t__m_numnames"
print "#define numcodes\t__m_numcodes"
print "#define numfnames\t__m_numfnames"
print "#define strnames\t__m_strnames"
print "#define strcodes\t__m_strcodes"
print "#define strfnames\t__m_strfnames"
print "#endif /* _XOPEN_SOURCE */"
print
print "/*"
print " * Exposed internal functions."
print " */"
print "extern int __m_putchar(int);"
print "extern int __m_mvcur(int, int, int, int, int (*)(int));"
print "extern int __m_read_terminfo(const char *, TERMINAL *);"
print "extern int __m_setupterm(const char *, int, int, int *);"
print
print "/*"
print " * Globals"
print " */"
print "extern int del_curterm(TERMINAL *);"
print "extern TERMINAL *set_curterm(TERMINAL *);"
print "extern int restartterm(const char *, int, int *);"
print "extern int setupterm(const char *, int, int *);"
print
print "extern int tgetent(char *, char *);"
print "extern int tgetflag(const char *);"
print "extern int tgetnum(const char *);"
print "extern char *tgetstr(const char *, char **);"
print "extern char *tgoto(const char *, int, int);"
print
print "extern int tigetflag(const char *);"
print "extern int tigetnum(const char *);"
print "extern char *tigetstr(const char *);"
print
print "extern int putp(const char *);"
print "extern const char *tparm("
print "\tconst char *, long, long, long, long, long, long, long, long, long);"
print "extern int tputs(const char *, int, int (*)(int));"
print
print "#ifndef _XOPEN_SOURCE_EXTENDED"
print
print "#define putp(str)\t\ttputs(str,1,__m_putchar)"
print "#define del_term\t\tdel_curterm"
print "#define setterm(t)\t\tsetupterm(t,STDOUT_FILENO,(int *) 0)"
print "#define tgoto(cm,c,r)\t\ttparm((char *)(cm), (long)(r), (long)(c))"
print
print "#ifndef _XOPEN_SOURCE"
print "#define beehive_glitch\t\tno_esc_ctrlc"
print "#define teleray_glitch\t\tdest_tabs_magic_smso"
print "#endif /* _XOPEN_SOURCE */"
print
print "#endif /* _XOPEN_SOURCE_EXTENDED */"
print
print "#endif /* __term_h__ */"
}
