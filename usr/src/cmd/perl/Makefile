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
# Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
#ident	"@(#)Makefile	1.5	05/06/08 SMI"
#

include ../Makefile.cmd

all	:= TARGET = all
install	:= TARGET = install
clobber := TARGET = clobber
clean	:= TARGET = clean
test	:= TARGET = test

PERL_VERSIONS = \
    5.6.1 \
    5.8.4

.PARALLEL: $(PERL_VERSIONS)

#
# copy_contrib is used by all the versions of perl to copy the files under
# contrib into their contrib build directory.  We therefore need to make sure
# it is fetched from SCCS before calling the version-specific makefiles so that
# they don't race over it.
#
all install test: contrib/copy_contrib $(PERL_VERSIONS)

clean: $(PERL_VERSIONS)

clobber: $(PERL_VERSIONS)
	$(RM) contrib/copy_contrib

#
# Perl is not lint-clean.  Fake up a target.
#
lint:
	@ $(ECHO) "usr/src/cmd/perl is not lint-clean: skipping"
	@ $(TRUE)

$(PERL_VERSIONS): FRC
	@ cd $@; pwd; $(MAKE) $(TARGET)

FRC:
