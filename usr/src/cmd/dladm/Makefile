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
# Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
#ident	"@(#)Makefile	1.3	05/07/30 SMI"
#

PROG= dladm

ROOTFS_PROG= $(PROG)
CONFIGFILES= aggregation.conf

POFILE= $(PROG).po

include ../Makefile.cmd

LDLIBS += -ldladm -lmacadm -ldlpi -llaadm -lkstat

ICONFIGFILES= $(CONFIGFILES:%=$(ROOTETC)/%)

$(ICONFIGFILES):= FILEMODE= 644
$(ICONFIGFILES):= OWNER= root
$(ICONFIGFILES):= GROUP= sys

.KEEP_STATE:

all: $(ROOTFS_PROG) 

#
# Message catalog
#
_msg: $(POFILE)

$(POFILE): $(PROG).c
	$(RM) $@
	$(COMPILE.cpp) $(PROG).c > $(POFILE).i
	$(XGETTEXT) $(XGETFLAGS) $(POFILE).i
	sed "/^domain/d" messages.po > $@
	$(RM) messages.po $(POFILE).i

install: all $(ROOTSBINPROG) $(ICONFIGFILES)
	$(RM) $(ROOTUSRSBINPROG)
	-$(SYMLINK) ../../sbin/$(PROG) $(ROOTUSRSBINPROG)

clean:

lint:	lint_PROG

include ../Makefile.targ
