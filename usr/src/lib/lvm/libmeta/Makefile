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
# ident	"@(#)Makefile	1.10	06/07/29 SMI"
#

include $(SRC)/lib/Makefile.lib

SUBDIRS =	$(MACH) 

MSGSRCS :sh= echo */*.[ch]
MSGFILES = $(MSGSRCS:%.c=%.i)
POFILE = libmeta.po	

DCFILES = common/meta_print.po
DCFILE = libmeta.dc

install :=	TARGET= install
clean :=	TARGET= clean
clobber :=	TARGET= clobber
lint :=		TARGET= lint
test :=		TARGET= test
debug :=	TARGET= debug

CPPFLAGS += -I$(SRC)/lib/lvm/libmeta/common/hdrs

.KEEP_STATE:

all debug install lint: $(SUBDIRS)

clean: $(SUBDIRS)
	$(RM) $(MSGFILES) $(DCFILES)

clobber: $(SUBDIRS)
	$(RM) $(POFILE) $(DCFILE)

$(DCFILE):= XGETFLAGS = -c TRANSLATION_NOTE_LC_TIME -t

$(DCFILE): $(DCFILES)
	$(CAT) $(DCFILES) > $(DCFILE)

$(POFILE): $(MSGFILES)
	$(BUILDPO.msgfiles)

_msg: $(MSGDOMAINPOFILE)

_dc: $(DCMSGDOMAINPOFILE)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:

include $(SRC)/Makefile.msg.targ
