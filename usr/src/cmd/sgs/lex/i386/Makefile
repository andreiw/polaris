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
# Copyright 1993,2003 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# ident	"@(#)Makefile	1.5	05/06/08 SMI"
#
# cmd/sgs/lex/i386/Makefile
#

PROG=		lex
INCLIST_i386=	-I.

include 	../../../Makefile.cmd
include		../Makefile.com

.KEEP_STATE:

all:		$(PROG)

all_lib:	$(LIBS)

install:	all $(ROOTCCSBINPROG) $(ROOTFORMS)

install_lib:	all_lib $(ROOTLIBS) $(ROOTLINKS) $(DYNLINKLIB) $(ROOTLINT)

include		../Makefile.targ
include		../../../Makefile.targ
