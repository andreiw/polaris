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
# Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# ident	"@(#)Makefile	1.7	05/08/10 SMI"
#

include ../Makefile.com

.KEEP_STATE:

all: $(FILES)
install: all $(MACH)_pkg

include ../Makefile.targ

sparc_pkg: FRC
	@if [ "$(SPARC_RM_PKGARCHIVE)" ]; then \
		[ -d $(SPARC_RM_PKGARCHIVE) ] || \
			mkdir -p $(SPARC_RM_PKGARCHIVE); \
		pkgmk -a sparc -f prototype_sparc_realmode \
			-d $(SPARC_RM_PKGARCHIVE) -r $(ROOT) -o $(PACKAGE); \
	fi

i386_pkg: pkg sparc_pkg
