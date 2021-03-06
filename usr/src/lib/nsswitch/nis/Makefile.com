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
#
# Copyright 1993,2001-2003 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# ident	"@(#)Makefile.com	1.10	05/06/08 SMI"
#
# lib/nsswitch/nis/Makefile.com

LIBRARY =	libnss_nis.a
VERS =		.1

OBJECTS =	bootparams_getbyname.o	\
		ether_addr.o		\
		getauthattr.o		\
		getauuser.o		\
		getexecattr.o		\
		getgrent.o		\
		gethostent.o		\
		gethostent6.o		\
		getnetent.o		\
		getnetgrent.o		\
		getprinter.o		\
		getprofattr.o		\
		getprojent.o		\
		getprotoent.o		\
		getpwnam.o		\
		getrpcent.o		\
		getservent.o		\
		getspent.o		\
		getuserattr.o		\
		netmasks.o		\
		nis_common.o

# include common nsswitch library definitions.
include		../../Makefile.com

# install this library in the root filesystem
include ../../../Makefile.rootfs

LDLIBS +=	-lnsl -lsocket
DYNLIB1 =	nss_nis.so$(VERS)
