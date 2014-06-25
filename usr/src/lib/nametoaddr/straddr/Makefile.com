#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License (the "License").
# You may not use this file except in compliance with the License.
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
# Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# ident	"@(#)Makefile.com	1.6	06/08/01 SMI"
#

LIBRARY =	libstraddr.a
VERS =		.2
OBJECTS =	straddr.o

include ../../../Makefile.lib

#
# Override DYNLIB and LIBLINKS so we don't get a "lib" prefix.
#
DYNLIB =	straddr.so$(VERS)
LIBLINKS =	straddr.so

LIBS =		$(DYNLIB)
LDLIBS +=	-lnsl -lc

SRCDIR =	../common

CPPFLAGS +=	-D_REENTRANT

.KEEP_STATE:

all: $(LIBS)

lint: lintcheck

include ../../../Makefile.targ
