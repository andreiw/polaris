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
#ident	"@(#)Makefile	1.4	05/06/08 SMI"
#
# Copyright (c) 1999-2000 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/picl/plugins/sun4u/grover/frutree/Makefile
#
include	$(SRC)/Makefile.psm

# include library definitions
include $(SRC)/lib/Makefile.lib

ROOT_PLATFORM = $(USR_PLAT_DIR)/SUNW,Sun-Blade-100

include	$(SRC)/cmd/picl/plugins/Makefile.com

ROOTLIBDIR	= $(ROOT_PLAT_PLUGINDIR)
ROOTLIBDIR	:= OWNER = root
ROOTLIBDIR	:= GROUP = sys

CONF=           piclfrutree.conf
ROOTCONF=       $(CONF:%=$(ROOTLIBDIR)/%)
$(ROOTCONF)	:= FILEMODE = 0644
INFOS=          piclfrutree.info system-board.info

ROOTLINTDIR =   $(ROOTLIBDIR)

.KEEP_STATE:

SUBDIRS=

all	:=	TARGET= all
install :=	TARGET= install
clean	:=	TARGET= clean
clobber	:=	TARGET= clobber
lint	:=	TARGET= lint

all: $(CONF)

install:	$(ROOTLIBDIR) $(ROOTCONF)
	
$(CONF): $(INFOS)
	$(RM) $@
	$(CPP) piclfrutree.info > $@

# include library targets
include $(SRC)/lib/Makefile.targ
include	$(SRC)/cmd/picl/plugins/Makefile.targ

$(ROOTLINTDIR)/%: ../%
	$(INS.file)

lint:

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
