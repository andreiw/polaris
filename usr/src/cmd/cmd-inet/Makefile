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
#ident	"@(#)Makefile	1.17	06/01/12 SMI"
#
# Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#

include	../Makefile.cmd

SUBDIRS=	etc sbin usr.bin usr.sbin usr.lib usr.sadm
MSGSUBDIRS=	usr.bin usr.sbin usr.lib usr.sadm
POFILES=        usr.bin/usr.bin.po usr.sbin/usr.sbin.po usr.sadm/usr.sadm.po
POFILE=		cmd-inet.po

all:=		TARGET= all
install:=	TARGET= install
clean:=		TARGET= clean
clobber:=	TARGET= clobber
lint:=		TARGET= lint
_msg:=          TARGET= _msg

.KEEP_STATE:

.PARALLEL:	$(SUBDIRS)

all clean install lint: $(SUBDIRS)

clobber:	$(SUBDIRS) local_clobber

local_clobber:
	$(RM) $(CLOBBERFILES)

_msg: $(MSGSUBDIRS)
	$(RM) $(POFILE)
	$(CAT) $(POFILES) > $(POFILE)
	$(RM)  $(MSGDOMAIN)/$(POFILE)
	$(CP) $(POFILE) $(MSGDOMAIN)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
