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
# ident	"@(#)Makefile	1.4	05/06/08 SMI"
#

CLASS_ACTION_SCRIPTS = \
	i.kcfconf \
	r.kcfconf \
	i.pkcs11conf \
	r.pkcs11conf

MANIFEST =		cryptosvc.xml

include ../../Makefile.cmd

.KEEP_STATE:

INSTALL_DIR =		$(ROOT)/usr/sadm/install
CLASS_SCR_DIR =		$(INSTALL_DIR)/scripts
CLASS_SCR_FILES =	$(CLASS_ACTION_SCRIPTS:%=$(CLASS_SCR_DIR)/%)

ROOTMANIFESTDIR =	$(ROOTSVCSYSTEM)

$(ROOTMANIFEST) :=	FILEMODE = 0444

$(CLASS_SCR_DIR)/%: %
	$(INS.file)

install: $(CLASS_SCR_FILES) $(ROOTMANIFEST)

check: $(CHKMANIFEST)
