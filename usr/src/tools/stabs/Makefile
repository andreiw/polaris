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
# Copyright (c) 1996-1998 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.2	05/06/08 SMI"
#

include		../Makefile.tools

all:=           TARGET= all
install:=       TARGET= install
clean:=         TARGET= clean
clobber:=       TARGET= clobber
lint:=          TARGET= lint

SUBDIRS_sparc	= sparc

# build only on archictures that have a SUBDIRS_arch entry above
SUBDIRS		= $(SUBDIRS_$(MACH))

.KEEP_STATE:

all:		$(SUBDIRS)
install:	$(SUBDIRS)
clean:		$(SUBDIRS)
clobber:	$(SUBDIRS)
lint:		$(SUBDIRS)

$(SUBDIRS):	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
