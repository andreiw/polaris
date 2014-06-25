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
# Copyright 2006 Sun Microsystems Laboratories.  All rights reserved.
# Use is subject to license terms.
#
# ident	"%Z%%M%	%I%	%E% SMI"
#
# Copyright 2005 Cyril Plisko. All rights reserved.
# Use is subject to license terms.
#
#
#	This makefile drives the production of all implementation architecture
#	dependent modules for the chrp architecture.

UTSBASE = ..

include Makefile.chrp

#	The following are ppc specific (rather than chrp) specific modules
#	which are required for the chrp kernel to completely lint. They are
#	not involved in the build in any other way. In order to minimize
#	build time, it is assumed that they are up to date.
#
PPC_LIB_DIR	 = $(UTSBASE)/ppc/lint-libs/$(OBJS_DIR)

PPC_LINTS	 = genunix

LINT_LIBS	 = $(LINT_LIB) \
		   $(KMODS:%=$(LINT_LIB_DIR)/llib-l%.ln) \
		   $(PPC_LINTS:%=$(PPC_LIB_DIR)/llib-l%.ln)

# Several header files in usr/src/uts/ppc/sys are generated.
# They are derived from specifications in usr/src/uts/ppc/sysgen.
# Make sure the generated header files are up to date.
# It is expected that the specifications change VERY rarely,
# but try to get make to do the right thing, rather than resorting
# to one-shot manual labor.

PPC_SYSGEN	= $(UTSBASE)/ppc/sysgen


def		:=	TARGET= def
all		:=	TARGET= all
install		:=	TARGET= install
clean		:=	TARGET= clean
clobber		:=	TARGET= clobber
lint		:=	TARGET= lint
lintlib		:=	TARGET= lintlib
machmodlintlib	:=	TARGET= modlintlib
clean.lint	:=	TARGET= clean.lint
check		:=	TARGET= check

.KEEP_STATE:

.PARALLEL:	$(KMODS) $(XMODS)

def all clean clobber clean.lint: $(PPC_SYSGEN) genassym unix .WAIT $(KMODS) $(XMODS)

install: install_platforms genassym unix .WAIT $(KMODS) $(XMODS)

lintlib:	unix

modlintlib:	$(KMODS)

$(PPC_SYSGEN): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

genassym unix $(KMODS):	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

$(XMODS):	FRC
	@if [ -f $@/Makefile ]; then \
		cd $@; pwd; $(MAKE) $(TARGET); \
	else \
		true; \
	fi

install_h check:	FRC
	@cd sys; pwd; $(MAKE) $(TARGET)

#
# Definitions for the /platform directory aliases.
# Currently only one for chrp - bPlan,Pegasos2.
#
PLAT_LINKS	= bPlan,Pegasos2

#
# Make the /platform directories.  This is hardwired here because
# the first stage of the project (KBI) only implements the userland
# changes, but the only reasonable place to record the aliases is
# here in kernel land.
#
install_platforms:	$(ROOT_PSM_DIR) $(USR_PSM_DIR) \
			$(ROOT_PLAT_LINKS) $(USR_PLAT_LINKS) \

#
#	Full kernel lint target.
#
LINT_TARGET	= globallint

# workaround for multiply defined errors
globallint := LINTFLAGS += -erroff=E_NAME_MULTIPLY_DEF2

globallint:
	@-$(ECHO) "\nFULL KERNEL: global crosschecks:"
	@-$(LINT) $(LINTFLAGS) $(LINT_LIBS) 2>&1 | $(LGREP.2)

lint:	lintlib .WAIT modlintlib .WAIT $(PPC_LINTS) $(LINT_DEPS)

$(PPC_LINTS):	FRC
	@cd $(UTSBASE)/ppc/$@; pwd; $(MAKE) modlintlib

FRC:

include ../Makefile.targ

#
# Cross-reference customization: build a cross-reference over all of the
# chrp-related directories.
#
XRDIRS	= ../chrp ../ppc ../common
XRPRUNE	= sun4u sun4

cscope.out tags: FRC
	$(XREF) -x $@
