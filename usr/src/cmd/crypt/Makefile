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
#pragma ident	"@(#)Makefile	1.10	05/06/08 SMI"
#
# Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#

PROG= crypt

include ../Makefile.cmd

LDLIBS += -lcrypt

.KEEP_STATE:

all: $(PROG)

install: all $(ROOTPROG)

clean:

lint:	lint_PROG

include ../Makefile.targ

# EXPORT DELETE START
EXPORT_SRC:
	$(RM) $(PROG).c+ Makefile+
	sed -e "/EXPORT DELETE START/,/EXPORT DELETE END/d" \
		< $(PROG).c > $(PROG).c+
	$(MV) $(PROG).c+ $(PROG).c
	sed -e "/^# EXPORT DELETE START/,/^# EXPORT DELETE END/d" \
		< Makefile > Makefile+
	$(RM) Makefile
	$(MV) Makefile+ Makefile
	$(CHMOD) 444 Makefile $(PROG).c

# EXPORT DELETE END
