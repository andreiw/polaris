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
#ident	"@(#)Makefile	1.7	06/07/14 SMI"
#
# uts/intel/pfil/Makefile
#
#	This makefile drives the production of the pfil driver 
#	kernel module.
#
#	intel architecture dependent
#

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..

#
#	Define the module and object file sets.
#
MODULE		= pfil
OBJECTS		= $(PFIL_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(PFIL_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_DRV_DIR)/$(MODULE)
ROOTLINK	= $(ROOT_STRMOD_DIR)/$(MODULE)
CONF_SRCDIR	= $(UTSBASE)/common/inet/pfil

#
#	Include common rules.
#
include $(UTSBASE)/intel/Makefile.intel

#
#	Define targets
#
ALL_TARGET	= $(BINARY) $(SRC_CONFFILE)
LINT_TARGET	= $(MODULE).lint
INSTALL_TARGET	= $(BINARY) $(ROOTMODULE) $(ROOTLINK) $(ROOT_CONFFILE)

MINOR=       echo $(RELEASE) | cut -d. -f2
CPPFLAGS +=  -DSUNDDI -DSOLARIS2=$(MINOR:sh) -DIRE_ILL_CN -DSOLARIS -DUSE_INET6
LDFLAGS += -dy -Ndrv/ip 

#
#	Default build targets.
#
.KEEP_STATE:

def:		$(DEF_DEPS)

all:		$(ALL_DEPS)

clean:		$(CLEAN_DEPS)

clobber:	$(CLOBBER_DEPS)

lint:		$(LINT_DEPS)

modlintlib:	$(MODLINTLIB_DEPS)

clean.lint:	$(CLEAN_LINT_DEPS)

install:	$(INSTALL_DEPS)

$(ROOTLINK):	$(ROOT_STRMOD_DIR) $(ROOTMODULE)
	-$(RM) $@; ln $(ROOTMODULE) $@

#
#	Include common targets.
#
include $(UTSBASE)/intel/Makefile.targ
