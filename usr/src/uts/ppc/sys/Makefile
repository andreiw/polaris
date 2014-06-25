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
# Copyright 2006 Sun Microsystems Laboratories.  All rights reserved.
# Use is subject to license terms.
#
# ident	"%Z%%M%	%I%	%E% SMI"
#

include ../../../Makefile.master

include Makefile.hdrs

SUBHDRS	=

ROOTDIR=	$(ROOT)/usr/include/sys
SCSIDIR=	$(ROOTDIR)/scsi
SCSIDIRS=	$(SCSIDIR) $(SCSIDIR)/conf $(SCSIDIR)/generic \
		$(SCSIDIR)/impl $(SCSIDIR)/targets
ROOTFSDIR=	$(ROOTDIR)/fs
ROOTDIRS=	$(ROOTDIR) $(ROOTFSDIR)

ROOTHDRS= $(HDRS:%=$(ROOTDIR)/%) $(SUBHDRS:%=$(ROOTDIR)/%)
ROOTFSHDRS= $(FSHDRS:%=$(ROOTDIR)/fs/%)

CHECKHDRS= 			\
	$(HDRS:%.h=%.check)	\
	$(SUBHDRS:%.h=%.check)	\
	$(FSHDRS:%.h=fs/%.check)

# install rules
$(ROOTDIR)/%: %
	$(INS.file)

.KEEP_STATE:

.PARALLEL: $(CHECKHDRS) $(ROOTHDRS) $(ROOTFSHDRS)

install_h: $(ROOTDIRS) .WAIT $(ROOTHDRS) $(ROOTFSHDRS)

$(ROOTDIRS):
	$(INS.dir)

check:	$(CHECKHDRS)
