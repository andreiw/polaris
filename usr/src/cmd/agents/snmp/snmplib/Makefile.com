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
# ident	"@(#)Makefile.com	1.11	06/08/03 SMI"
#

LIBRARY =	libssasnmp.a
VERS =		.1
OBJECTS =	impl.o error.o trace.o signals.o asn1.o pdu.o request.o \
		trap.o snmp_api.o madman_api.o madman_trap.o

include $(SRC)/lib/Makefile.lib

LIBS =		$(DYNLIB) $(LINTLIB)
LDLIBS +=	-lc -lsocket -lnsl
$(LINTLIB):=	SRCS = $(SRCDIR)/$(LINTSRC)

MAPFILES =	../snmp-mapfile-vers 

CPPFLAGS +=	-I..
LINTFLAGS64 +=	-errchk=longptr64

.KEEP_STATE:

all: $(LIBS)

lint: lintcheck

include $(SRC)/lib/Makefile.targ
