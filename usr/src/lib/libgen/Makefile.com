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
# ident	"@(#)Makefile.com	1.15	06/08/03 SMI"
#

LIBRARY =	libgen.a
VERS =		.1
OBJECTS =	bgets.o bufsplit.o copylist.o eaccess.o gmatch.o isencrypt.o \
		mkdirp.o p2open.o pathfind.o reg_compile.o reg_step.o rmdirp.o \
		strccpy.o strecpy.o strfind.o strrspn.o strtrns.o

include ../../Makefile.lib

# install this library in the root filesystem
include ../../Makefile.rootfs

LIBS =		$(DYNLIB) $(LINTLIB)
LDLIBS +=	-lc

SRCDIR =	../common
$(LINTLIB) :=	SRCS = $(SRCDIR)/$(LINTSRC)

MAPFILES +=	$(MAPFILE32)

CFLAGS +=	$(CCVERBOSE)
CPPFLAGS +=	-D_REENTRANT -D_LARGEFILE64_SOURCE -I../inc -I../../common/inc

.KEEP_STATE:

all: $(LIBS) fnamecheck

lint:	lintcheck

include ../../Makefile.targ
